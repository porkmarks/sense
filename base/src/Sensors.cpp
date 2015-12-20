#include "Sensors.h"
#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>

std::chrono::high_resolution_clock::time_point string_to_time_point(const std::string& str)
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

const Sensors::Clock::duration Sensors::MEASUREMENT_JITTER = std::chrono::seconds(60);
const Sensors::Clock::duration Sensors::COMMS_DURATION = std::chrono::seconds(10);


Sensors::Sensors()
{
    m_sensors.reserve(100);
    m_last_measurement_time_point = Clock::now();
    m_last_comms_time_point = Clock::now();
}


Sensors::Sensor_Id Sensors::add_sensor(const std::string& name)
{
    Sensor_Id id = ++m_last_address;
    Sensor sensor;
    sensor.id = id;
    sensor.name = name;
    m_sensors.emplace_back(std::move(sensor));
    return id;
}

void Sensors::set_comms_period(Clock::duration period)
{
    m_comms_period = std::max(Clock::duration(std::chrono::seconds(10)), period);
}
Sensors::Clock::duration Sensors::compute_comms_period() const
{
    Clock::duration max_period = m_sensors.size() * COMMS_DURATION;
    Clock::duration period = std::max(m_comms_period, max_period);
    return std::max(period, m_measurement_period + MEASUREMENT_JITTER);
}

void Sensors::set_measurement_period(Clock::duration period)
{
    m_measurement_period = std::max(Clock::duration(std::chrono::seconds(30)), period);
}
Sensors::Clock::duration Sensors::get_measurement_period() const
{
    return m_measurement_period;
}

Sensors::Clock::time_point Sensors::compute_next_measurement_time_point()
{
    auto now = Clock::now();
    while (m_last_measurement_time_point + m_measurement_period <= now)
    {
        m_last_measurement_time_point += m_measurement_period;
    }

    return m_last_measurement_time_point + m_measurement_period;
}

Sensors::Clock::time_point Sensors::compute_next_comms_time_point(Sensor_Id id)
{
    auto it = std::find_if(m_sensors.begin(), m_sensors.end(), [id](const Sensor& sensor)
    {
        return sensor.id == id;
    });
    if (it == m_sensors.end())
    {
        std::cerr << "Cannot find sensor id " << id << "\n";
        return Clock::now() + std::chrono::hours(99999999);
    }

    auto now = Clock::now();
    while (m_last_comms_time_point + m_comms_period <= now)
    {
        m_last_comms_time_point += m_comms_period;
    }

    Clock::time_point start = m_last_comms_time_point + m_comms_period;
    return start + std::distance(m_sensors.begin(), it) * COMMS_DURATION;
}

void Sensors::remove_sensor(Sensor_Id id)
{
    auto it = std::find_if(m_sensors.begin(), m_sensors.end(), [id](const Sensor& sensor)
    {
        return sensor.id == id;
    });
    if (it == m_sensors.end())
    {
        std::cerr << "Cannot find sensor id " << id << "\n";
        return;
    }

    m_sensors.erase(it);
}

const Sensors::Sensor* Sensors::find_sensor_by_id(Sensor_Id id) const
{
    auto it = std::find_if(m_sensors.begin(), m_sensors.end(), [id](const Sensor& sensor)
    {
        return sensor.id == id;
    });
    if (it == m_sensors.end())
    {
        return nullptr;
    }
    return &(*it);
}

void Sensors::push_back_measurement(Sensor_Id id, const Measurement& measurement)
{
    auto it = std::find_if(m_sensors.begin(), m_sensors.end(), [id](const Sensor& sensor)
    {
        return sensor.id == id;
    });
    if (it == m_sensors.end())
    {
        std::cerr << "Cannot find sensor id " << id << "\n";
        return;
    }

    _add_measurement(Clock::now(), id, measurement);

//    Sensor& sensor = *it;
//    if (!sensor.measurements.empty() &&
//        sensor.measurements.back().time_point > measurement.time_point)
//    {
//        std::cerr << "Measurement from the past. Last: " <<
//                     time_point_to_string(sensor.measurements.back().time_point) <<
//                     ", New: " << time_point_to_string(measurement.time_point) << ".\n";
//        return;
//    }

//    Clock::time_point begin = Clock::now() - m_history_duration;
//    if (measurement.time_point >= begin)
//    {
//        sensor.measurements.push_back(measurement);
//    }
    clear_old_measurements();
}

void Sensors::add_measurement(Clock::time_point time_point, Sensor_Id id, const Measurement& measurement)
{
    auto it = std::find_if(m_sensors.begin(), m_sensors.end(), [id](const Sensor& sensor)
    {
      return sensor.id == id;
    });
    if (it == m_sensors.end())
    {
      std::cerr << "Cannot find sensor id " << id << "\n";
      return;
    }

    _add_measurement(time_point, id, measurement);
}

