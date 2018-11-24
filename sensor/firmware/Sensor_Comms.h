#pragma once

#include "Data_Defs.h"
#include "Module.h"
#include "RFM95.h"

class Sensor_Comms
{
public:
#ifndef __AVR__
#   pragma pack(push, 1) // exact fit - no padding
#endif
    struct Header
    {
        uint32_t crc;
        uint32_t source_address = BROADCAST_ADDRESS;
        uint32_t destination_address = BROADCAST_ADDRESS;
        uint16_t req_id : 15;
        uint16_t needs_response : 1;
        uint8_t type = 0;
    };

#ifndef __AVR__
#   pragma pack(pop)
#endif

    Sensor_Comms();

    static constexpr uint32_t BROADCAST_ADDRESS = 0;
    static constexpr uint32_t BASE_ADDRESS = 0xFFFFFFFFULL;

    static constexpr uint32_t PAIR_ADDRESS_BEGIN = BROADCAST_ADDRESS + 1;
    static constexpr uint32_t PAIR_ADDRESS_END = PAIR_ADDRESS_BEGIN + 1000;

    static constexpr uint32_t SLAVE_ADDRESS_BEGIN = PAIR_ADDRESS_END + 1;
    static constexpr uint32_t SLAVE_ADDRESS_END = BASE_ADDRESS - 1;

    static constexpr uint8_t MAX_USER_DATA_SIZE = 128 - sizeof(Sensor_Comms::Header);


    bool init(uint8_t retries, int8_t power_dBm);

    void set_transmission_power(int8_t power_dBm);

    uint32_t get_address() const;
    void set_address(uint32_t address);
    void set_destination_address(uint32_t address);

    void sleep_mode();

    void* get_tx_packet_payload(uint8_t* raw_buffer) const;

    uint8_t begin_packet(uint8_t* raw_buffer, uint8_t type, bool needs_response);

    uint8_t pack(uint8_t* raw_buffer, const void* data, uint8_t size);

    bool send_packed_packet(uint8_t* raw_buffer, bool wait_minimum_time);
    bool send_packet(uint8_t* raw_buffer, uint8_t packet_size, bool wait_minimum_time);

    uint8_t* receive_packet(uint8_t* raw_buffer, uint8_t& packet_size, chrono::millis timeout);

    int8_t get_input_dBm();
    uint32_t get_rx_packet_source_address(uint8_t* received_buffer) const;
    uint8_t get_rx_packet_type(uint8_t* received_buffer) const;
    bool get_rx_packet_needs_response(uint8_t* received_buffer) const;
    const void* get_rx_packet_payload(uint8_t* received_buffer) const;

private:
    Module m_module;
    RFM95  m_lora;

    uint32_t m_address = BROADCAST_ADDRESS;
    uint32_t m_destination_address = BROADCAST_ADDRESS;
    uint16_t m_last_req_id = 0;

    void wait_minimum_time() const;
    chrono::millis m_acks_send_duration = chrono::millis(100);
    chrono::time_ms m_last_send_time_point;
    chrono::time_ms m_last_receive_time_point;

    static const uint8_t ACK_BUFFER_SIZE = sizeof(Header) + sizeof(data::sensor::Ack);

    bool validate_packet(uint8_t* data, uint8_t size, uint8_t desired_payload_size);

    void send_ack(const Header& header);

    uint8_t m_offset = 0;
};

uint8_t packet_raw_size(uint8_t payload_size);


