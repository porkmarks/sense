#include "Sensors.h"
#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>
#include <cassert>

std::chrono::high_resolution_clock::time_point string_to_time_point(std::string const& str)
{
    using namespace std;
    using namespace std::chrono;

    int yyyy, mm, dd, HH, MM, SS;

    char scanf_format[] = "%4d-%2d-%2d %2d:%2d:%2d";

    sscanf(str.c_str(), scanf_format, &yyyy, &mm, &dd, &HH, &MM, &SS);

    tm ttm = tm();
    ttm.tm_year = yyyy - 1900; // Year since 1900
    ttm.tm_mon = mm - 1; // Month since January
    ttm.tm_mday = dd; // Day of the month [1-31]
    ttm.tm_hour = HH; // Hour of the day [00-23]
    ttm.tm_min = MM;
    ttm.tm_sec = SS;

    time_t ttime_t = mktime(&ttm);

    high_resolution_clock::time_point time_point_result = std::chrono::high_resolution_clock::from_time_t(ttime_t);

    return time_point_result;
}

std::string time_point_to_string(std::chrono::high_resolution_clock::time_point tp)
{
    using namespace std;
    using namespace std::chrono;

    auto ttime_t = high_resolution_clock::to_time_t(tp);
    //auto tp_sec = high_resolution_clock::from_time_t(ttime_t);
    //milliseconds ms = duration_cast<milliseconds>(tp - tp_sec);

    std::tm * ttm = localtime(&ttime_t);

    char date_time_format[] = "%Y-%m-%d %H:%M:%S";

    char time_str[256];

    strftime(time_str, sizeof(time_str), date_time_format, ttm);

    string result(time_str);
    //result.append(".");
    //result.append(to_string(ms.count()));

    return result;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

const Sensors::Clock::duration Sensors::MEASUREMENT_JITTER = std::chrono::seconds(10);
const Sensors::Clock::duration Sensors::COMMS_DURATION = std::chrono::seconds(10);


Sensors::Sensors(DB& db)
    : m_db(db)
{
    m_sensor_cache.reserve(100);
}

bool Sensors::init()
{
    boost::optional<DB::Config> config = m_db.get_config();
    if (!config)
    {
        m_config.measurement_period = std::chrono::seconds(10);
        m_config.comms_period = m_config.measurement_period * 3;
        m_config.baseline_time_point = Clock::now();
        if (!m_db.set_config(m_config))
        {
            std::cerr << "Cannot set config\n";
            return false;
        }
    }
    else
    {
        m_config = *config;
    }

    m_next_measurement_time_point = m_config.baseline_time_point;
    m_next_comms_time_point = m_config.baseline_time_point;

    boost::optional<std::vector<DB::Sensor>> sensors = m_db.get_sensors();
    if (!sensors)
    {
        std::cerr << "Cannot load sensors\n";
        return false;
    }

    for (DB::Sensor const& s: *sensors)
    {
        m_sensor_cache.emplace_back(s);
    }

    return true;
}

Sensors::Sensor const* Sensors::add_expected_sensor()
{
    boost::optional<DB::Expected_Sensor> expected_sensor = m_db.get_expected_sensor();
    if (!expected_sensor)
    {
        std::cerr << "Not expecting any sensor!\n";
        return nullptr;
    }

    Sensor const* sensor = add_sensor(expected_sensor->name);
    return sensor;
}

void Sensors::set_sensor_measurement_range(Sensor_Id id, uint32_t first_measurement_index, uint32_t measurement_count)
{
    auto it = std::find_if(m_sensor_cache.begin(), m_sensor_cache.end(), [id](Sensor const& sensor)
    {
        return sensor.id == id;
    });
    if (it == m_sensor_cache.end())
    {
        std::cerr << "Cannot find sensor id " << id << "\n";
        return;
    }

    Sensor& sensor = (*it);
    sensor.first_recorded_measurement_index = first_measurement_index;
    sensor.recorded_measurement_count = measurement_count;

    sensor.max_confirmed_measurement_index = std::max(sensor.max_confirmed_measurement_index, sensor.first_recorded_measurement_index);
}

Sensors::Sensor const* Sensors::add_sensor(std::string const& name)
{
    uint16_t address = ++m_last_address;

    boost::optional<Sensors::Sensor_Id> opt_id = m_db.add_sensor(name, address);
    if (!opt_id)
    {
        std::cerr << "Failed to add sensor in the DB\n";
        return nullptr;
    }

    assert(address >= Comms::SLAVE_ADDRESS_BEGIN && address < Comms::SLAVE_ADDRESS_END);
    Sensor sensor;
    sensor.id = *opt_id;
    sensor.name = name;
    sensor.address = address;
//    sensor.max_index = compute_next_measurement_index();
    m_sensor_cache.emplace_back(std::move(sensor));

    return &m_sensor_cache.back();
}

void Sensors::remove_all_sensors()
{
}

bool Sensors::set_comms_period(Clock::duration period)
{
    DB::Config config = m_config;
    config.comms_period = period;
    if (m_db.set_config(config))
    {
        m_config = config;
        return true;
    }
    return false;
}

Sensors::Clock::duration Sensors::compute_comms_period() const
{
    Clock::duration max_period = m_sensor_cache.size() * COMMS_DURATION;
    Clock::duration period = std::max(m_config.comms_period, max_period);
    return std::max(period, m_config.measurement_period + MEASUREMENT_JITTER);
}

bool Sensors::set_measurement_period(Clock::duration period)
{
    DB::Config config = m_config;
    config.measurement_period = period;
    if (m_db.set_config(config))
    {
        m_config = config;
        return true;
    }
    return false;
}
Sensors::Clock::duration Sensors::get_measurement_period() const
{
    return m_config.measurement_period;
}

Sensors::Clock::time_point Sensors::compute_next_measurement_time_point()
{
    auto now = Clock::now();
    while (m_next_measurement_time_point <= now)
    {
        m_next_measurement_time_point += m_config.measurement_period;
        m_next_measurement_index++;
    }

    return m_next_measurement_time_point;
}

uint32_t Sensors::compute_next_measurement_index()
{
    compute_next_measurement_time_point();
    return m_next_measurement_index;
}

uint32_t Sensors::compute_last_confirmed_measurement_index(Sensor_Id id) const
{
    Sensor const* sensor = find_sensor_by_id(id);
    if (!sensor)
    {
        std::cerr << "Cannot find sensor id " << id << "\n";
        return 0;
    }

    return sensor->max_confirmed_measurement_index;
}

Sensors::Clock::time_point Sensors::compute_next_comms_time_point(Sensor_Id id) const
{
    auto it = std::find_if(m_sensor_cache.begin(), m_sensor_cache.end(), [id](Sensor const& sensor)
    {
        return sensor.id == id;
    });
    if (it == m_sensor_cache.end())
    {
        std::cerr << "Cannot find sensor id " << id << "\n";
        return Clock::now() + std::chrono::hours(99999999);
    }

    Clock::duration period = compute_comms_period();

    auto now = Clock::now();
    while (m_next_comms_time_point <= now)
    {
        m_next_comms_time_point += period;
    }

    Clock::time_point start = m_next_comms_time_point;
    return start + std::distance(m_sensor_cache.begin(), it) * COMMS_DURATION;
}

void Sensors::remove_sensor(Sensor_Id id)
{
    auto it = std::find_if(m_sensor_cache.begin(), m_sensor_cache.end(), [id](Sensor const& sensor)
    {
        return sensor.id == id;
    });
    if (it == m_sensor_cache.end())
    {
        std::cerr << "Cannot find sensor id " << id << "\n";
        return;
    }

    m_sensor_cache.erase(it);
}

Sensors::Sensor const* Sensors::find_sensor_by_id(Sensor_Id id) const
{
    auto it = std::find_if(m_sensor_cache.begin(), m_sensor_cache.end(), [id](Sensor const& sensor)
    {
        return sensor.id == id;
    });
    if (it == m_sensor_cache.end())
    {
        return nullptr;
    }
    return &(*it);
}

Sensors::Sensor const* Sensors::find_sensor_by_address(Sensor_Address address) const
{
    auto it = std::find_if(m_sensor_cache.begin(), m_sensor_cache.end(), [address](Sensor const& sensor)
    {
        return sensor.address == address;
    });
    if (it == m_sensor_cache.end())
    {
        return nullptr;
    }
    return &(*it);
}

//void Sensors::push_back_measurement(Sensor_Id id, Measurement const& measurement)
//{
//    auto it = std::find_if(m_sensors.begin(), m_sensors.end(), [id](Sensor const& sensor)
//    {
//        return sensor.id == id;
//    });
//    if (it == m_sensors.end())
//    {
//        std::cerr << "Cannot find sensor id " << id << "\n";
//        return;
//    }

//    _add_measurement(m_next_measurement_index, id, measurement);

////    Sensor& sensor = *it;
////    if (!sensor.measurements.empty() &&
////        sensor.measurements.back().time_point > measurement.time_point)
////    {
////        std::cerr << "Measurement from the past. Last: " <<
////                     time_point_to_string(sensor.measurements.back().time_point) <<
////                     ", New: " << time_point_to_string(measurement.time_point) << ".\n";
////        return;
////    }

////    Clock::time_point begin = Clock::now() - m_history_duration;
////    if (measurement.time_point >= begin)
////    {
////        sensor.measurements.push_back(measurement);
////    }
//    clear_old_measurements();
//}

void Sensors::add_measurement(Sensor_Id id, Measurement const& measurement)
{
    auto it = std::find_if(m_sensor_cache.begin(), m_sensor_cache.end(), [id](Sensor const& sensor)
    {
        return sensor.id == id;
    });
    if (it == m_sensor_cache.end())
    {
        std::cerr << "Cannot find sensor id " << id << "\n";
        return;
    }

    Sensor& sensor = *it;

    if (measurement.index > sensor.max_confirmed_measurement_index)
    {
        Clock::time_point time_point = m_config.baseline_time_point + m_config.measurement_period * measurement.index;
        if (m_db.add_measurement(id, time_point, measurement))
        {
            if (measurement.index == sensor.max_confirmed_measurement_index + 1)
            {
                sensor.max_confirmed_measurement_index++;
            }
        }
    }
}

Sensors::Measurement Sensors::merge_measurements(Sensors::Measurement const& m1, Sensors::Measurement const& m2)
{
    Measurement merged;
    if (m1.flags & Measurement::Flag::SENSOR_ERROR)
    {
        merged.flags = m2.flags;
        merged.humidity = m2.humidity;
        merged.temperature = m2.temperature;
        merged.vcc = m2.vcc;
    }
    else if (m2.flags & Measurement::Flag::SENSOR_ERROR)
    {
        merged.flags = m1.flags;
        merged.humidity = m1.humidity;
        merged.temperature = m1.temperature;
        merged.vcc = m1.vcc;
    }
    else
    {
        merged.flags = m1.flags | m2.flags;
        merged.humidity = (m1.humidity + m2.humidity) / 2.f;
        merged.temperature = (m1.temperature + m2.temperature) / 2.f;
        merged.vcc = (m1.vcc + m2.vcc) / 2.f;
    }
    merged.rx_rssi = (m1.rx_rssi + m2.rx_rssi) / 2;
    merged.tx_rssi = (m1.tx_rssi + m2.tx_rssi) / 2;
    return merged;
}

