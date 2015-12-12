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

Sensors::Sensors(Scheduler& scheduler)
    : m_scheduler(scheduler)
{
    m_sensors.reserve(100);
}


Sensors::Sensor_Id Sensors::add_sensor(const std::string& name)
{
    Sensor_Id id = ++m_last_address;
    Sensor sensor;
    sensor.id = id;
    sensor.name = name;
    sensor.slot_id = m_scheduler.add_slot();
    sensor.measurements.reserve(10000);
    m_sensors.emplace_back(std::move(sensor));
    return id;
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

    m_scheduler.remove_slot(it->slot_id);

    m_sensors.erase(it);
}

const std::vector<Sensors::Sensor>& Sensors::get_all_sensors() const
{
    return m_sensors;
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

void Sensors::add_measurement(Sensor_Id id, const Measurement& measurement)
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

    Sensor& sensor = *it;
    if (!sensor.measurements.empty() &&
        sensor.measurements.back().time_point > measurement.time_point)
    {
        std::cerr << "Measurement from the past. Last: " <<
                     time_point_to_string(sensor.measurements.back().time_point) <<
                     ", New: " << time_point_to_string(measurement.time_point) << ".\n";
        return;
    }

    Clock::time_point begin = Clock::now() - m_history_duration;
    if (measurement.time_point >= begin)
    {
        sensor.measurements.push_back(measurement);
    }
    clear_old_measurements(sensor);
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

        sensor.slot_id = m_scheduler.add_slot();
        sensor.measurements.reserve(10000);
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

    for (const Measurement& measurement: sensor->measurements)
    {
        if (measurement.time_point < begin)
        {
            continue;
        }
        if (measurement.time_point > end)
        {
            break;
        }
        stream << time_point_to_string(measurement.time_point) << "\t"
               << measurement.temperature << "\t"
               << measurement.humidity << "\t"
               << measurement.vcc << "\t"
               << static_cast<float>(measurement.tx_rssi) << "\t"
               << static_cast<float>(measurement.rx_rssi) << "\n";
    }
}

void Sensors::set_measurement_history_duration(Clock::duration duration)
{
    m_history_duration = duration;
    clear_old_measurements();
}

void Sensors::clear_old_measurements()
{
    for (Sensor& sensor: m_sensors)
    {
        clear_old_measurements(sensor);
    }
}

void Sensors::clear_old_measurements(Sensor& sensor)
{
    Clock::time_point begin = Clock::now() - m_history_duration;

    while (!sensor.measurements.empty() &&
           sensor.measurements.front().time_point < begin)
    {
        sensor.measurements.erase(sensor.measurements.begin());
    }
}


