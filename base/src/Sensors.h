#pragma once

#include <chrono>
#include <vector>
#include <map>
#include <string>

#include "Comms.h"
#include "DB.h"


class Sensors
{
public:
    typedef DB::Clock Clock;
    typedef DB::Sensor_Id Sensor_Id;
    typedef DB::Sensor_Address Sensor_Address;


    typedef DB::Measurement Measurement;

    struct Sensor : public DB::Sensor
    {
        Sensor() = default;
        Sensor(Sensor const&) = default;
        Sensor(DB::Sensor const& other) : DB::Sensor(other) {}

        uint32_t first_recorded_measurement_index = 0;
        uint32_t recorded_measurement_count = 0;
    };

    Sensors(DB& db);

    //how much sensors can deviate from the measurement global clock. This is due to the unstable clock frequency of the sensor CPU
    static const Clock::duration MEASUREMENT_JITTER;
    //how long does a communication burst take
    static const Clock::duration COMMS_DURATION;

    bool init();

    //how often will the sensors do the measurement
    bool set_measurement_period(Clock::duration period);
    Clock::duration get_measurement_period() const;

    uint32_t compute_next_measurement_index();
    uint32_t compute_last_confirmed_measurement_index(Sensor_Id id) const;

    Clock::time_point compute_next_measurement_time_point();
    Clock::time_point compute_next_comms_time_point(Sensor_Id id) const;

    //how often will the sensors talk to the base station
    bool set_comms_period(Clock::duration period);
    Clock::duration compute_comms_period() const;

    //sensor manipulation
    Sensor const* add_expected_sensor();
    Sensor const* add_sensor(std::string const& name);
    void remove_sensor(Sensor_Id id);
    void remove_all_sensors();

    Sensor const* find_sensor_by_id(Sensor_Id id) const;
    Sensor const* find_sensor_by_address(Sensor_Address address) const;

    void set_sensor_measurement_range(Sensor_Id id, uint32_t first_measurement_index, uint32_t  last_measurement_index);

    //measurement manipulation
    void add_measurement(Sensor_Id id, Measurement const& measurement);

private:
    Measurement merge_measurements(Measurement const& m1, Measurement const& m2);
    void _add_measurement(Sensor_Id id, Measurement const& measurement);

    DB::Config m_config;

    mutable Clock::time_point m_next_measurement_time_point;
    mutable Clock::time_point m_next_comms_time_point;

    mutable uint32_t m_next_measurement_index = 0;

    std::vector<Sensor> m_sensor_cache;

    uint16_t m_last_address = Comms::SLAVE_ADDRESS_BEGIN;
    Clock::duration m_history_duration = std::chrono::hours(24) * 30 * 3; //approx 3 months

    DB& m_db;
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

