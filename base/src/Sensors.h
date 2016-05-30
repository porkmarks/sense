#pragma once

#include <chrono>
#include <vector>
#include <map>
#include <string>
#include <boost/thread.hpp>
#include <mutex>

#include "Comms.h"
#include "DB.h"


class Sensors
{
public:
    typedef DB::Clock Clock;
    typedef DB::Sensor_Id Sensor_Id;
    typedef DB::Sensor_Address Sensor_Address;
    typedef DB::Measurement Measurement;
    typedef DB::Sensor Sensor;
    typedef DB::Config Config;

    Sensors();
    ~Sensors();

    //how much sensors can deviate from the measurement global clock. This is due to the unstable clock frequency of the sensor CPU
    static const Clock::duration MEASUREMENT_JITTER;
    //how long does a communication burst take
    static const Clock::duration COMMS_DURATION;

    bool init(std::string const& server, std::string const& db, std::string const& username, std::string const& password, uint16_t port);

    //how often will the sensors do the measurement
    //bool set_measurement_period(Clock::duration period);
    Clock::duration get_measurement_period() const;

    uint32_t compute_next_measurement_index();
    uint32_t compute_last_confirmed_measurement_index(Sensor_Id id);

    Clock::time_point compute_next_measurement_time_point();
    Clock::time_point compute_next_comms_time_point(Sensor_Id id) const;

    //how often will the sensors talk to the base station
    //bool set_comms_period(Clock::duration period);
    Clock::duration compute_comms_period() const;

    //sensor manipulation
    Sensor const* add_expected_sensor();
    Sensor const* add_sensor(std::string const& name);
    bool remove_sensor(Sensor_Id id);
    bool revert_to_expected_sensor(Sensor_Id id);

    Sensor const* find_sensor_by_id(Sensor_Id id) const;
    Sensor const* find_sensor_by_address(Sensor_Address address) const;

    void set_sensor_measurement_range(Sensor_Id id, uint32_t first_measurement_index, uint32_t  last_measurement_index);
    void set_sensor_b2s_input_dBm(Sensor_Id id, int8_t dBm);

    //measurement manipulation
    void add_measurement(Sensor_Id id, Measurement const& measurement);

private:
    std::vector<Config> m_configs; //sorted ascendion, by date. Most recent one is last

    Config get_first_config() const;
    Config get_last_config() const;

    mutable std::recursive_mutex m_mutex;

    mutable Clock::time_point m_next_measurement_time_point;
    mutable Clock::time_point m_next_comms_time_point;

    mutable uint32_t m_next_measurement_index = 0;

    struct Sensor_Data
    {
        Sensor sensor;
        std::set<uint32_t> measurement_indices;
        uint32_t measurement_indices_base = 0;

        int8_t b2s_input_dBm = 0;
        uint32_t first_recorded_measurement_index = 0;
        uint32_t recorded_measurement_count = 0;
    };

    std::vector<Sensor_Data> m_sensors;

    Sensor_Data* _find_sensor_data_by_id(Sensor_Id id);
    Sensor_Data const* _find_sensor_data_by_id(Sensor_Id id) const;
    void optimize_measurement_indices(Sensor_Data& sensor_data);
    void remove_confirmed_indices(Sensor_Data& sensor_data);
    void remove_old_indices(Sensor_Data& sensor_data, uint32_t max);

    uint16_t m_last_address = Comms::SLAVE_ADDRESS_BEGIN;

    std::unique_ptr<DB> m_main_db;

    struct Worker
    {
        std::unique_ptr<DB> db;

        Clock::time_point last_config_refresh_time_point = Clock::now();

        boost::thread thread;
        bool stop_request = false;

        struct Item
        {
            Sensor_Id sensor_id;
            Clock::time_point time_point;
            Measurement measurement;
        };

        mutable std::recursive_mutex items_mutex;
        std::vector<Item> items;
    };

    Worker m_worker;

    void process_worker_thread();
    void refresh_config();
};

namespace std
{

template <class Rep, class Period,
    class = typename std::enable_if
    <
        std::chrono::duration<Rep, Period>::min() <
        std::chrono::duration<Rep, Period>::zero()
        >::type
    >
    constexpr
    inline
    std::chrono::duration<Rep, Period>
    abs(std::chrono::duration<Rep, Period> d)
{
    return d >= d.zero() ? d : -d;
}

}

