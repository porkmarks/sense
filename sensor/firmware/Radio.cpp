#include "Radio.h"
#include "CRC.h"

#include "Arduino_Compat.h"
#include "Log.h"

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
    return uint8_t(sizeof(Radio::Header)) + payload_size;
}

constexpr chrono::millis k_ack_jitter(30);
constexpr chrono::millis k_minimum_duration_before_sending(200);

Radio::Radio()
#ifdef __AVR__
    : m_module(SS, PD3, PD4)
#else
    : m_module(0, 22, 23)
#endif
    , m_lora(&m_module)
{
}

bool Radio::init(uint8_t retries, int8_t power_dBm)
{
    if (retries == 0)
    {
        retries = 1;
    }

    uint8_t i = 0;
    for (i = 0; i < retries; i++)
    {
        //float freq = 434.0, float bw = 125.0, uint8_t sf = 9, uint8_t cr = 7, uint8_t syncWord = SX127X_SYNC_WORD, int8_t power = 17, uint8_t currentLimit = 100, uint16_t preambleLength = 8, uint8_t gain = 0);

        int state = m_lora.begin(868.f,   //FREQ
                                 250.f,   //BW
                                 8,       //SF
                                 8,       //CR
                                 SX127X_SYNC_WORD,
                                 power_dBm);     //Power
        if (state == ERR_NONE)
        {
            break;
        }
        LOG(PSTR("Try failed %d: %d\n"), int(i), state);
        chrono::delay(chrono::millis(500));
    }

    if (i >= retries)
    {
        return false;
    }

    //chrono::time_ms start = chrono::now();
    //send_ack(Header());
    chrono::millis d = m_lora.computeAirTimeUpperBound(ACK_BUFFER_SIZE);
    m_acks_send_duration = k_ack_jitter + d + k_ack_jitter;
    LOG(PSTR("ACK duration: %ldms\n"), m_acks_send_duration.count);

    sleep();

    return true;
}

void Radio::set_frequency(float freq)
{
    m_lora.setFrequency(freq);
    auto_sleep();
}

void Radio::set_transmission_power(int8_t power_dBm)
{
    m_lora.setOutputPower(power_dBm);
    auto_sleep();
}

void Radio::set_auto_sleep(bool enabled)
{
    m_auto_sleep = enabled;
}

void Radio::auto_sleep()
{
    if (m_auto_sleep)
    {
        sleep();
    }
}

void Radio::sleep()
{
    m_lora.sleep();
}

Radio::Address Radio::get_address() const
{
    return m_address;
}
void Radio::set_address(Address address)
{
    m_address = address;
}
void Radio::set_destination_address(Address address)
{
    m_destination_address = address;
}

uint8_t Radio::begin_packet(uint8_t* raw_buffer, uint8_t type, bool needs_response)
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

uint8_t Radio::pack(uint8_t* raw_buffer, const void* data, uint8_t size)
{
    if (m_offset + size > MAX_USER_DATA_SIZE)
    {
        return 0;
    }
    memcpy(raw_buffer + sizeof(Header) + m_offset, data, size);
    m_offset += size;

    return MAX_USER_DATA_SIZE - m_offset;
}

bool Radio::send_packed_packet(uint8_t* raw_buffer, bool wait_minimum_time)
{
    return send_packet(raw_buffer, m_offset, wait_minimum_time);
}

bool Radio::validate_packet(uint8_t* data, uint8_t size, uint8_t desired_payload_size)
{
    if (!data || size <= sizeof(Header))
    {
        LOG(PSTR("null/size\n"));
        return false;
    }

    Header* header_ptr = reinterpret_cast<Header*>(data);

    //insufficient data?
    if (size < sizeof(Header) + desired_payload_size)
    {
        LOG(PSTR("insuf data: %d<%d, type %d\n"), (int)size, (int)(sizeof(Header) + desired_payload_size), (int)header_ptr->type);
        return false;
    }

    uint32_t crc = header_ptr->crc;

    //zero the crc
    header_ptr->crc = 0;

    //corrupted?
    uint32_t computed_crc = crc32(data, size);
    if (crc != computed_crc)
    {
        LOG(PSTR("crc %lu/%lu, type %d\n"), crc, computed_crc, (int)header_ptr->type);
        return false;
    }

    //not addressed to me?
    if (header_ptr->destination_address != m_address && header_ptr->destination_address != BROADCAST_ADDRESS)
    {
        LOG(PSTR("notForMe %d != %d\n"), (int)header_ptr->destination_address, (int)m_address);
        return false;
    }

    //LOG(PSTR("received packet %d, size %d\n"), (int)header_ptr->type, (int)size);

    return true;
}

