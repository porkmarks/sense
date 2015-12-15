#pragma once

#include <chrono>
#include <vector>
#include <map>
#include <string>

#include "Comms.h"


class Sensors
{
public:
    typedef std::chrono::high_resolution_clock Clock;
    typedef uint32_t Sensor_Id;

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

        float temperature = 0.f;
        float humidity = 0;
        float vcc = 0.f;
        int8_t tx_rssi = 0;
        int8_t rx_rssi = 0;
        uint8_t flags = 0;
    };

    struct Measurement_Entry
    {
        Clock::time_point time_point;
        std::map<Sensor_Id, Measurement> measurements;
    };

    struct Sensor
    {
        Sensor_Id id = 0;
        std::string name;
    };

    Sensors();

    //how much sensors can deviate from the measurement global clock. This is due to the unstable clock frequency of the sensor CPU
    static const Sensors::Clock::duration MEASUREMENT_JITTER;
    //how long does a communication burst take
    static const Sensors::Clock::duration COMMS_DURATION;


    //how often will the sensors do the measurement
    void set_measurement_period(Clock::duration period);
    Clock::duration get_measurement_period() const;

    Clock::time_point compute_next_measurement_time_point();
    Clock::time_point compute_next_comms_time_point(Sensor_Id id);

    //how often will the sensors talk to the base station
    void set_comms_period(Clock::duration period);
    Clock::duration compute_comms_period() const;

    //sensor manipulation
    Sensor_Id add_sensor(const std::string& name);
    void remove_sensor(Sensor_Id id);

    const Sensor* find_sensor_by_id(Sensor_Id id) const;

    //measurement manipulation
    void push_back_measurement(Sensor_Id id, const Measurement& measurement);
    void add_measurement(Clock::time_point time_point, Sensor_Id id, const Measurement& measurement);
    const Measurement* get_last_measurement(Sensor_Id id);

    //serialization and reporting
    void load_sensors(const std::string& filename);
    void save_sensors(const std::string& filename);

    void save_sensor_measurements_tsv(const std::string& filename, Sensor_Id id, Clock::time_point begin, Clock::time_point end);

    void set_measurement_history_duration(Clock::duration duration);

private:
    Measurement merge_measurements(const Measurement& m1, const Measurement& m2);
    void _add_measurement(Clock::time_point time_point, Sensor_Id id, const Measurement& measurement);

    Clock::duration m_measurement_period = std::chrono::minutes(5);
    Clock::duration m_comms_period = std::chrono::seconds(10);

    Clock::time_point m_last_measurement_time_point;
    Clock::time_point m_last_comms_time_point;

    std::vector<Sensor> m_sensors;

    uint32_t m_last_address = Comms::SLAVE_ADDRESS_BEGIN;
    Clock::duration m_history_duration = std::chrono::hours(24) * 30 * 3; //approx 3 months

    std::vector<Measurement_Entry> m_measurements;

    void clear_old_measurements();
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

