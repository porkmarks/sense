#pragma once

#include "Data_Defs.h"

#include "rfm22b/rfm22b.h"

class Comms
{
public:
    Comms();

    static constexpr uint16_t BROADCAST_ADDRESS = 0;
    static constexpr uint16_t BASE_ADDRESS = 0xFFFF;

    static constexpr uint16_t PAIR_ADDRESS_BEGIN = BROADCAST_ADDRESS + 1;
    static constexpr uint16_t PAIR_ADDRESS_END = PAIR_ADDRESS_BEGIN + 1000;

    static constexpr uint16_t SLAVE_ADDRESS_BEGIN = PAIR_ADDRESS_END + 1;
    static constexpr uint16_t SLAVE_ADDRESS_END = SLAVE_ADDRESS_BEGIN + 10000;

    bool init(uint8_t retries);

    void set_address(uint16_t address);
    void set_destination_address(uint16_t address);

    void idle_mode();
    void stand_by_mode();

    uint8_t begin_packet(data::Type type);

    template<class T> uint8_t pack(const T& data);
    uint8_t pack(const void* data, uint8_t size);

    bool send_packet(uint8_t retries);

    uint8_t receive_packet(uint32_t timeout);
    uint16_t get_packet_source_address() const;
    int8_t get_input_dBm();
    data::Type get_packet_type() const;
    const void* get_packet_payload() const;

private:
    RFM22B m_rf22;

    uint16_t m_address = BROADCAST_ADDRESS;
    uint16_t m_destination_address = BROADCAST_ADDRESS;
    uint16_t m_last_req_id = 0;

#ifndef __AVR__
#   pragma pack(push, 1) // exact fit - no padding
#endif
    struct Header
    {
        uint32_t crc;
        uint16_t source_address = BROADCAST_ADDRESS;
        uint16_t destination_address = BROADCAST_ADDRESS;
        uint16_t req_id : 15;
        data::Type type;
    };

#ifndef __AVR__
#   pragma pack(pop)
#endif


    static const uint8_t RESPONSE_BUFFER_SIZE = sizeof(Header) + sizeof(data::Response);

    bool validate_packet(uint8_t* data, uint8_t size, uint8_t desired_payload_size);

    void send_response(const Header& header);

    static const uint8_t MAX_USER_DATA_SIZE = RFM22B::MAX_PACKET_LENGTH - sizeof(Header);

    uint8_t m_buffer[RFM22B::MAX_PACKET_LENGTH];
    uint8_t m_offset = 0;
};


template<class T> uint8_t Comms::pack(const T& data)
{
    static_assert(sizeof(T) <= MAX_USER_DATA_SIZE, "");
    return pack(&data, sizeof(T));
}
