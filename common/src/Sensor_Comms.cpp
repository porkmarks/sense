#include "Sensor_Comms.h"
#include "CRC.h"

#ifdef __AVR__

#   include <Arduino.h>

#else

#   include <thread>
#   include <string.h>
#   define delay(x) std::this_thread::sleep_for(std::chrono::milliseconds(x))
#   define printf_P printf
#   define PSTR

#endif

Sensor_Comms::Sensor_Comms()
{

}

bool Sensor_Comms::init(uint8_t retries, uint8_t power)
{
    if (retries == 0)
    {
        retries = 1;
    }

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

    m_rf22.set_gpio_function(RFM22B::GPIO::GPIO0, RFM22B::GPIO_Function::TX_STATE);
    m_rf22.set_gpio_function(RFM22B::GPIO::GPIO1, RFM22B::GPIO_Function::RX_STATE);

    uint8_t modem_config[42] =
    {
        0x15,
        0x40,
        0xC8,
        0x00,
        0xA3,
        0xD7,
        0x02,
        0x91,
        0x2B,
        0x28,
        0x7D,
        0x29,
        0xCD,
        0x00,
        0x02,
        0x08,
        0x22,
        0x2D,
        0xD4,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x01,
        0x00,
        0x00,
        0x00,
        0x00,
        0xFF,
        0xFF,
        0xFF,
        0xFF,
        0x51,
        0xEC,
        0x2C,
        0x23,
        0x08,
        0x53,
        0x75,
        0x80,
    };

    m_rf22.set_modem_configuration(modem_config);

//    m_rf22.set_carrier_frequency(434.7f);
//    m_rf22.set_frequency_deviation(5000);
//    m_rf22.set_data_rate(2400);
//    m_rf22.set_modulation_type(RFM22B::Modulation_Type::FSK);
//    m_rf22.set_modulation_data_source(RFM22B::Modulation_Data_Source::FIFO);
//    m_rf22.set_data_clock_configuration(RFM22B::Data_Clock_Configuration::NONE);
//    m_rf22.set_preamble_length(8);
//    m_rf22.set_sync_words({ 0x2d, 0xd4 }, 2);

    m_rf22.set_transmission_power(power);

    printf_P(PSTR("Frequency is %f\n"), m_rf22.get_carrier_frequency());
    printf_P(PSTR("FH Step is %lu\n"), m_rf22.get_frequency_hopping_step_size());
    printf_P(PSTR("Channel is %d\n"), (int)m_rf22.get_channel());
    printf_P(PSTR("Frequency deviation is %lu\n"), m_rf22.get_frequency_deviation());
    printf_P(PSTR("Data rate is %lu\n"), m_rf22.get_data_rate());
    printf_P(PSTR("Modulation Type %d\n"), (int)m_rf22.get_modulation_type());
    printf_P(PSTR("Modulation Data Source %d\n"), (int)m_rf22.get_modulation_data_source());
    printf_P(PSTR("Data Clock Configuration %d\n"), (int)m_rf22.get_data_clock_configuration());
    printf_P(PSTR("Transmission Power is %d\n"), (int)m_rf22.get_transmission_power());

//    Starting rfm22b setup...Frequency is 432.999850
//    FH Step is 0
//    Channel is 0
//    Frequency deviation is 20000
//    Data rate is 39993
//    Modulation Type 2
//    Modulation Data Source 2
//    Data Clock Configuration 0
//    Transmission Power is 1

    m_rf22.stand_by_mode();

    return true;
}

void Sensor_Comms::idle_mode()
{
    m_rf22.idle_mode();
}

void Sensor_Comms::stand_by_mode()
{
    m_rf22.stand_by_mode();
}

uint32_t Sensor_Comms::get_address() const
{
    return m_address;
}
void Sensor_Comms::set_address(uint32_t address)
{
    m_address = address;
}
void Sensor_Comms::set_destination_address(uint32_t address)
{
    m_destination_address = address;
}

uint8_t Sensor_Comms::get_payload_raw_buffer_size(uint8_t size) const
{
    return size + sizeof(Header) + 1;
}

uint8_t Sensor_Comms::begin_packet(uint8_t* raw_buffer, data::sensor::Type type)
{
    Header* header_ptr = reinterpret_cast<Header*>(raw_buffer + 1);
    header_ptr->type = type;
    header_ptr->source_address = m_address;
    header_ptr->destination_address = m_destination_address;
    header_ptr->req_id = ++m_last_req_id;
    header_ptr->crc = 0;

    m_offset = 0;
    return MAX_USER_DATA_SIZE;
}

uint8_t Sensor_Comms::pack(uint8_t* raw_buffer, const void* data, uint8_t size)
{
    if (m_offset + size > MAX_USER_DATA_SIZE)
    {
        return 0;
    }
    memcpy(raw_buffer + 1 + sizeof(Header) + m_offset, data, size);
    m_offset += size;

    return MAX_USER_DATA_SIZE - m_offset;
}

bool Sensor_Comms::send_packet(uint8_t* raw_buffer, uint8_t retries)
{
    return send_packet(raw_buffer, m_offset, retries);
}

