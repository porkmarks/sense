#include <math.h>
#include "Storage.h"
#include <string.h>
#include <assert.h>

///////////////////////////////////////////////////////////////////////////////

namespace std
{
template<typename T> T min(T a, T b) { return a < b ? a : b; }
template<typename T> T max(T a, T b) { return a > b ? a : b; }

}

#ifndef __AVR__
#   pragma pack(push, 1) // exact fit - no padding
#endif

struct Item8
{
    uint8_t m_is_16 : 1;
    //humidity is very unstable so allocate more bits
    uint8_t m_humidity_delta : 4;
    static constexpr float HUMIDITY_SCALE = 2.f;
    static constexpr float MAX_HUMIDITY_DELTA = 3.75f; //+-

    //temp is pretty stable.
    uint8_t m_temperature_delta : 3;
    static constexpr float TEMPERATURE_SCALE = 5.f;
    static constexpr float MAX_TEMPERATURE_DELTA = 0.7f;//+-

    typedef Storage::Data Data;

    bool pack(const Data& last_data, const Data& data);
    Data unpack(const Data& last_data) const;
};
struct Item16
{
    uint16_t m_is_16 : 1;
    uint16_t m_humidity : 8; //absolute value, not a delta
    static constexpr float HUMIDITY_SCALE = 2.55f;
    uint16_t m_temperature_delta : 7;
    static constexpr float TEMPERATURE_SCALE = 5.f;
    static constexpr float MAX_TEMPERATURE_DELTA = 12.7f;//+-

    typedef Storage::Data Data;

    bool pack(const Data& last_data, const Data& data);
    Data unpack(const Data& last_data) const;
};

#ifndef __AVR__
#   pragma pack(pop)
#endif

static_assert(sizeof(Item8) == 1, "");
static_assert(sizeof(Item16) == 2, "");

static_assert(sizeof(Storage) <= 38 * 30 + 18, "");


//////////////////////////////////////////////////////////////////////////

bool Item8::pack(const Data& last_data, const Data& data)
{
    float dt = data.temperature - last_data.temperature;
    if (fabs(dt) > MAX_TEMPERATURE_DELTA)
    {
        return false;
    }
    float dh = data.humidity - last_data.humidity;
    if (fabs(dh) > MAX_HUMIDITY_DELTA)
    {
        return false;
    }

    m_is_16 = 0;
    m_humidity_delta = static_cast<uint8_t>((dh + MAX_HUMIDITY_DELTA) * HUMIDITY_SCALE);
    m_temperature_delta = static_cast<uint8_t>((dt + MAX_TEMPERATURE_DELTA) * TEMPERATURE_SCALE);
    return true;
}
Item8::Data Item8::unpack(const Data& last_data) const
{
    Data data;
    float dt = static_cast<float>(m_temperature_delta) / TEMPERATURE_SCALE - MAX_TEMPERATURE_DELTA;
    data.temperature = last_data.temperature + dt;
    float dh = static_cast<float>(m_humidity_delta) / HUMIDITY_SCALE - MAX_HUMIDITY_DELTA;
    data.humidity = last_data.humidity + dh;
    return data;
}
bool Item16::pack(const Data& last_data, const Data& data)
{
    float dt = data.temperature - last_data.temperature;
    if (fabs(dt) > MAX_TEMPERATURE_DELTA)
    {
        return false;
    }

    m_is_16 = 1;
    int16_t ih = static_cast<int16_t>(data.humidity * HUMIDITY_SCALE);
    m_humidity = static_cast<uint8_t>(std::min(std::max(ih, int16_t(0)), int16_t(255)));

    int16_t idt = static_cast<int16_t>((dt + MAX_TEMPERATURE_DELTA) * TEMPERATURE_SCALE);
    m_temperature_delta = static_cast<uint8_t>(std::min(std::max(idt, int16_t(0)), int16_t(127)));
    return true;
}
Item16::Data Item16::unpack(const Data& last_data) const
{
    Data data;
    float dt = static_cast<float>(m_temperature_delta) / TEMPERATURE_SCALE - MAX_TEMPERATURE_DELTA;
    data.temperature = last_data.temperature + dt;
    data.humidity = static_cast<float>(m_humidity) / HUMIDITY_SCALE;
    return data;
}

