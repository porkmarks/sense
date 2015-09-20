#include "Comms.h"
#include "CRC.h"
#include <Arduino.h>


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

    Serial.print(F("Frequency is ")); Serial.println(m_rf22.get_carrier_frequency());
    Serial.print(F("FH Step is ")); Serial.println(m_rf22.get_frequency_hopping_step_size());
    Serial.print(F("Channel is ")); Serial.println(m_rf22.get_channel());
    Serial.print(F("Frequency deviation is ")); Serial.println(m_rf22.get_frequency_deviation());
    Serial.print(F("Data rate is ")); Serial.println(m_rf22.get_data_rate());
    Serial.print(F("Modulation Type ")); Serial.println((int)m_rf22.get_modulation_type());
    Serial.print(F("Modulation Data Source ")); Serial.println((int)m_rf22.get_modulation_data_source());
    Serial.print(F("Data Clock Configuration ")); Serial.println((int)m_rf22.get_data_clock_configuration());
    Serial.print(F("Transmission Power is ")); Serial.println(m_rf22.get_transmission_power());

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
    m_header.address = address;
}

uint8_t Comms::begin_pack(data::Type type)
{
    m_header.type = type;
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

bool Comms::send_pack(uint8_t retries)
{
    if (m_offset == 0)
    {
        return false;
    }

    memcpy(m_buffer, &m_header, sizeof(Header));
    pack(crc32(m_buffer, m_offset));

    uint8_t response_buffer[RESPONSE_BUFFER_SIZE];

    retries++;
    uint8_t i = 0;
    for (i = 0; i < retries; i++)
    {
        m_rf22.send(m_buffer, m_offset);
        size_t size = m_rf22.receive(response_buffer, sizeof(response_buffer), 100);
        if (size > 0)
        {
            Response response;
            data::Type type;
            if (parse_data(response_buffer, size, type, &response, sizeof(Response)))
            {
                if (response == Response::ACK)
                {
                    m_offset = 0;
                    return true;
                }
                else if (response == Response::NACK)
                {
                    return false;
                }
            }
        }
        delay(100 + (random() % 5) * 100);
    }

    return false;
}

bool Comms::parse_data(uint8_t* data, size_t size, data::Type& type, void* payload, uint8_t payload_size)
{
    if (!data || size != sizeof(Header) + payload_size + sizeof(CRC))
    {
        return false;
    }

    uint32_t* crc_ptr = reinterpret_cast<uint32_t*>(data + sizeof(Header) + payload_size);
    uint32_t crc = *crc_ptr;

    //zero the crc
    *crc_ptr = 0;

    //corrupted?
    uint32_t computed_crc = crc32(reinterpret_cast<const uint8_t*>(data), size);
    if (crc != computed_crc)
    {
        return false;
    }

    //not addressed to me?
    Header* header_ptr = reinterpret_cast<Header*>(data);
    if (header_ptr->address != m_header.address)
    {
        return false;
    }
    type = header_ptr->type;

    //seems ok
    memcpy(payload, reinterpret_cast<uint8_t*>(data + sizeof(Header)), payload_size);
    return true;
}

bool Comms::receive(data::Type type, void* payload, uint8_t payload_size, uint32_t timeout)
{
    size_t size = m_rf22.receive(m_buffer, payload_size + sizeof(Header) + sizeof(CRC), timeout);
    if (size > 0)
    {
        data::Type _type;
        if (parse_data(m_buffer, size, _type, payload, payload_size) && _type == type)
        {
            return true;
        }
    }

    return false;
}

