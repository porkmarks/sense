#pragma once

#include <stdint.h>
#include <math.h>

namespace data
{

enum class Type : uint8_t
{
    MEASUREMENT,
    PAIR_REQUEST,
    PAIR_RESPONSE,
};


#pragma pack(push, 1) // exact fit - no padding

struct Measurement
{
    Measurement(uint32_t timestamp,
              uint8_t index,
              float vcc,
              float humidity,
              float temperature)
        : timestamp(timestamp)
        , index(index)
    {
        vcc -= 2.f;
        this->vcc = static_cast<uint8_t>(fmin(fmax(vcc, 0.f), 2.55f) * 100.f);
        this->humidity = static_cast<uint8_t>(humidity * 2.55f);
        this->temperature = static_cast<uint8_t>(temperature * 100.f);
    }

    void unpack(uint32_t& timestamp, uint8_t& index, float& vcc, float& humidity, float& temperature)
    {
        timestamp = this->timestamp;
        index = this->index;
        vcc = static_cast<float>(this->vcc) / 100.f + 2.f;
        humidity = static_cast<float>(this->humidity) / 2.55f;
        temperature = static_cast<float>(this->temperature) / 100.f;
    }

//packed data
    uint32_t timestamp;
    uint8_t index;
    uint8_t vcc; //(vcc - 2) * 100
    uint8_t humidity; //*2.55
    int16_t temperature; //*100
};

struct Pair_Request
{
};
struct Pair_Response
{
    uint16_t address;
    uint32_t timestamp;
};

#pragma pack(pop)

}