///////////////////////////////////////////////////////////////////////////////

static constexpr float GROUP_VCC_BIAS = 2.f;
static constexpr float GROUP_VCC_SCALE = 100.f;
static constexpr float GROUP_MIN_VCC = 2.f;
static constexpr float GROUP_MAX_VCC = 4.5f;

static constexpr float GROUP_HUMIDITY_SCALE = 2.55f;
static constexpr float GROUP_MIN_HUMIDITY = 0.f;
static constexpr float GROUP_MAX_HUMIDITY = 100.f;

static constexpr float GROUP_TEMPERATURE_SCALE = 100.f;
static constexpr float GROUP_MIN_TEMPERATURE = -60.f;
static constexpr float GROUP_MAX_TEMPERATURE = 80.f;

///////////////////////////////////////////////////////////////////////////////

void Storage::Group::pack(const Data& data)
{
    float humidity = std::min(std::max(data.humidity, GROUP_MIN_HUMIDITY), GROUP_MAX_HUMIDITY);
    int16_t ih = static_cast<int16_t>(humidity * GROUP_HUMIDITY_SCALE);
    m_humidity = static_cast<uint8_t>(std::min(std::max(ih, int16_t(0)), int16_t(255)));

    float temperature = std::min(std::max(data.temperature, GROUP_MIN_TEMPERATURE), GROUP_MAX_TEMPERATURE);
    int32_t it = static_cast<int32_t>(temperature * GROUP_TEMPERATURE_SCALE);
    m_temperature = static_cast<int16_t>(std::min(std::max(it, int32_t(-32767)), int32_t(32767)));
}

Storage::Data Storage::Group::unpack() const
{
    Data data;
    data.humidity = static_cast<float>(m_humidity) / GROUP_HUMIDITY_SCALE;
    data.temperature = static_cast<float>(m_temperature) / GROUP_TEMPERATURE_SCALE;
    return data;
}

bool Storage::Group::unpack_next(Storage::iterator& it) const
{
    if (!m_header.is_initialized)
    {
        return false;
    }
    if (it.offset == -1)
    {
        it.data = unpack();
        it.offset = 0;
        return true;
    }
    if (it.offset < 0)
    {
        return false;
    }
    if (it.offset >= m_header.data_size)
    {
        return false;
    }

    {
        const Item8* item8 = reinterpret_cast<const Item8*>(m_data + it.offset);
        if (!item8->m_is_16)
        {
            it.data = item8->unpack(it.data);
            it.offset += sizeof(Item8);
            return true;
        }
    }

    {
        const Item16* item16 = reinterpret_cast<const Item16*>(m_data + it.offset);
        it.data = item16->unpack(it.data);
        it.offset += sizeof(Item16);
        return true;
    }
}

uint8_t Storage::Group::get_data_count() const
{
    uint8_t data_count = 0;
    Storage::iterator it;

    while (unpack_next(it))
    {
        data_count++;
    }
    return data_count;
}


///////////////////////////////////////////////////////////////////////////////

Storage::Storage()
{
    memset(m_groups, 0, sizeof(Group) * MAX_GROUP_COUNT);
}


bool Storage::unpack_next(iterator& it) const
{
    if (m_group_count == 0)
    {
        return false;
    }

    if (it.group_idx < 0)
    {
        const Group& g = get_first_group();
        //skip the popped elements in the beginning and warm up the data
        for (uint8_t i = 0; i < g.m_header.skip_count; i++)
        {
            bool ok = _unpack_next(it);
            assert(ok);
        }
    }

    return _unpack_next(it);
}

