#include "DB.h"
#include <algorithm>
#include <cassert>

constexpr float k_alertVcc = 2.2f;
constexpr int8_t k_alertSignal = -110;


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
bool DB::saveAs(std::string const& filename) const
{
    return false;
}

size_t DB::getAlarmCount() const
{
    return m_alarms.size();
}

DB::Alarm const& DB::getAlarm(size_t index) const
{
    return m_alarms[index];
}

void DB::addAlarm(Alarm const& alarm)
{
    m_alarms.push_back(alarm);
}
void DB::removeAlarm(size_t index)
{
    if (index < m_alarms.size())
    {
        m_alarms.erase(m_alarms.begin() + index);
    }
    else
    {
        assert(false);
    }
}

uint8_t DB::computeTriggeredAlarm(Measurement const& measurement) const
{
    uint8_t triggered = 0;

    for (Alarm const& alarm: m_alarms)
    {
        if (alarm.highTemperatureWatch && measurement.temperature > alarm.highTemperature)
        {
            triggered |= TriggeredAlarm::Temperature;
        }
        if (alarm.lowTemperatureWatch && measurement.temperature < alarm.lowTemperature)
        {
            triggered |= TriggeredAlarm::Temperature;
        }

        if (alarm.highHumidityWatch && measurement.humidity > alarm.highHumidity)
        {
            triggered |= TriggeredAlarm::Humidity;
        }
        if (alarm.lowHumidityWatch && measurement.humidity < alarm.lowHumidity)
        {
            triggered |= TriggeredAlarm::Humidity;
        }

        if (alarm.vccWatch && measurement.vcc <= k_alertVcc)
        {
            triggered |= TriggeredAlarm::Vcc;
        }
        if (alarm.errorFlagsWatch && measurement.errorFlags != 0)
        {
            triggered |= TriggeredAlarm::ErrorFlags;
        }
        if (alarm.signalWatch && std::min(measurement.s2b, measurement.b2s) <= k_alertSignal)
        {
            triggered |= TriggeredAlarm::Signal;
        }
    }

    return triggered;
}

uint8_t DB::computeTriggeredAlarm(SensorId sensorId) const
{
    uint8_t triggered = 0;

    auto it = m_measurements.find(sensorId);
    if (it == m_measurements.end())
    {
        return triggered;
    }

    for (StoredMeasurement const& sm: it->second)
    {
        Measurement m = unpack(sensorId, sm);
        triggered |= computeTriggeredAlarm(m);
        if ((triggered & TriggeredAlarm::All) == TriggeredAlarm::All)
        {
            return triggered;
        }
    }

    return triggered;
}

bool DB::addMeasurement(Measurement const& measurement)
{
    PrimaryKey pk = computePrimaryKey(measurement);

    //check for duplicates
    auto it = std::lower_bound(m_sortedPrimaryKeys.begin(), m_sortedPrimaryKeys.end(), pk);
    if (it != m_sortedPrimaryKeys.end() && *it == pk)
    {
        return false;
    }

    m_measurements[measurement.sensorId].push_back(pack(measurement));
    m_sortedPrimaryKeys.insert(it, pk);

    return true;
}

std::vector<DB::Measurement> DB::getAllMeasurements() const
{
    std::vector<DB::Measurement> result;
    result.reserve(8192);

    for (auto const& pair : m_measurements)
    {
        for (StoredMeasurement const& sm: pair.second)
        {
            result.push_back(unpack(pair.first, sm));
        }
    }
    return result;
}
size_t DB::getAllMeasurementCount() const
{
    size_t count = 0;
    for (auto const& pair : m_measurements)
    {
        count += pair.second.size();
    }
    return count;
}