void Sensors::_add_measurement(Clock::time_point time_point, Sensor_Id id, const Measurement& measurement)
{
    //find the measurement that is older than time_point - period/2.
    Measurement_Entry new_entry;
    new_entry.time_point = time_point - m_measurement_period / 2;
    auto upper_it = std::upper_bound(m_measurements.begin(), m_measurements.end(), new_entry, [](const Measurement_Entry& e1, const Measurement_Entry& e2)
    {
        return e1.time_point < e2.time_point;
    });

    if (upper_it != m_measurements.end())
    {
        //if we found one, check if it's within +- period/2 of the desired timepoint.
        //For sure it has to be older than time_point - period/2 because this is what the upper_bound did
        Measurement_Entry& entry = *upper_it;
        //merge with the entry?
        if (std::abs(entry.time_point - time_point) < m_measurement_period / 2)
        {
            auto it = entry.measurements.find(id);
            if (it != entry.measurements.end())
            {
                Measurement merged = merge_measurements(it->second, measurement);
                entry.measurements.emplace(id, merged);
            }
            else
            {
                entry.measurements.emplace(id, measurement);
            }
            return;
        }
    }

    //if no proper time slot was found, insert it in place
    Measurement_Entry entry;
    entry.time_point = time_point;
    entry.measurements.emplace(id, measurement);

    m_measurements.emplace(upper_it, entry);
}

Sensors::Measurement Sensors::merge_measurements(const Sensors::Measurement& m1, const Sensors::Measurement& m2)
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

const Sensors::Measurement* Sensors::get_last_measurement(Sensor_Id id)
{
    for (auto mit = m_measurements.rbegin(); mit != m_measurements.rend(); ++mit)
    {
       Measurement_Entry& entry = *mit;
       auto it = entry.measurements.find(id);
       if (it != entry.measurements.end())
       {
            return &it->second;
       }
    }
    return nullptr;
}
void Sensors::set_measurement_history_duration(Clock::duration duration)
{
    m_history_duration = duration;
    clear_old_measurements();
}

void Sensors::clear_old_measurements()
{
    Clock::time_point begin = Clock::now() - m_history_duration;

    while (!m_measurements.empty() &&
    m_measurements.front().time_point < begin)
    {
        m_measurements.erase(m_measurements.begin());
    }
}

void Sensors::load_sensors(const std::string& filename)
{
    while (!m_sensors.empty())
    {
        remove_sensor(m_sensors.back().id);
    }

    std::ifstream stream(filename);
    if (!stream)
    {
        std::cerr << "Cannot open file " << filename << "\n";
        return;
    }

    std::string line;
    //skip header
    std::getline(stream, line);

    while (std::getline(stream, line))
    {
        std::istringstream ss(line);

        Sensor sensor;

        ss >> sensor.id >> sensor.name;
        std::cout << "Loaded sensor " << sensor.name << " id " << sensor.id << "\n";

        m_sensors.emplace_back(std::move(sensor));
    }
}

void Sensors::save_sensors(const std::string& filename)
{
    std::ofstream stream(filename);
    if (!stream)
    {
        std::cerr << "Cannot open file " << filename << "\n";
        return;
    }

    //header
    stream << "id" << "\t"
           << "name" << "\n";

    for (const Sensor& sensor: m_sensors)
    {
        stream << sensor.id << "\t" << sensor.name << "\n";
    }
}

void Sensors::save_sensor_measurements_tsv(const std::string& filename, Sensor_Id id, Clock::time_point begin, Clock::time_point end)
{
    const Sensor* sensor = find_sensor_by_id(id);
    if (!sensor)
    {
        std::cerr << "Cannot find sensor id " << id << "\n";
        return;
    }

    std::ofstream stream(filename);
    if (!stream)
    {
        std::cerr << "Cannot open file " << filename << "\n";
        return;
    }

    //header
    stream << "temperature" << "\t"
           << "humidity" << "\t"
           << "vcc" << "\t"
           << "tx_rssi" << "\t"
           << "rx_rssi" << "\n";

//    for (const Measurement& measurement: sensor->measurements)
//    {
//        if (measurement.time_point < begin)
//        {
//            continue;
//        }
//        if (measurement.time_point > end)
//        {
//            break;
//        }
//        stream << time_point_to_string(measurement.time_point) << "\t"
//               << measurement.temperature << "\t"
//               << measurement.humidity << "\t"
//               << measurement.vcc << "\t"
//               << static_cast<float>(measurement.tx_rssi) << "\t"
//               << static_cast<float>(measurement.rx_rssi) << "\n";
//    }
}



