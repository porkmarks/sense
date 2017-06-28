#pragma once

#include <chrono>
#include <string>
#include <vector>
#include <map>
#include "Data_Defs.h"

class DB
{
public:
    DB();

    typedef uint32_t Sensor_Id;
    typedef std::chrono::high_resolution_clock Clock;

    bool create(std::string const& name);
    bool open(std::string const& name);

    bool save() const;
    bool save_as(std::string const& filename) const;

    struct Measurement
    {
        struct Flag
        {
            enum type : uint8_t
            {
                SENSOR_ERROR   = 1 << 0,
                COMMS_ERROR    = 1 << 1
            };
        };
        Sensor_Id sensor_id = 0;
        uint32_t index = 0;
        Clock::time_point time_point;
        float temperature = 0;
        float humidity = 0;
        float vcc = 0;
        int8_t b2s_input_dBm = 0;
        int8_t s2b_input_dBm = 0;
        uint8_t flags = 0;
    };

    bool add_measurement(Measurement const& measurement);

    template <typename T>
    struct Range
    {
        T min = T();
        T max = T();
    };

    struct Filter
    {
        bool use_sensor_filter = false;
        std::vector<Sensor_Id> sensor_ids;

        bool use_index_filter = false;
        Range<uint32_t> index_filter;

        bool use_time_point_filter = false;
        Range<Clock::time_point> time_point_filter;

        bool use_temperature_filter = false;
        Range<float> temperature_filter;

        bool use_humidity_filter = false;
        Range<float> humidity_filter;

        bool use_vcc_filter = false;
        Range<float> vcc_filter;

        bool use_b2s_filter = false;
        Range<int8_t> b2s_filter;

        bool use_s2b_filter = false;
        Range<int8_t> s2b_filter;

        bool use_errors_filter = false;
        bool errors_filter = true;
    };

    std::vector<Measurement> get_all_measurements() const;
    size_t get_all_measurement_count() const;

    std::vector<Measurement> get_filtered_measurements(Filter const& filter) const;
    size_t get_filtered_measurement_count(Filter const& filter) const;

private:

    bool cull(Measurement const& measurement, Filter const& filter) const;

    struct Stored_Measurement
    {
        uint32_t index;
        time_t time_point;
        int16_t temperature;    //t * 100
        int16_t humidity;       //h * 100
        uint8_t vcc;            //(vcc - 2) * 100
        int8_t b2s_input_dBm;
        int8_t s2b_input_dBm;
        uint8_t flags;
    };

    typedef std::vector<Stored_Measurement> Stored_Measurements;
    typedef uint64_t Primary_Key;

    static inline Stored_Measurement pack(Measurement const& m);
    static inline Measurement unpack(Sensor_Id sensor_id, Stored_Measurement const& m);
    static inline Primary_Key compute_primary_key(Measurement const& m);
    static inline Primary_Key compute_primary_key(Sensor_Id sensor_id, Stored_Measurement const& m);

    std::map<Sensor_Id, Stored_Measurements> m_measurements;
    std::vector<Primary_Key> m_sorted_primary_keys;

    std::string m_filename;
};