bool Storage::_unpack_next(iterator& it) const
{
    if (m_group_count == 0)
    {
        return false;
    }

    if (it.group_idx < 0)
    {
        it.group_idx = m_first_group_idx;
        it.offset = -1;
    }

    const Storage::Group& group = m_groups[it.group_idx];
    if (!group.unpack_next(it))
    {
        if (it.group_idx == get_last_group_idx())
        {
            return false;
        }

        it.group_idx = (it.group_idx + 1) % MAX_GROUP_COUNT;
        it.offset = -1;
        return unpack_next(it);
    }

    return true;
}

size_t Storage::get_data_count() const
{
    size_t data_count = 0;
    Storage::iterator it;

    while (unpack_next(it))
    {
        data_count++;
    }
    return data_count;
}

uint8_t Storage::get_group_count() const
{
    return m_group_count;
}

uint8_t Storage::get_first_group_idx() const
{
    return m_first_group_idx;
}

uint8_t Storage::get_last_group_idx() const
{
    return (m_first_group_idx + m_group_count - 1) % MAX_GROUP_COUNT;
}

Storage::Group& Storage::get_first_group()
{
    return m_groups[get_first_group_idx()];
}

const Storage::Group& Storage::get_first_group() const
{
    return m_groups[get_first_group_idx()];
}

Storage::Group& Storage::get_last_group()
{
    return m_groups[get_last_group_idx()];
}

const Storage::Group& Storage::get_last_group() const
{
    return m_groups[get_last_group_idx()];
}

bool Storage::pop_first_group()
{
    if (m_group_count == 0)
    {
        return false;
    }
    m_first_group_idx = (m_first_group_idx + 1) % MAX_GROUP_COUNT;
    m_group_count--;
    return true;
}

bool Storage::add_group()
{
    if (m_group_count >= MAX_GROUP_COUNT)
    {
        return false;
    }
    m_group_count++;
    Group& group = get_last_group();

    memset(&group, 0, sizeof(Group));
    return true;
}

void Storage::clear()
{
    m_first_group_idx = 0;
    m_group_count = 0;

    memset(m_groups, 0, sizeof(Group) * MAX_GROUP_COUNT);
}

bool Storage::empty() const
{
    return m_group_count == 0;
}

bool Storage::pop_front()
{
    if (get_group_count() == 0)
    {
        return false;
    }

    Group& g = get_first_group();
    g.m_header.skip_count++;
    if (g.m_header.skip_count >= g.get_data_count())
    {
        pop_first_group();
    }

    return true;
}

bool Storage::push_back(const Data& data)
{
    if (get_group_count() == 0)
    {
        if (!add_group())
        {
            return false;
        }
    }
    assert(get_group_count() > 0);

    Group& g = get_last_group();
    if (g.m_header.is_initialized == 0)
    {
        g.m_header.is_initialized = 1;
        g.m_header.data_size = 0;
        g.pack(data);

        //unpack to set the last values AFTER the quantization
        m_last_data = g.unpack();
    }
    else
    {
        Item8 item8;
        if (item8.pack(m_last_data, data) && g.m_header.data_size + sizeof(Item8) <= Group::DATA_SIZE)
        {
            memcpy(g.m_data + g.m_header.data_size, &item8, sizeof(Item8));
            g.m_header.data_size += sizeof(Item8);

            //unpack to set the last values AFTER the quantization
            m_last_data = item8.unpack(m_last_data);
        }
        else if (g.m_header.data_size + sizeof(Item16) <= Group::DATA_SIZE)
        {
            Item16 item16;
            item16.pack(m_last_data, data);
            memcpy(g.m_data + g.m_header.data_size, &item16, sizeof(Item16));
            g.m_header.data_size += sizeof(Item16);

            //unpack to set the last values AFTER the quantization
            m_last_data = item16.unpack(m_last_data);
        }
        else //new group
        {
            if (!add_group())
            {
                return false;
            }
            return push_back(data);
        }
    }

    return true;
}