bool Sensor_Comms::send_packet(uint8_t* raw_buffer, uint8_t packet_size, uint8_t retries)
{
    Header* header_ptr = reinterpret_cast<Header*>(raw_buffer + 1);
    header_ptr->crc = 0;

    uint32_t crc = crc32(raw_buffer + 1, sizeof(Header) + packet_size);
    header_ptr->crc = crc;

    if (retries == 0)
    {
        retries = 1;
    }

    for (uint8_t tx_r = 0; tx_r < retries; tx_r++)
    {
        m_rf22.send_raw(raw_buffer, sizeof(Header) + packet_size, 100);
        for (uint8_t rx_r = 0; rx_r < 3; rx_r++)
        {
            uint8_t response_buffer[RESPONSE_BUFFER_SIZE + 1] = { 0 };
            uint8_t size = RESPONSE_BUFFER_SIZE;
            uint8_t* data = m_rf22.receive_raw(response_buffer, size, 100);
            if (data)
            {
                if (validate_packet(data, size, sizeof(data::sensor::Response)))
                {
                    data::sensor::Response* response_ptr = reinterpret_cast<data::sensor::Response*>(data + sizeof(Header));
                    if (response_ptr->req_id == header_ptr->req_id && response_ptr->ack != 0)
                    {
                        return true;
                    }
                }
            }
        }
        delay(10 + (random() % 5) * 10);
    }

    return false;
}

bool Sensor_Comms::validate_packet(uint8_t* data, uint8_t size, uint8_t desired_payload_size)
{
    if (!data || size <= sizeof(Header))
    {
        printf_P(PSTR("null or wrong size\n"));
        return false;
    }

    Header* header_ptr = reinterpret_cast<Header*>(data);

    //insufficient data?
    if (size < sizeof(Header) + desired_payload_size)
    {
        printf_P(PSTR("insuficient data: %d < %d for type %d\n"), (int)size, (int)(sizeof(Header) + desired_payload_size), (int)header_ptr->type);
        return false;
    }

    uint32_t crc = header_ptr->crc;

    //zero the crc
    header_ptr->crc = 0;

    //corrupted?
    uint32_t computed_crc = crc32(data, size);
    if (crc != computed_crc)
    {
        printf_P(PSTR("bad crc %lu/%lu for type %d\n"), crc, computed_crc, (int)header_ptr->type);
        return false;
    }

    //not addressed to me?
    if (header_ptr->destination_address != m_address && header_ptr->destination_address != BROADCAST_ADDRESS)
    {
        printf_P(PSTR("not for me\n"));
        return false;
    }

    //printf_P(PSTR("received packet %d, size %d\n"), (int)header_ptr->type, (int)size);

    return true;
}

void Sensor_Comms::send_response(const Header& header)
{
    uint8_t response_buffer[RESPONSE_BUFFER_SIZE + 1] = { 0 };
    uint8_t* ptr = response_buffer + 1;

    Header* header_ptr = reinterpret_cast<Header*>(ptr);
    header_ptr->source_address = m_address;
    header_ptr->destination_address = header.source_address;
    header_ptr->type = data::sensor::Type::RESPONSE;
    header_ptr->req_id = ++m_last_req_id;
    header_ptr->crc = 0;

    data::sensor::Response* response_ptr = reinterpret_cast<data::sensor::Response*>(ptr + sizeof(Header));
    response_ptr->ack = 1;
    response_ptr->req_id = header.req_id;

    uint32_t crc = crc32(ptr, RESPONSE_BUFFER_SIZE);
    header_ptr->crc = crc;

    for (uint8_t i = 0; i < 3; i++)
    {
        m_rf22.send_raw(response_buffer, RESPONSE_BUFFER_SIZE);
        delay(2);
    }
}

uint8_t* Sensor_Comms::receive_packet(uint8_t* raw_buffer, uint8_t& packet_size, uint32_t timeout)
{
#ifdef __AVR__
    uint32_t start = millis();
    uint32_t elapsed = 0;
#else
    auto start = std::chrono::high_resolution_clock::now();
    uint32_t elapsed = 0;
#endif

    do
    {
        uint8_t size = sizeof(Header) + packet_size;
        uint8_t* data = m_rf22.receive_raw(raw_buffer, size, timeout - elapsed);
        if (data && size > sizeof(Header))
        {
            Header* header_ptr = reinterpret_cast<Header*>(data);
            if (validate_packet(data, size, 0))
            {
                if (get_rx_packet_type(data) != data::sensor::Type::RESPONSE) //ignore protocol packets
                {
                    send_response(*header_ptr);

                    packet_size = size - sizeof(Header);
                    return data;
                }
            }
        }

#ifdef __AVR__
        elapsed = millis() - start;
#else
        elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start).count();
#endif

    } while (elapsed < timeout);

    return nullptr;
}

data::sensor::Type Sensor_Comms::get_rx_packet_type(uint8_t* received_buffer) const
{
    const Header* header_ptr = reinterpret_cast<const Header*>(received_buffer);
    return header_ptr->type;
}
const void* Sensor_Comms::get_rx_packet_payload(uint8_t* received_buffer) const
{
    return received_buffer + sizeof(Header);
}
void* Sensor_Comms::get_tx_packet_payload(uint8_t* raw_buffer) const
{
    return raw_buffer + sizeof(Header) + 1;
}

uint32_t Sensor_Comms::get_rx_packet_source_address(uint8_t* received_buffer) const
{
    const Header* header_ptr = reinterpret_cast<const Header*>(received_buffer);
    return header_ptr->source_address;
}

int8_t Sensor_Comms::get_input_dBm()
{
    return m_rf22.get_input_dBm();
}

