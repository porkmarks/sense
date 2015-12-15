#pragma once

#include <stdint.h>
#include <math.h>

namespace data
{

enum class Type : uint8_t
{
    RESPONSE,
    MEASUREMENT,
    PAIR_REQUEST,
    PAIR_RESPONSE,
    CONFIG_REQUEST,
    CONFIG,
};


#ifndef __AVR__
#   pragma pack(push, 1) // exact fit - no padding
#endif

struct Measurement
{
    enum class Flag : uint8_t
    {
        SENSOR_ERROR   = 1 << 0,
        COMMS_ERROR    = 1 << 1
    };

    void pack(float vcc, float humidity, float temperature)
    {
        vcc -= 2.f;
        this->vcc = static_cast<uint8_t>(fmin(fmax(vcc, 0.f), 2.55f) * 100.f);
        this->humidity = static_cast<uint8_t>(humidity * 2.55f);
        this->temperature = static_cast<uint16_t>(temperature * 100.f);
    }

    void unpack(float& vcc, float& humidity, float& temperature) const
    {
        vcc = static_cast<float>(this->vcc) / 100.f + 2.f;
        humidity = static_cast<float>(this->humidity) / 2.55f;
        temperature = static_cast<float>(this->temperature) / 100.f;
    }

//packed data
    uint32_t index = 0;
    uint32_t time_point = 0; //seconds, since 1970 epoch
    uint8_t flags = 0;
    uint8_t vcc = 0; //(vcc - 2) * 100
    uint8_t humidity = 0; //*2.55
    int16_t temperature = 0; //*100
};

struct Config_Request
{
};
struct Config
{
    uint32_t server_time_point = 0;
    uint32_t scheduled_time_point = 0;
};

struct Response
{
    uint16_t ack : 1;
    uint16_t req_id : 15;
};

struct Pair_Request
{
};
struct Pair_Response
{
    uint32_t server_time_point = 0;
    uint16_t address = 0;
};

#ifndef __AVR__
#   pragma pack(pop)
#endif

}
