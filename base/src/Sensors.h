#pragma once

#include <chrono>
#include <vector>
#include <map>
#include <string>
#include <boost/thread.hpp>

#include "Sensor_Comms.h"

class Sensors
{
public:
    typedef std::chrono::high_resolution_clock Clock;
    typedef uint32_t Sensor_Id;
    typedef uint32_t Sensor_Address;

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

        uint32_t index = 0;
        float temperature = 0.f;
        float humidity = 0;
        float vcc = 0.f;
        int8_t b2s_input_dBm = 0;
        int8_t s2b_input_dBm = 0;
        uint8_t flags = 0;
    };

    struct Calibration
    {
        float temperature_bias = 0.f;
        float humidity_bias = 0.f;
    };

    struct Sensor
    {
        Sensor_Id id = 0;
        Sensor_Address address = 0;
        std::string name;
        uint32_t max_confirmed_measurement_index = 0;

        int8_t b2s_input_dBm = 0;
        uint32_t first_recorded_measurement_index = 0;
        uint32_t recorded_measurement_count = 0;

        Calibration calibration;
    };

    Sensors();
    ~Sensors();

    //how much sensors can deviate from the measurement global clock. This is due to the unstable clock frequency of the sensor CPU
    static const Clock::duration MEASUREMENT_JITTER;
    //how long does a communication burst take
    static const Clock::duration COMMS_DURATION;

    bool init();
    bool is_initialized() const;

    std::function<void(Sensor_Id sensor_id, Clock::time_point time_point, Measurement const& measurement)> cb_report_measurement;

    struct Config
    {
        std::string name;
        bool sensors_sleeping = false;
        Clock::duration measurement_period;
        Clock::duration comms_period;

        //This is computed when creating the config so that this equation holds for any config:
        // measurement_time_point = config.baseline_time_point + measurement_index * config.measurement_period
        //
        //So when creating a new config, this is how to calculate the baseline:
        // m = some measurement (any)
        // config.baseline_time_point = m.time_point - m.index * config.measurement_period
        //
        //The reason for this is to keep the indices valid in all configs
        Clock::time_point baseline_time_point;
    };

    Config const& get_config();
    void set_config(Config const& config);

    Clock::duration compute_comms_period() const;
    uint32_t compute_next_measurement_index() const; //the next real-time index
    uint32_t compute_next_measurement_index(Sensor_Id id) const; //the next index for this sensor. This might be in the future!!!
    uint32_t compute_last_confirmed_measurement_index(Sensor_Id id) const;

    Clock::time_point compute_next_measurement_time_point(Sensor_Id id) const;
    Clock::time_point compute_next_comms_time_point(Sensor_Id id) const;

    //sensor manipulation
    struct Unbound_Sensor_Data
    {
        Sensor_Id id;
        std::string name;
    };

    void set_unbound_sensor_data(Unbound_Sensor_Data const& data);
    boost::optional<Unbound_Sensor_Data> get_unbound_sensor_data() const;
    void confirm_sensor_binding(Sensor_Id id, bool confirmed);

    std::function<void(Sensor_Id, Sensor_Address, Sensors::Calibration const&)> cb_sensor_bound;

    Sensor const* bind_sensor(Sensors::Calibration const& calibration);
    Sensor const* add_sensor(Sensor_Id id, std::string const& name, Sensor_Address address, Calibration const& calibration);
    bool remove_sensor(Sensor_Id id);
    void remove_all_sensors();
    std::vector<Sensor> get_sensors() const;

    Sensor const* find_sensor_by_id(Sensor_Id id) const;
    Sensor const* find_sensor_by_address(Sensor_Address address) const;

    void set_sensor_calibration(Sensor_Id id, Calibration const& calibration);

    void set_sensor_measurement_range(Sensor_Id id, uint32_t first_measurement_index, uint32_t last_measurement_index);
    void set_sensor_b2s_input_dBm(Sensor_Id id, int8_t dBm);

    //measurement manipulation
    void report_measurements(Sensor_Id id, std::vector<Measurement> const& measurements);
    void confirm_measurement(Sensor_Id id, uint32_t measurement_index);

private:
    Sensor* _find_sensor_by_id(Sensor_Id id);

    Config m_config;
    bool m_is_initialized = false;

    boost::optional<Unbound_Sensor_Data> m_unbound_sensor_data_opt;
    std::vector<Sensor> m_sensors;

    uint32_t m_last_sensor_id = 0;
    uint32_t m_last_address = Sensor_Comms::SLAVE_ADDRESS_BEGIN;
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