std::vector<DB::Measurement> DB::getFilteredMeasurements(Filter const& filter) const
{
    std::vector<DB::Measurement> result;
    result.reserve(8192);

    for (auto const& pair : m_measurements)
    {
        for (StoredMeasurement const& sm: pair.second)
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
size_t DB::getFilteredMeasurementCount(Filter const& filter) const
{
    size_t count = 0;
    for (auto const& pair : m_measurements)
    {
        for (StoredMeasurement const& sm: pair.second)
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

bool DB::getLastMeasurementForSensor(SensorId sensor_id, Measurement& measurement) const
{
    auto it = m_measurements.find(sensor_id);
    if (it == m_measurements.end())
    {
        return false;
    }

    Clock::time_point best_time_point = Clock::time_point(Clock::duration::zero());
    for (StoredMeasurement const& sm: it->second)
    {
        Measurement m = unpack(sensor_id, sm);
        if (m.timePoint > best_time_point)
        {
            measurement = m;
            best_time_point = m.timePoint;
        }
    }

    return true;
}

bool DB::cull(Measurement const& measurement, Filter const& filter) const
{
    if (filter.useSensorFilter)
    {
        if (std::find(filter.sensorIds.begin(), filter.sensorIds.end(), measurement.sensorId) == filter.sensorIds.end())
        {
            return false;
        }
    }
    if (filter.useIndexFilter)
    {
        if (measurement.index < filter.indexFilter.min || measurement.index > filter.indexFilter.max)
        {
            return false;
        }
    }
    if (filter.useTimePointFilter)
    {
        if (measurement.timePoint < filter.timePointFilter.min || measurement.timePoint > filter.timePointFilter.max)
        {
            return false;
        }
    }
    if (filter.useTemperatureFilter)
    {
        if (measurement.temperature < filter.temperatureFilter.min || measurement.temperature > filter.temperatureFilter.max)
        {
            return false;
        }
    }
    if (filter.useHumidityFilter)
    {
        if (measurement.humidity < filter.humidityFilter.min || measurement.humidity > filter.humidityFilter.max)
        {
            return false;
        }
    }
    if (filter.useVccFilter)
    {
        if (measurement.vcc < filter.vccFilter.min || measurement.vcc > filter.vccFilter.max)
        {
            return false;
        }
    }
    if (filter.useB2SFilter)
    {
        if (measurement.b2s < filter.b2sFilter.min || measurement.b2s > filter.b2sFilter.max)
        {
            return false;
        }
    }
    if (filter.useS2BFilter)
    {
        if (measurement.s2b < filter.s2bFilter.min || measurement.s2b > filter.s2bFilter.max)
        {
            return false;
        }
    }
    if (filter.useErrorsFilter)
    {
        bool has_errors = measurement.errorFlags != 0;
        if (has_errors != filter.errorsFilter)
        {
            return false;
        }
    }

    return true;
}

inline DB::StoredMeasurement DB::pack(Measurement const& m)
{
    StoredMeasurement sm;
    sm.index = m.index;
    sm.time_point = Clock::to_time_t(m.timePoint);
    sm.temperature = static_cast<int16_t>(std::max(std::min(m.temperature, 320.f), -320.f) * 100.f);
    sm.humidity = static_cast<int16_t>(std::max(std::min(m.humidity, 320.f), -320.f) * 100.f);
    sm.vcc = static_cast<uint8_t>(std::max(std::min((m.vcc - 2.f), 2.55f), 0.f) * 100.f);
    sm.b2s = m.b2s;
    sm.s2b = m.s2b;
    sm.flags = m.errorFlags;
    return sm;
}

inline DB::Measurement DB::unpack(SensorId sensor_id, StoredMeasurement const& sm)
{
    Measurement m;
    m.sensorId = sensor_id;
    m.index = sm.index;
    m.timePoint = Clock::from_time_t(sm.time_point);
    m.temperature = static_cast<float>(sm.temperature) / 100.f;
    m.humidity = static_cast<float>(sm.humidity) / 100.f;
    m.vcc = static_cast<float>(sm.vcc) / 100.f + 2.f;
    m.b2s = sm.b2s;
    m.s2b = sm.s2b;
    m.errorFlags = sm.flags;
    return m;
}

inline DB::PrimaryKey DB::computePrimaryKey(Measurement const& m)
{
    return (PrimaryKey(m.sensorId) << 32) | PrimaryKey(m.index);
}
inline DB::PrimaryKey DB::computePrimaryKey(SensorId sensor_id, StoredMeasurement const& m)
{
    return (PrimaryKey(sensor_id) << 32) | PrimaryKey(m.index);
}
