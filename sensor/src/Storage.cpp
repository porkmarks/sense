#include <Arduino.h>
#include "Storage.h"
#include "algorithm"

namespace storage
{

///////////////////////////////////////////////////////////////////////////////

struct Item8
{
    uint8_t is_16 : 1;
    //humidity is very unstable so allocate more bits
    uint8_t humidity_delta : 4;
    static constexpr float HUMIDITY_SCALE = 2.f;
    static constexpr float MAX_HUMIDITY_DELTA = 3.75f; //+-

    //temp is pretty stable.
    uint8_t temperature_delta : 3;
    static constexpr float TEMPERATURE_SCALE = 5.f;
    static constexpr float MAX_TEMPERATURE_DELTA = 0.7f;//+-

    bool pack(float last_t, float last_h, float t, float h)
    {
        float dt = t - last_t;
        if (fabs(dt) > MAX_TEMPERATURE_DELTA)
        {
            return false;
        }
        float dh = h - last_h;
        if (fabs(dh) > MAX_HUMIDITY_DELTA)
        {
            return false;
        }

        is_16 = 0;
        humidity_delta = static_cast<uint8_t>((dh + MAX_HUMIDITY_DELTA) * HUMIDITY_SCALE);
        temperature_delta = static_cast<uint8_t>((dt + MAX_TEMPERATURE_DELTA) * TEMPERATURE_SCALE);
    }
    void unpack(float last_t, float last_h, float& t, float& h) const
    {
        float dt = static_cast<float>(temperature_delta) / TEMPERATURE_SCALE - MAX_TEMPERATURE_DELTA;
        t = last_t + dt;
        float dh = static_cast<float>(humidity_delta) / HUMIDITY_SCALE - MAX_HUMIDITY_DELTA;
        h = last_h + dh;
    }
};
struct Item16
{
    uint16_t is_16 : 1;
    uint16_t humidity : 8; //absolute value, not a delta
    static constexpr float HUMIDITY_SCALE = 2.55f;
    uint16_t temperature_delta : 7;
    static constexpr float TEMPERATURE_SCALE = 5.f;
    static constexpr float MAX_TEMPERATURE_DELTA = 12.7f;//+-

    bool pack(float last_t, float last_h, float t, float h)
    {
        float dt = t - last_t;
        if (fabs(dt) > MAX_TEMPERATURE_DELTA)
        {
            return false;
        }

        is_16 = 1;
        int16_t ih = static_cast<int16_t>(h * HUMIDITY_SCALE);
        humidity = static_cast<uint8_t>(std::min(std::max(ih, 0), 255));

        int16_t idt = static_cast<int16_t>((dt + MAX_TEMPERATURE_DELTA) * TEMPERATURE_SCALE);
        temperature_delta = static_cast<uint8_t>(std::min(std::max(idt, 0), 127));
    }
    void unpack(float last_t, float last_h, float& t, float& h) const
    {
        float dt = static_cast<float>(temperature_delta) / TEMPERATURE_SCALE - MAX_TEMPERATURE_DELTA;
        t = last_t + dt;
        h = static_cast<float>(humidity) / HUMIDITY_SCALE;
    }
};

struct Group
{
    uint8_t vcc = 0; //(vcc - 2) * 100
    static constexpr float VCC_BIAS = 2.f;
    static constexpr float VCC_SCALE = 100.f;

    uint8_t humidity = 0;
    static constexpr float HUMIDITY_SCALE = 2.55f;

    int16_t temperature = 0;
    static constexpr float TEMPERATURE_SCALE = 100.f;

    void pack(float t, float h, float v)
    {
        int16_t iv = static_cast<int16_t>((v - VCC_BIAS) * VCC_SCALE);
        vcc = static_cast<uint8_t>(std::min(std::max(iv, 0), 255));

        int16_t ih = static_cast<int16_t>(h * HUMIDITY_SCALE);
        humidity = static_cast<uint8_t>(std::min(std::max(ih, 0), 255));

        int32_t it = static_cast<int32_t>(t * TEMPERATURE_SCALE);
        humidity = static_cast<int16_t>(std::min(std::max(it, int32_t(0)), 65535));
    }

    void unpack(float& t, float& h, float& v) const
    {
        v = static_cast<float>(vcc) / VCC_SCALE + VCC_BIAS;
        h = static_cast<float>(humidity) / HUMIDITY_SCALE;
        t = static_cast<float>(temperature) / TEMPERATURE_SCALE;
    }

    static constexpr uint8_t DATA_SIZE   = 32;
    uint8_t data[DATA_SIZE];

    union
    {
        uint8_t is_initialized : 1;
        uint8_t data_size : 7;
    } header;
};

static_assert(sizeof(Item8) == 1, "");
static_assert(sizeof(Item16) == 2, "");
static_assert(sizeof(Group) == 37, "");

//////////////////////////////////////////////////////////////////////////////

constexpr uint8_t GROUP_COUNT = 30;
Group s_groups[GROUP_COUNT];
static uint8_t s_first_group_idx = 0;
static uint8_t s_last_group_idx = 0;

static float s_last_temperature;
static float s_last_humidity;


void initialize()
{
    memset(s_groups, 0, sizeof(Group) * GROUP_COUNT);
}

bool push_back(float temperature, float humidity, float vcc)
{
    Group& g = get_last_group();
    if (g.header.is_initialized == 0)
    {
        g.header.is_initialized = 1;
        g.header.data_size = 0;
        g.pack(temperature, humidity, vcc);
        g.unpack(s_last_temperature, s_last_humidity, vcc);
    }
    else
    {
        Item8 item8;
        if (item8.pack(s_last_temperature, s_last_humidity, temperature, humidity) && g.header.data_size + sizeof(Item8) <= Group::DATA_SIZE)
        {
            memcpy(g.data + g.header.data_size, &item8, sizeof(Item8));
            g.header.data_size += sizeof(Item8);

            item8.unpack(s_last_temperature, s_last_humidity, temperature, humidity);
            s_last_temperature = temperature;
            s_last_humidity = humidity;
        }
        else if (g.header.data_size + sizeof(Item16) <= Group::DATA_SIZE)
        {
            Item16 item16;
            item16.pack(s_last_temperature, s_last_humidity, temperature, humidity);
            memcpy(g.data + g.header.data_size, &item16, sizeof(Item16));
            g.header.data_size += sizeof(Item16);

            item16.unpack(s_last_temperature, s_last_humidity, temperature, humidity);
            s_last_temperature = temperature;
            s_last_humidity = humidity;
        }
        else //new group
        {
            uint8_t new_group_idx = (s_last_group_idx + 1) % GROUP_COUNT;
            if (new_group_idx == s_first_group_idx)
            {
                return false;
            }
            s_last_group_idx = new_group_idx;

            return push_back(temperature, humidity, vcc);
        }
    }

    return true;
}

size_t get_group_count();
Group& get_first_group();
Group& get_last_group();
bool pop_first_group();

void clear();


}
