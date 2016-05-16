#include "Comms.h"
#include "CRC.h"

#ifdef RASPBERRY_PI
#   include <thread>
#   include <string.h>
#   define delay(x) std::this_thread::sleep_for(std::chrono::milliseconds(x))
#else
#   include <Arduino.h>
#endif

Comms::Comms()
{

}

bool Comms::init(uint8_t retries)
{
    retries += 1;

    uint8_t i = 0;
    for (i = 0; i < retries; i++)
    {
        if (m_rf22.init())
        {
            break;
        }
        delay(500);
    }
    if (i >= retries)
    {
        return false;
    }

    m_rf22.set_carrier_frequency(434.f);
    m_rf22.set_modulation_type(RFM22B::Modulation_Type::GFSK);
    m_rf22.set_modulation_data_source(RFM22B::Modulation_Data_Source::FIFO);
    m_rf22.set_data_clock_configuration(RFM22B::Data_Clock_Configuration::NONE);
    m_rf22.set_transmission_power(20);
    m_rf22.set_gpio_function(RFM22B::GPIO::GPIO0, RFM22B::GPIO_Function::TX_STATE);
    m_rf22.set_gpio_function(RFM22B::GPIO::GPIO1, RFM22B::GPIO_Function::RX_STATE);

    printf(("Frequency is %f\n"), m_rf22.get_carrier_frequency());
    printf(("FH Step is %lu\n"), m_rf22.get_frequency_hopping_step_size());
    printf(("Channel is %d\n"), (int)m_rf22.get_channel());
    printf(("Frequency deviation is %lu\n"), m_rf22.get_frequency_deviation());
    printf(("Data rate is %lu\n"), m_rf22.get_data_rate());
    printf(("Modulation Type %d\n"), (int)m_rf22.get_modulation_type());
    printf(("Modulation Data Source %d\n"), (int)m_rf22.get_modulation_data_source());
    printf(("Data Clock Configuration %d\n"), (int)m_rf22.get_data_clock_configuration());
    printf(("Transmission Power is %d\n"), (int)m_rf22.get_transmission_power());

    m_rf22.stand_by_mode();

    return true;
}

void Comms::idle_mode()
{
    m_rf22.idle_mode();
}

void Comms::stand_by_mode()
{
    m_rf22.stand_by_mode();
}

void Comms::set_address(uint16_t address)
{
    m_address = address;
}
void Comms::set_destination_address(uint16_t address)
{
    m_destination_address = address;
}

uint8_t Comms::begin_packet(data::Type type)
{
    Header* header_ptr = reinterpret_cast<Header*>(m_buffer);
    header_ptr->type = type;
    header_ptr->source_address = m_address;
    header_ptr->destination_address = m_destination_address;
    header_ptr->req_id = ++m_last_req_id;
    header_ptr->crc = 0;

    m_offset = 0;
    return MAX_USER_DATA_SIZE;
}

uint8_t Comms::pack(const void* data, uint8_t size)
{
    if (m_offset + size > MAX_USER_DATA_SIZE)
    {
        return 0;
    }
    memcpy(m_buffer + sizeof(Header) + m_offset, data, size);
    m_offset += size;

    return MAX_USER_DATA_SIZE - m_offset;
}

bool Comms::send_packet(uint8_t retries)
{
    uint32_t crc = crc32(m_buffer, sizeof(Header) + m_offset);

    Header* header_ptr = reinterpret_cast<Header*>(m_buffer);
    header_ptr->crc = crc;

    uint8_t response_buffer[RESPONSE_BUFFER_SIZE];

    retries++;
    uint8_t i = 0;
    for (i = 0; i < retries; i++)
    {
        m_rf22.send(m_buffer, sizeof(Header) + m_offset);
        int8_t size = m_rf22.receive(response_buffer, sizeof(response_buffer), 100);
        if (size > 0)
        {
            if (validate_packet(response_buffer, size, sizeof(data::Response)))
            {
                data::Response* response_ptr = reinterpret_cast<data::Response*>(response_buffer + sizeof(Header));
                if (response_ptr->req_id == header_ptr->req_id && response_ptr->ack != 0)
                {
                    m_offset = 0;
                    return true;
                }
            }
        }
        delay(100 + (random() % 5) * 100);
    }

    return false;
}

bool Comms::validate_packet(uint8_t* data, uint8_t size, uint8_t desired_payload_size)
{
    if (!data || size <= sizeof(Header))
    {
        return false;
    }

    //insufficient data?
    if (desired_payload_size != 0 && size != sizeof(Header) + desired_payload_size)
    {
        return false;
    }

    Header* header_ptr = reinterpret_cast<Header*>(data);
    uint32_t crc = header_ptr->crc;

    //zero the crc
    header_ptr->crc = 0;

    //corrupted?
    uint32_t computed_crc = crc32(reinterpret_cast<const uint8_t*>(data), size);
    if (crc != computed_crc)
    {
        return false;
    }

    //not addressed to me?
    if (header_ptr->destination_address != m_address && header_ptr->destination_address != BROADCAST_ADDRESS)
    {
        return false;
    }

    return true;
}

void Comms::send_response(const Header& header)
{
    uint8_t response_buffer[RESPONSE_BUFFER_SIZE];
    Header* header_ptr = reinterpret_cast<Header*>(response_buffer);
    header_ptr->source_address = m_address;
    header_ptr->destination_address = header.source_address;
    header_ptr->type = data::Type::RESPONSE;
    header_ptr->req_id = ++m_last_req_id;
    header_ptr->crc = 0;

    data::Response* response_ptr = reinterpret_cast<data::Response*>(response_buffer + sizeof(Header));
    response_ptr->ack = 1;
    response_ptr->req_id = header.req_id;

    uint32_t crc = crc32(response_buffer, RESPONSE_BUFFER_SIZE);
    header_ptr->crc = crc;

    for (uint8_t i = 0; i < 3; i++)
    {
        m_rf22.send(response_buffer, RESPONSE_BUFFER_SIZE);
        delay(10);
    }
}

uint8_t Comms::receive_packet(uint32_t timeout)
{
    int8_t size = m_rf22.receive(m_buffer, RFM22B::MAX_PACKET_LENGTH, timeout);
    if (size > 0 && static_cast<uint8_t>(size) > sizeof(Header))
    {
        if (validate_packet(m_buffer, size, 0))
        {
            Header* header_ptr = reinterpret_cast<Header*>(m_buffer);
            send_response(*header_ptr);

            return size - sizeof(Header);
        }
    }
    return 0;
}

data::Type Comms::get_packet_type() const
{
    const Header* header_ptr = reinterpret_cast<const Header*>(m_buffer);
    return header_ptr->type;
}
const void* Comms::get_packet_payload() const
{
    return m_buffer + sizeof(Header);
}

uint16_t Comms::get_packet_source_address() const
{
    const Header* header_ptr = reinterpret_cast<const Header*>(m_buffer);
    return header_ptr->source_address;
}
