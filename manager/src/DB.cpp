#include "DB.h"
#include <algorithm>

DB::DB()
{

}

bool DB::create(std::string const& name)
{
    m_filename = "measurements-" + name + ".db";
    m_measurements.clear();
    return true;
}

bool DB::open(std::string const& name)
{
    return false;
}


bool DB::save() const
{
    return false;
}
bool DB::save_as(std::string const& filename) const
{
    return false;
}

bool DB::add_measurement(Measurement const& measurement)
{
    Primary_Key pk = compute_primary_key(measurement);

    //check for duplicates
    auto it = std::lower_bound(m_sorted_primary_keys.begin(), m_sorted_primary_keys.end(), pk);
    if (it != m_sorted_primary_keys.end() && *it == pk)
    {
        return false;
    }

    m_measurements[measurement.sensor_id].push_back(pack(measurement));
    m_sorted_primary_keys.insert(it, pk);

    return true;
}

std::vector<DB::Measurement> DB::get_all_measurements() const
{
    std::vector<DB::Measurement> result;
    result.reserve(8192);

    for (auto const& pair : m_measurements)
    {
        for (Stored_Measurement const& sm: pair.second)
        {
            result.push_back(unpack(pair.first, sm));
        }
    }
    return result;
}
size_t DB::get_all_measurement_count() const
{
    size_t count = 0;
    for (auto const& pair : m_measurements)
    {
        count += pair.second.size();
    }
    return count;
}

std::vector<DB::Measurement> DB::get_filtered_measurements(Filter const& filter) const
{
    std::vector<DB::Measurement> result;
    result.reserve(8192);

    for (auto const& pair : m_measurements)
    {
        for (Stored_Measurement const& sm: pair.second)
        {
            Measurement m = unpack(pair.first, sm);
            if (cull(m, filter))
            {
                result.push_back(m);
            }
        }
    }
    return result;
}
size_t DB::get_filtered_measurement_count(Filter const& filter) const
{
    size_t count = 0;
    for (auto const& pair : m_measurements)
    {
        for (Stored_Measurement const& sm: pair.second)
        {
            Measurement m = unpack(pair.first, sm);
            if (cull(m, filter))
            {
                count++;
            }
        }
    }
    return count;
}

bool DB::get_last_measurement_for_sensor(Sensor_Id sensor_id, Measurement& measurement) const
{
    auto it = m_measurements.find(sensor_id);
    if (it == m_measurements.end())
    {
        return false;
    }

    Clock::time_point best_time_point = Clock::time_point(Clock::duration::zero());
    for (Stored_Measurement const& sm: it->second)
    {
        Measurement m = unpack(sensor_id, sm);
        if (m.time_point > best_time_point)
        {
            measurement = m;
            best_time_point = m.time_point;
        }
    }

    return true;
}

bool DB::cull(Measurement const& measurement, Filter const& filter) const
{
    if (filter.use_sensor_filter)
    {
        if (std::find(filter.sensor_ids.begin(), filter.sensor_ids.end(), measurement.sensor_id) == filter.sensor_ids.end())
        {
            return false;
        }
    }
    if (filter.use_index_filter)
    {
        if (measurement.index < filter.index_filter.min || measurement.index > filter.index_filter.max)
        {
            return false;
        }
    }
    if (filter.use_time_point_filter)
    {
        if (measurement.time_point < filter.time_point_filter.min || measurement.time_point > filter.time_point_filter.max)
        {
            return false;
        }
    }
    if (filter.use_temperature_filter)
    {
        if (measurement.temperature < filter.temperature_filter.min || measurement.temperature > filter.temperature_filter.max)
        {
            return false;
        }
    }
    if (filter.use_humidity_filter)
    {
        if (measurement.humidity < filter.humidity_filter.min || measurement.humidity > filter.humidity_filter.max)
        {
            return false;
        }
    }
    if (filter.use_vcc_filter)
    {
        if (measurement.vcc < filter.vcc_filter.min || measurement.vcc > filter.vcc_filter.max)
        {
            return false;
        }
    }
    if (filter.use_b2s_filter)
    {
        if (measurement.b2s_input_dBm < filter.b2s_filter.min || measurement.b2s_input_dBm > filter.b2s_filter.max)
        {
            return false;
        }
    }
    if (filter.use_s2b_filter)
    {
        if (measurement.s2b_input_dBm < filter.s2b_filter.min || measurement.s2b_input_dBm > filter.s2b_filter.max)
        {
            return false;
        }
    }
    if (filter.use_errors_filter)
    {
        bool has_errors = measurement.flags != 0;
        if (has_errors != filter.errors_filter)
        {
            return false;
        }
    }

    return true;
}

inline DB::Stored_Measurement DB::pack(Measurement const& m)
{
    Stored_Measurement sm;
    sm.index = m.index;
    sm.time_point = Clock::to_time_t(m.time_point);
    sm.temperature = static_cast<int16_t>(std::max(std::min(m.temperature, 320.f), -320.f) * 100.f);
    sm.humidity = static_cast<int16_t>(std::max(std::min(m.humidity, 320.f), -320.f) * 100.f);
    sm.vcc = static_cast<uint8_t>(std::max(std::min((m.vcc - 2.f), 2.55f), 0.f) * 100.f);
    sm.b2s_input_dBm = m.b2s_input_dBm;
    sm.s2b_input_dBm = m.s2b_input_dBm;
    sm.flags = m.flags;
    return sm;
}

inline DB::Measurement DB::unpack(Sensor_Id sensor_id, Stored_Measurement const& sm)
{
    Measurement m;
    m.sensor_id = sensor_id;
    m.index = sm.index;
    m.time_point = Clock::from_time_t(sm.time_point);
    m.temperature = static_cast<float>(sm.temperature) / 100.f;
    m.humidity = static_cast<float>(sm.humidity) / 100.f;
    m.vcc = static_cast<float>(sm.vcc) / 100.f + 2.f;
    m.b2s_input_dBm = sm.b2s_input_dBm;
    m.s2b_input_dBm = sm.s2b_input_dBm;
    m.flags = sm.flags;
    return m;
}

inline DB::Primary_Key DB::compute_primary_key(Measurement const& m)
{
    return (Primary_Key(m.sensor_id) << 32) | Primary_Key(m.index);
}
inline DB::Primary_Key DB::compute_primary_key(Sensor_Id sensor_id, Stored_Measurement const& m)
{
    return (Primary_Key(sensor_id) << 32) | Primary_Key(m.index);
}