void Radio::wait_minimum_time() const
{
    chrono::time_ms now = chrono::now();
    chrono::time_ms last = m_last_send_time_point > m_last_receive_time_point ? m_last_send_time_point : m_last_receive_time_point;
    chrono::millis dt = (last + k_minimum_duration_before_sending) - now;
    if (dt.count > 0)
    {
        chrono::delay(dt);
    }
}

void Radio::send_ack(const Header& header)
{
    uint8_t ack_buffer[ACK_BUFFER_SIZE];// = { 0 };
    uint8_t* ptr = ack_buffer;

    Header* header_ptr = reinterpret_cast<Header*>(ptr);
    header_ptr->source_address = m_address;
    header_ptr->destination_address = header.source_address;
    header_ptr->type = static_cast<uint8_t>(data::sensor::Type::ACK);
    header_ptr->req_id = ++m_last_req_id;
    header_ptr->crc = 0;

    data::sensor::Ack* ack_ptr = reinterpret_cast<data::sensor::Ack*>(ptr + sizeof(Header));
    ack_ptr->ack = 1;
    ack_ptr->req_id = header.req_id;

    uint32_t crc = crc32(ptr, ACK_BUFFER_SIZE);
    header_ptr->crc = crc;

    m_lora.transmit(ack_buffer, ACK_BUFFER_SIZE);
}

bool Radio::send_packet(uint8_t* raw_buffer, uint8_t packet_size, bool _wait_minimum_time)
{
    //LOG(PSTR("SP\n"));
    Header* header_ptr = reinterpret_cast<Header*>(raw_buffer);
    header_ptr->crc = 0;

    uint32_t crc = crc32(raw_buffer, sizeof(Header) + packet_size);
    header_ptr->crc = crc;

    //LOG(PSTR("SP2\n"));
    if (_wait_minimum_time)
    {
        //wait some time between sends to make sure the receiver is done sending acks and listens again
        wait_minimum_time();
    }

    uint8_t ack_buffer[ACK_BUFFER_SIZE + 1] = { 0 };

    //LOG(PSTR("SP tr\n"));
    //m_rf22.send_raw(raw_buffer, sizeof(Header) + packet_size, 100);
    int16_t state = m_lora.transmit(raw_buffer, sizeof(Header) + packet_size);
    if (state == ERR_NONE)
    {
        chrono::time_ms start = chrono::now();
        chrono::millis d = chrono::now() - start;
        do
        {
            //LOG(PSTR("SP ra\n"));
            uint8_t size = ACK_BUFFER_SIZE;
            state = m_lora.receive(ack_buffer, size, m_acks_send_duration - d);
            if (state == ERR_NONE && size > 0)
            {
                auto_sleep();
                if (validate_packet(ack_buffer, size, sizeof(data::sensor::Ack)))
                {
                    //LOG(PSTR("SP ra done\n"));
                    data::sensor::Ack* ack_ptr = reinterpret_cast<data::sensor::Ack*>(ack_buffer + sizeof(Header));
                    if (ack_ptr->req_id == header_ptr->req_id && ack_ptr->ack != 0)
                    {
                        m_last_send_time_point = chrono::now();
                        return true;
                    }
                }
                else
                {
//                    LOG(PSTR("SP ra fail\n"));
                }
            }
            d = chrono::now() - start;
        } while (d < m_acks_send_duration);

        LOG(PSTR("noack\n"));
    }
    else
    {
        LOG(PSTR("txerr %d\n"), (int)state);
    }

    auto_sleep();
    m_last_send_time_point = chrono::now();
    return false;
}

