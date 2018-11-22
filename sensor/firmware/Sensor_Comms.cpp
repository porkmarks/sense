#include "Sensor_Comms.h"
#include "CRC.h"

#include "Arduino_Compat.h"

#ifdef __AVR__
#   include <stdio.h>
#else
#   include <chrono>
#   include <iostream>
#   include <thread>
#   include <cstring>
#   include <cmath>
#endif

uint8_t packet_raw_size(uint8_t payload_size)
{
    return uint8_t(sizeof(Sensor_Comms::Header)) + payload_size;
}


Sensor_Comms::Sensor_Comms()
#ifdef __AVR__
    : m_module(SS, PD3, PD4)
#else
    : m_module(0, 22, 23)
#endif
    , m_lora(&m_module)
{
}

bool Sensor_Comms::init(uint8_t retries, int8_t power_dBm)
{
    if (retries == 0)
    {
        retries = 1;
    }

    uint8_t i = 0;
    for (i = 0; i < retries; i++)
    {
        //float freq = 434.0, float bw = 125.0, uint8_t sf = 9, uint8_t cr = 7, uint8_t syncWord = SX127X_SYNC_WORD, int8_t power = 17, uint8_t currentLimit = 100, uint16_t preambleLength = 8, uint8_t gain = 0);

        int state = m_lora.begin(868.f,  //FREQ
                                 250.f,  //BW
                                 9,      //SF
                                 7,
                                 SX127X_SYNC_WORD,
                                 power_dBm);     //Power
        if (state == ERR_NONE)
        {
            break;
        }
        printf_P(PSTR("Try failed %d: %d\n"), int(i), state);
        chrono::delay(chrono::millis(500));
    }

    if (i >= retries)
    {
        return false;
    }

    //    m_rf22.set_transmission_power(power);

    /*    printf_P(PSTR("Frequency is %luKhz\n"), (int32_t)(m_rf22.get_carrier_frequency()*1000.f));
    printf_P(PSTR("FH Step is %lu\n"), m_rf22.get_frequency_hopping_step_size());
    printf_P(PSTR("Channel is %d\n"), (int)m_rf22.get_channel());
    printf_P(PSTR("Frequency deviation is %lu\n"), m_rf22.get_frequency_deviation());
    printf_P(PSTR("Data rate is %lu\n"), m_rf22.get_data_rate());
    printf_P(PSTR("Modulation Type %d\n"), (int)m_rf22.get_modulation_type());
    printf_P(PSTR("Modulation Data Source %d\n"), (int)m_rf22.get_modulation_data_source());
    printf_P(PSTR("Data Clock Configuration %d\n"), (int)m_rf22.get_data_clock_configuration());
    printf_P(PSTR("Transmission Power is %d\n"), (int)m_rf22.get_transmission_power());
*/
    sleep_mode();

    return true;
}

void Sensor_Comms::set_transmission_power(int8_t power_dBm)
{
    m_lora.setOutputPower(power_dBm);
}

