#pragma once

#include "Data_Defs.h"
#include "Module.h"
#include "RFM95.h"

class Radio
{
public:
    typedef uint16_t Address;

#ifndef __AVR__
#   pragma pack(push, 1) // exact fit - no padding
#endif
    struct Header
    {
        static const uint8_t k_internal_version = 1;
        
        static const uint32_t k_max_internal_version = uint32_t(1) << 3;
        uint32_t internal_version : 3;

		    static const uint32_t k_max_user_version = uint32_t(1) << 4;
		    uint32_t user_version : 4;

        static const uint32_t k_max_user_type = uint32_t(1) << 5;
        uint32_t user_type : 5;
        
        static const uint32_t k_max_req_id = uint32_t(1) << 16;
        uint32_t req_id : 16;
        
        uint32_t needs_response : 1;
        
        static const uint32_t k_max_address = uint32_t(1) << 14;
        uint32_t source_address : 14;
        uint32_t destination_address : 14;
        uint32_t crc;
    };

#ifndef __AVR__
#   pragma pack(pop)
#endif

    static_assert(sizeof(Header) == 12, "Invalid header size");

    Radio();

    static constexpr Address BROADCAST_ADDRESS = 0;
    static constexpr Address BASE_ADDRESS = Header::k_max_address - 1;

    static constexpr Address PAIR_ADDRESS_BEGIN = BROADCAST_ADDRESS + 1;
    static constexpr Address PAIR_ADDRESS_END = PAIR_ADDRESS_BEGIN + 1000;

    static constexpr Address SLAVE_ADDRESS_BEGIN = PAIR_ADDRESS_END + 1;
    static constexpr Address SLAVE_ADDRESS_END = BASE_ADDRESS - 1;

    static constexpr uint8_t MAX_USER_DATA_SIZE = 128 - sizeof(Radio::Header);


    bool init(uint8_t retries, int8_t power_dBm);

    void set_frequency(float freq);
    void set_transmission_power(int8_t power_dBm);

    Address get_address() const;
    void set_address(Address address);
    void set_destination_address(Address address);

    void set_auto_sleep(bool enabled);
    void sleep();

    void* get_tx_packet_payload(uint8_t* raw_buffer) const;

    uint8_t begin_packet(uint8_t* raw_buffer, uint8_t user_version, uint8_t user_type, bool needs_response);

    uint8_t pack(uint8_t* raw_buffer, const void* data, uint8_t size);

    bool send_packed_packet(uint8_t* raw_buffer, bool wait_minimum_time);
    bool send_packet(uint8_t* raw_buffer, uint8_t packet_size, bool wait_minimum_time);

    uint8_t* receive_packet(uint8_t* raw_buffer, uint8_t& packet_size, chrono::millis timeout);

    bool start_async_receive();
    uint8_t* async_receive_packet(uint8_t* raw_buffer, uint8_t& packet_size);
    void stop_async_receive();

    int16_t get_input_dBm();
    int16_t get_last_input_dBm();
    Address get_rx_packet_source_address(uint8_t* received_buffer) const;
    uint8_t get_rx_packet_user_version(uint8_t* received_buffer) const;
    uint8_t get_rx_packet_user_type(uint8_t* received_buffer) const;
    bool get_rx_packet_needs_response(uint8_t* received_buffer) const;
    const void* get_rx_packet_payload(uint8_t* received_buffer) const;

private:
    Module m_module;
    RFM95  m_lora;

    Address m_address = BROADCAST_ADDRESS;
    Address m_destination_address = BROADCAST_ADDRESS;
    uint16_t m_last_req_id = 0;

    void wait_minimum_time() const;
    chrono::time_ms m_last_send_time_point;
    chrono::time_ms m_last_receive_time_point;

    bool validate_packet(uint8_t* data, uint8_t size, uint8_t desired_payload_size);

    void auto_sleep();

    uint8_t m_offset = 0;
    bool m_auto_sleep = false;
    int16_t m_last_rssi = 0;
};

uint8_t packet_raw_size(uint8_t payload_size);
