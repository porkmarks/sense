#pragma once

#include <inttypes.h>
#include <stddef.h>

class Storage
{
public:
    Storage();

    struct Data
    {
        float temperature;
        float humidity;
        float vcc;
    };

    struct iterator
    {
        Data data;
        int offset = -1;
        int8_t group_idx = -1;
    };

    bool unpack_next(iterator& it) const;

    size_t get_data_count() const;
    uint8_t get_group_count() const;
    uint8_t get_first_group_idx() const;
    uint8_t get_last_group_idx() const;

    bool push_back(const Data& data);
    bool pop_front();

    void clear();

private:

#ifndef __AVR__
#   pragma pack(push, 1) // exact fit - no padding
#endif

    struct Group
    {
        friend class Storage;
    protected:

        struct
        {
            uint8_t is_initialized : 1;
            uint8_t data_size : 7;
        } m_header;

        uint8_t m_vcc = 0; //(vcc - 2) * 100
        uint8_t m_humidity = 0;
        int16_t m_temperature = 0;

        void pack(const Data& data);
        Data unpack() const;

        uint8_t get_data_count() const;
        bool unpack_next(Storage::iterator& it) const;

        static constexpr uint8_t DATA_SIZE   = 32;
        uint8_t m_data[DATA_SIZE];
    };

#ifndef __AVR__
#   pragma pack(pop)
#endif

    const Group& get_first_group() const;
    Group& get_first_group();
    const Group& get_last_group() const;
    Group& get_last_group();
    bool pop_first_group();
    bool add_group();

    bool _unpack_next(iterator& it) const;

    static constexpr uint8_t MAX_GROUP_COUNT = 30;

    static_assert(sizeof(Group) == 37, "Storage::Group is broken");

    Data m_last_data;

    Group m_groups[MAX_GROUP_COUNT];
    uint8_t m_first_group_idx = 0;
    uint8_t m_group_count = 0;
    uint8_t m_first_group_skip_count = 0;
};