void Sensor_Comms::sleep_mode()
{
    m_lora.sleep();
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

uint8_t Sensor_Comms::begin_packet(uint8_t* raw_buffer, uint8_t type, bool needs_response)
{
    Header* header_ptr = reinterpret_cast<Header*>(raw_buffer);
    header_ptr->type = type;
    header_ptr->source_address = m_address;
    header_ptr->destination_address = m_destination_address;
    header_ptr->req_id = ++m_last_req_id;
    header_ptr->needs_response = needs_response ? 1 : 0;
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
    memcpy(raw_buffer + sizeof(Header) + m_offset, data, size);
    m_offset += size;

    return MAX_USER_DATA_SIZE - m_offset;
}

bool Sensor_Comms::send_packet(uint8_t* raw_buffer, uint8_t retries)
{
    return send_packet(raw_buffer, m_offset, retries);
}

bool Sensor_Comms::send_packet(uint8_t* raw_buffer, uint8_t packet_size, uint8_t retries)
{
    Header* header_ptr = reinterpret_cast<Header*>(raw_buffer);
    header_ptr->crc = 0;

    uint32_t crc = crc32(raw_buffer, sizeof(Header) + packet_size);
    header_ptr->crc = crc;

    if (retries == 0)
    {
        retries = 1;
    }

    for (uint8_t tx_r = 0; tx_r < retries; tx_r++)
    {
        //m_rf22.send_raw(raw_buffer, sizeof(Header) + packet_size, 100);
        if (m_lora.transmit(raw_buffer, sizeof(Header) + packet_size) == ERR_NONE)
        {
            chrono::time_ms start = chrono::now();
            while (chrono::now() - start < chrono::millis(50));
            {
                uint8_t response_buffer[RESPONSE_BUFFER_SIZE + 1] = { 0 };
                size_t size = RESPONSE_BUFFER_SIZE;
                if (m_lora.receive(response_buffer, size) == ERR_NONE && size > 0)
                {
                    if (validate_packet(response_buffer, size, sizeof(data::sensor::Response)))
                    {
                        data::sensor::Response* response_ptr = reinterpret_cast<data::sensor::Response*>(response_buffer + sizeof(Header));
                        if (response_ptr->req_id == header_ptr->req_id && response_ptr->ack != 0)
                        {
                            return true;
                        }
                    }
                }
            }
        }
        chrono::delay(chrono::millis(10 + (random() % 5) * 10));
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
    uint8_t response_buffer[RESPONSE_BUFFER_SIZE] = { 0 };
    uint8_t* ptr = response_buffer;

    Header* header_ptr = reinterpret_cast<Header*>(ptr);
    header_ptr->source_address = m_address;
    header_ptr->destination_address = header.source_address;
    header_ptr->type = static_cast<uint8_t>(data::sensor::Type::RESPONSE);
    header_ptr->req_id = ++m_last_req_id;
    header_ptr->crc = 0;

    data::sensor::Response* response_ptr = reinterpret_cast<data::sensor::Response*>(ptr + sizeof(Header));
    response_ptr->ack = 1;
    response_ptr->req_id = header.req_id;

    uint32_t crc = crc32(ptr, RESPONSE_BUFFER_SIZE);
    header_ptr->crc = crc;

    for (uint8_t i = 0; i < 2; i++)
    {
        m_lora.transmit(response_buffer, RESPONSE_BUFFER_SIZE);
        chrono::delay(chrono::millis(2));
    }
}

uint8_t* Sensor_Comms::receive_packet(uint8_t* raw_buffer, uint8_t& packet_size, uint32_t timeout)
{
    auto start = chrono::now();
    uint32_t elapsed = 0;

    do
    {
        size_t size = sizeof(Header) + packet_size;
        if (m_lora.receive(raw_buffer, size) == ERR_NONE && size > sizeof(Header))
        {
            Header* header_ptr = reinterpret_cast<Header*>(raw_buffer);
            if (validate_packet(raw_buffer, size, 0))
            {
                if (get_rx_packet_type(raw_buffer) != static_cast<uint8_t>(data::sensor::Type::RESPONSE)) //ignore protocol packets
                {
                    chrono::delay(chrono::micros(5)); //to allow the sender to go in receive mode
                    send_response(*header_ptr);

                    packet_size = size - sizeof(Header);
                    return raw_buffer;
                }
            }
        }

        elapsed = (chrono::now() - start).count;
    } while (elapsed < timeout);

    return nullptr;
}

uint8_t Sensor_Comms::get_rx_packet_type(uint8_t* received_buffer) const
{
    const Header* header_ptr = reinterpret_cast<const Header*>(received_buffer);
    return header_ptr->type;
}
bool Sensor_Comms::get_rx_packet_needs_response(uint8_t* received_buffer) const
{
    const Header* header_ptr = reinterpret_cast<const Header*>(received_buffer);
    return header_ptr->needs_response ? true : false;
}
const void* Sensor_Comms::get_rx_packet_payload(uint8_t* received_buffer) const
{
    return received_buffer + sizeof(Header);
}
void* Sensor_Comms::get_tx_packet_payload(uint8_t* raw_buffer) const
{
    return raw_buffer + sizeof(Header);
}

uint32_t Sensor_Comms::get_rx_packet_source_address(uint8_t* received_buffer) const
{
    const Header* header_ptr = reinterpret_cast<const Header*>(received_buffer);
    return header_ptr->source_address;
}

int8_t Sensor_Comms::get_input_dBm()
{
    return m_lora.getRSSI();
}

