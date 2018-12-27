#pragma once

#include <chrono>
#include <vector>
#include <map>
#include <string>
#include <deque>
#include <thread>
#include <boost/optional.hpp>

#include "Sensor_Comms.h"

class Sensors
{
public:
    typedef std::chrono::system_clock Clock;
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

        Clock::time_point last_comms_tp = Clock::time_point(Clock::duration::zero());

        Calibration calibration;
        uint32_t serial_number = 0;

        bool sleeping = false;
    };

    Sensors();
    ~Sensors();

    //how much sensors can deviate from the measurement global clock. This is due to the unstable clock frequency of the sensor CPU
    static const Clock::duration MEASUREMENT_JITTER;
    //how long does a communication burst take
    static const Clock::duration COMMS_DURATION;

    bool init();
    bool is_initialized() const;

    struct Reported_Measurement
    {
        Sensor_Id sensor_id;
        Clock::time_point time_point;
        Measurement measurement;
    };

    std::function<void(std::vector<Reported_Measurement> const&)> cb_report_measurements;

    struct Config
    {
        std::string name;
        bool sensors_sleeping = false;
        uint8_t sensors_power = 15;
        Clock::duration measurement_period = std::chrono::seconds(5 * 60);
        Clock::duration comms_period = std::chrono::seconds(5 * 60);

        //when did this config become active?
        Clock::time_point baseline_measurement_time_point = Clock::time_point(Clock::duration::zero());
        uint32_t baseline_measurement_index = 0;
    };

    std::vector<Config> get_configs() const;
    void remove_all_configs();
    Config get_config() const;
    void set_config(Config config);

    Clock::duration compute_comms_period() const;
    uint32_t compute_next_measurement_index() const; //the next real-time index
    uint32_t compute_next_measurement_index(Sensor_Id id) const; //the next index for this sensor. This might be in the future!!!
    uint32_t compute_baseline_measurement_index() const; //sensors should start counting from this.
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

    std::function<void(Sensor_Id, Sensor_Address, uint32_t, Sensors::Calibration const&)> cb_sensor_bound;

    Sensor const* bind_sensor(uint32_t serial_number, Sensors::Calibration const& calibration);
    Sensor const* add_sensor(Sensor_Id id, std::string const& name, Sensor_Address address, uint32_t serial_number, Calibration const& calibration);
    bool remove_sensor(Sensor_Id id);
    void remove_all_sensors();
    std::vector<Sensor> get_sensors() const;

    Sensor const* find_sensor_by_id(Sensor_Id id) const;
    Sensor const* find_sensor_by_address(Sensor_Address address) const;

    void set_sensor_calibration(Sensor_Id id, Calibration const& calibration);

    void set_sensor_measurement_range(Sensor_Id id, uint32_t first_measurement_index, uint32_t last_measurement_index);
    void set_sensor_b2s_input_dBm(Sensor_Id id, int8_t dBm);
    void set_sensor_last_comms_time_point(Sensor_Id id, Clock::time_point tp);
    void set_sensor_sleeping(Sensor_Id id, bool sleeping);
    Clock::time_point get_sensor_last_comms_time_point(Sensor_Id id) const;

    void notify_sensor_details_changed(Sensor_Id id);

    std::function<void(Sensor_Id)> cb_sensor_details_changed;

    //measurement manipulation
    void report_measurements(Sensor_Id id, std::vector<Measurement> const& measurements);
    void confirm_measurement(Sensor_Id id, uint32_t measurement_index);

private:
    Sensor* _find_sensor_by_id(Sensor_Id id);
    Config find_config_for_measurement_index(uint32_t measurement_index) const;

    std::deque<Config> m_configs;

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