uint8_t* Radio::receive_packet(uint8_t* raw_buffer, uint8_t& packet_size, chrono::millis timeout)
{
    auto start = chrono::now();
    do
    {
//        LOG(PSTR("RP re\n"));
        uint8_t size = sizeof(Header) + packet_size;
        if (m_lora.receive(raw_buffer, size, timeout) == ERR_NONE && size > sizeof(Header))
        {
            auto_sleep();
            chrono::time_ms receive_tp = chrono::now();
            Header* header_ptr = reinterpret_cast<Header*>(raw_buffer);
            if (validate_packet(raw_buffer, size, 0))
            {
//                LOG(PSTR("RP re done\n"));
                if (get_rx_packet_type(raw_buffer) != static_cast<uint8_t>(data::sensor::Type::ACK)) //ignore protocol packets
                {
                    chrono::delay(k_ack_jitter - (chrono::now() - receive_tp)); //to allow the sender to go in receive mode

//                    LOG(PSTR("RP se\n"));
                    send_ack(*header_ptr);
                    auto_sleep();
//                    LOG(PSTR("RP se done\n"));

                    m_last_receive_time_point = chrono::now();
                    packet_size = size - sizeof(Header);
                    return raw_buffer;
                }
            }
            else
            {
//                LOG(PSTR("RP re fail\n"));
            }
        }

    } while (chrono::now() - start < timeout);

    m_last_receive_time_point = chrono::now();
    auto_sleep();
    return nullptr;
}

bool Radio::start_async_receive()
{
    return m_lora.startReceiving() == ERR_NONE;
}

uint8_t* Radio::async_receive_packet(uint8_t* raw_buffer, uint8_t& packet_size)
{
    bool gotIt = false;
    uint8_t size = sizeof(Header) + packet_size;
    int8_t state = m_lora.getReceivedPackage(raw_buffer, size, gotIt);
    if (state != ERR_NONE)
    {
        LOG(PSTR("rxerr %d\n"), (int)state);
        return nullptr;
    }

    if (!gotIt)
    {
        return nullptr;
    }

    chrono::time_ms receive_tp = chrono::now();
    Header* header_ptr = reinterpret_cast<Header*>(raw_buffer);
    if (validate_packet(raw_buffer, size, 0))
    {
//                LOG(PSTR("RP re done\n"));
        if (get_rx_packet_type(raw_buffer) != static_cast<uint8_t>(data::sensor::Type::ACK)) //ignore protocol packets
        {
            chrono::delay(k_ack_jitter - (chrono::now() - receive_tp)); //to allow the sender to go in receive mode

//                    LOG(PSTR("RP se\n"));
            send_ack(*header_ptr);
//                    LOG(PSTR("RP se done\n"));

            m_last_receive_time_point = chrono::now();
            packet_size = size - sizeof(Header);
            stop_async_receive();
            return raw_buffer;
        }
    }
    else
    {
//                LOG(PSTR("RP re fail\n"));
    }

    return nullptr;
}

void Radio::stop_async_receive()
{
    m_lora.stopReceiving();
    auto_sleep();    
}

uint8_t Radio::get_rx_packet_type(uint8_t* received_buffer) const
{
    const Header* header_ptr = reinterpret_cast<const Header*>(received_buffer);
    return header_ptr->type;
}
bool Radio::get_rx_packet_needs_response(uint8_t* received_buffer) const
{
    const Header* header_ptr = reinterpret_cast<const Header*>(received_buffer);
    return header_ptr->needs_response ? true : false;
}
const void* Radio::get_rx_packet_payload(uint8_t* received_buffer) const
{
    return received_buffer + sizeof(Header);
}
void* Radio::get_tx_packet_payload(uint8_t* raw_buffer) const
{
    return raw_buffer + sizeof(Header);
}

Radio::Address Radio::get_rx_packet_source_address(uint8_t* received_buffer) const
{
    const Header* header_ptr = reinterpret_cast<const Header*>(received_buffer);
    return header_ptr->source_address;
}

int8_t Radio::get_input_dBm()
{
    int8_t rssi = m_lora.getRSSI();
    auto_sleep();
    return rssi;
}
