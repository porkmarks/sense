#pragma once

#include "Data_Defs.h"

#include "rfm22b.h"

class Sensor_Comms
{
#ifndef __AVR__
#   pragma pack(push, 1) // exact fit - no padding
#endif
    struct Header
    {
        uint32_t crc;
        uint32_t source_address = BROADCAST_ADDRESS;
        uint32_t destination_address = BROADCAST_ADDRESS;
        uint16_t req_id : 15;
        uint8_t type = 0;
    };

#ifndef __AVR__
#   pragma pack(pop)
#endif

public:
    Sensor_Comms();

    static constexpr uint32_t BROADCAST_ADDRESS = 0;
    static constexpr uint32_t BASE_ADDRESS = 0xFFFFFFFFULL;

    static constexpr uint32_t PAIR_ADDRESS_BEGIN = BROADCAST_ADDRESS + 1;
    static constexpr uint32_t PAIR_ADDRESS_END = PAIR_ADDRESS_BEGIN + 1000;

    static constexpr uint32_t SLAVE_ADDRESS_BEGIN = PAIR_ADDRESS_END + 1;
    static constexpr uint32_t SLAVE_ADDRESS_END = BASE_ADDRESS - 1;

    bool init(uint8_t retries, uint8_t power);

    void set_transmission_power(uint8_t power);

    uint32_t get_address() const;
    void set_address(uint32_t address);
    void set_destination_address(uint32_t address);

    void idle_mode();
    void stand_by_mode();

    uint8_t get_payload_raw_buffer_size(uint8_t size) const;
    void* get_tx_packet_payload(uint8_t* raw_buffer) const;

    uint8_t begin_packet(uint8_t* raw_buffer, uint8_t type);

    template<class T> uint8_t pack(uint8_t* raw_buffer, const T& data);
    uint8_t pack(uint8_t* raw_buffer, const void* data, uint8_t size);

    bool send_packet(uint8_t* raw_buffer, uint8_t retries);
    bool send_packet(uint8_t* raw_buffer, uint8_t packet_size, uint8_t retries);

    uint8_t* receive_packet(uint8_t* raw_buffer, uint8_t& packet_size, uint32_t timeout);

    uint32_t get_rx_packet_source_address(uint8_t* received_buffer) const;
    int8_t get_input_dBm();
    uint8_t get_rx_packet_type(uint8_t* received_buffer) const;
    const void* get_rx_packet_payload(uint8_t* received_buffer) const;

    static const uint8_t MAX_USER_DATA_SIZE = RFM22B::MAX_DATAGRAM_LENGTH - sizeof(Header);

private:
    RFM22B m_rf22;

    uint32_t m_address = BROADCAST_ADDRESS;
    uint32_t m_destination_address = BROADCAST_ADDRESS;
    uint16_t m_last_req_id = 0;

    static const uint8_t RESPONSE_BUFFER_SIZE = sizeof(Header) + sizeof(data::sensor::Response);

    bool validate_packet(uint8_t* data, uint8_t size, uint8_t desired_payload_size);

    void send_response(const Header& header);


    uint8_t m_offset = 0;
};


template<class T> uint8_t Sensor_Comms::pack(uint8_t* dst, const T& data)
{
    static_assert(sizeof(T) <= MAX_USER_DATA_SIZE, "");
    return pack(dst, &data, sizeof(T));
}
