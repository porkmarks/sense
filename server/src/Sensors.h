#pragma once

#include <vector>
#include <string>

#include "Comms.h"
#include "Scheduler.h"


class Sensors
{
public:
    typedef std::chrono::high_resolution_clock Clock;
    typedef uint32_t Sensor_Id;

    struct Measurement
    {
        Clock::time_point time_point;
        float temperature = 0.f;
        float humidity = 0;
        float vcc = 0.f;
        int8_t tx_rssi = 0;
        int8_t rx_rssi = 0;
    };

    struct Sensor
    {
        Sensor_Id id = 0;
        std::string name;
        Scheduler::Slot_Id slot_id;

        std::vector<Measurement> measurements;
    };

    Sensors(Scheduler& scheduler);

    Sensor_Id add_sensor(const std::string& name);
    void remove_sensor(Sensor_Id id);
    const std::vector<Sensor>& get_all_sensors() const;

    const Sensor* find_sensor_by_id(Sensor_Id id) const;

    void add_measurement(Sensor_Id id, const Measurement& measurement);

    void load_sensors(const std::string& filename);
    void save_sensors(const std::string& filename);

    void save_sensor_measurements_tsv(const std::string& filename, Sensor_Id id, Clock::time_point begin, Clock::time_point end);

    void set_measurement_history_duration(Clock::duration duration);

private:
    Scheduler& m_scheduler;

    std::vector<Sensor> m_sensors;

    uint32_t m_last_address = Comms::SLAVE_ADDRESS_BEGIN;
    Clock::duration m_history_duration = std::chrono::hours(24) * 30 * 3; //approx 3 months

    void clear_old_measurements();
    void clear_old_measurements(Sensor& sensor);
};
