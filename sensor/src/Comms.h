#pragma once

#include "Data_Defs.h"

#include "rfm22b/rfm22b.h"

class Comms
{
public:
    Comms();

    static constexpr uint16_t BROADCAST_ADDRESS = 0;
    static constexpr uint16_t SERVER_ADDRESS = 0xFFFF;

    static constexpr uint16_t PAIR_ADDRESS_BEGIN = BROADCAST_ADDRESS + 1;
    static constexpr uint16_t PAIR_ADDRESS_END = PAIR_ADDRESS_BEGIN + 1000;

    static constexpr uint16_t SLAVE_ADDRESS_BEGIN = PAIR_ADDRESS_END + 1;
    static constexpr uint16_t SLAVE_ADDRESS_END = SLAVE_ADDRESS_BEGIN + 10000;

    bool init(uint8_t retries);

    void set_address(uint16_t address);

    void idle_mode();
    void stand_by_mode();

    uint8_t begin_pack(data::Type type);

    template<class T> uint8_t pack(const T& data);
    uint8_t pack(const void* data, uint8_t size);

    bool send_pack(uint8_t retries);

    template<class T>
    bool receive(data::Type type, T& payload, uint32_t timeout);

    bool receive(data::Type type, void* payload, uint8_t payload_size, uint32_t timeout);

private:
    RFM22B m_rf22;

    struct Header
    {
        uint16_t address = 0;
        data::Type type;
    } m_header;

    typedef uint32_t CRC;

    enum class Response : uint8_t
    {
        ACK,
        NACK
    };

    static const uint8_t RESPONSE_BUFFER_SIZE = sizeof(Header) + sizeof(Response) + sizeof(CRC);

    bool parse_data(uint8_t* data, size_t size, data::Type& type, void* payload, uint8_t payload_size);

    static const uint8_t MAX_USER_DATA_SIZE = RFM22B::MAX_PACKET_LENGTH - sizeof(Header) - sizeof(CRC);

    uint8_t m_buffer[RFM22B::MAX_PACKET_LENGTH];
    uint8_t m_offset = 0;
};


template<class T> uint8_t Comms::pack(const T& data)
{
    static_assert(sizeof(T) <= MAX_USER_DATA_SIZE, "");
    return pack(&data, sizeof(T));
}

template<class T>
bool Comms::receive(data::Type type, T& payload, uint32_t timeout)
{
    return receive(type, &payload, sizeof(T), timeout);
}
