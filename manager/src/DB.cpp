#include "DB.h"
#include <algorithm>
#include <functional>
#include <cassert>
#include <fstream>
#include <iostream>

#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/writer.h"
#include "rapidjson/reader.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/ostreamwrapper.h"
#include "rapidjson/istreamwrapper.h"

#include "CRC.h"

constexpr float k_alertVcc = 2.2f;
constexpr int8_t k_alertSignal = -110;


static bool renameFile(const char* oldName, const char* newName)
{
#ifdef _WIN32
    //one on success
    return MoveFile(_T(oldName), _T(newName)) != 0;
#else
    //zero on success
    return rename(oldName, newName) == 0;
#endif
}

template <typename Stream, typename T>
void write(Stream& s, T const& t)
{
    s.write(reinterpret_cast<const char*>(&t), sizeof(T));
}

template <typename Stream, typename T>
bool read(Stream& s, T& t)
{
    return s.read(reinterpret_cast<char*>(&t), sizeof(T)).good();
}

//////////////////////////////////////////////////////////////////////////

DB::DB()
{
    m_storeThread = std::thread(std::bind(&DB::storeThreadProc, this));
}

//////////////////////////////////////////////////////////////////////////

DB::~DB()
{
    m_storeThreadExit = true;
    m_storeCV.notify_all();
    if (m_storeThread.joinable())
    {
        m_storeThread.join();
    }
}

//////////////////////////////////////////////////////////////////////////

bool DB::create(std::string const& name)
{
    m_dbFilename = "sense-" + name + ".db";
    m_dataFilename = "sense-" + name + ".data";
    m_mainData.measurements.clear();

    remove(m_dbFilename.c_str());
    remove(m_dataFilename.c_str());

    return true;
}

//////////////////////////////////////////////////////////////////////////

bool DB::open(std::string const& name)
{
    Clock::time_point start = Clock::now();

    m_dbFilename = "sense-" + name + ".db";
    m_dataFilename = "sense-" + name + ".data";

    Data data;

    {
        std::ifstream file(m_dataFilename);
        if (!file.is_open())
        {
            std::cerr << "Failed to open " << m_dataFilename << " file: " << std::strerror(errno) << "\n";
            return false;
        }

        rapidjson::Document document;
        rapidjson::BasicIStreamWrapper<std::ifstream> reader(file);
        document.ParseStream(reader);
        if (document.GetParseError() != rapidjson::ParseErrorCode::kParseErrorNone)
        {
            std::cerr << "Failed to open " << m_dataFilename << " file: " << rapidjson::GetParseErrorFunc(document.GetParseError()) << "\n";
            return false;
        }
        if (!document.IsObject())
        {
            std::cerr << "Bad document.\n";
            return false;
        }

        auto it = document.FindMember("config");
        if (it == document.MemberEnd() || !it->value.IsObject())
        {
            std::cerr << "Bad or missing config object\n";
            return false;
        }

        {
            rapidjson::Value const& configj = it->value;
            auto it = configj.FindMember("name");
            if (it == configj.MemberEnd() || !it->value.IsString())
            {
                std::cerr << "Bad or missing config name\n";
                return false;
            }
            data.config.descriptor.name = it->value.GetString();

            it = configj.FindMember("sensors_sleeping");
            if (it == configj.MemberEnd() || !it->value.IsBool())
            {
                std::cerr << "Bad or missing config sensors_sleeping\n";
                return false;
            }
            data.config.descriptor.sensorsSleeping = it->value.GetBool();

            it = configj.FindMember("measurement_period");
            if (it == configj.MemberEnd() || !it->value.IsUint64())
            {
                std::cerr << "Bad or missing config measurement_period\n";
                return false;
            }
            data.config.descriptor.measurementPeriod = std::chrono::seconds(it->value.GetUint64());

            it = configj.FindMember("comms_period");
            if (it == configj.MemberEnd() || !it->value.IsUint64())
            {
                std::cerr << "Bad or missing config comms_period\n";
                return false;
            }
            data.config.descriptor.commsPeriod = std::chrono::seconds(it->value.GetUint64());

            it = configj.FindMember("baseline");
            if (it == configj.MemberEnd() || !it->value.IsUint64())
            {
                std::cerr << "Bad or missing config baseline\n";
                return false;
            }
            data.config.baselineTimePoint = Clock::time_point(std::chrono::seconds(it->value.GetUint64()));
        }

        it = document.FindMember("sensors");
        if (it == document.MemberEnd() || !it->value.IsArray())
        {
            std::cerr << "Bad or missing sensors array\n";
            return false;
        }

        {
            rapidjson::Value const& sensorsj = it->value;
            for (size_t i = 0; i < sensorsj.Size(); i++)
            {
                Sensor sensor;
                rapidjson::Value const& sensorj = sensorsj[i];
                auto it = sensorj.FindMember("name");
                if (it == sensorj.MemberEnd() || !it->value.IsString())
                {
                    std::cerr << "Bad or missing config name\n";
                    return false;
                }
                sensor.descriptor.name = it->value.GetString();

                it = sensorj.FindMember("id");
                if (it == sensorj.MemberEnd() || !it->value.IsUint())
                {
                    std::cerr << "Bad or missing sensor id\n";
                    return false;
                }
                sensor.id = it->value.GetUint();

                it = sensorj.FindMember("address");
                if (it == sensorj.MemberEnd() || !it->value.IsUint())
                {
                    std::cerr << "Bad or missing sensor address\n";
                    return false;
                }
                sensor.address = it->value.GetUint();

                it = sensorj.FindMember("state");
                if (it == sensorj.MemberEnd() || !it->value.IsUint())
                {
                    std::cerr << "Bad or missing sensor state\n";
                    return false;
                }
                sensor.state = static_cast<Sensor::State>(it->value.GetUint());

                it = sensorj.FindMember("next_measurement");
                if (it == sensorj.MemberEnd() || !it->value.IsUint64())
                {
                    std::cerr << "Bad or missing sensor next_measurement\n";
                    return false;
                }
                sensor.nextMeasurementTimePoint = Clock::time_point(std::chrono::seconds(it->value.GetUint64()));

                it = sensorj.FindMember("next_comms");
                if (it == sensorj.MemberEnd() || !it->value.IsUint64())
                {
                    std::cerr << "Bad or missing sensor next_comms\n";
                    return false;
                }
                sensor.nextCommsTimePoint = Clock::time_point(std::chrono::seconds(it->value.GetUint64()));

                data.sensors.push_back(sensor);
            }
        }

        it = document.FindMember("alarms");
        if (it == document.MemberEnd() || !it->value.IsArray())
        {
            std::cerr << "Bad or missing alarms array\n";
            return false;
        }

        {
            rapidjson::Value const& alarmsj = it->value;
            for (size_t i = 0; i < alarmsj.Size(); i++)
            {
                Alarm alarm;
                rapidjson::Value const& alarmj = alarmsj[i];
                auto it = alarmj.FindMember("name");
                if (it == alarmj.MemberEnd() || !it->value.IsString())
                {
                    std::cerr << "Bad or missing config name\n";
                    return false;
                }
                alarm.descriptor.name = it->value.GetString();

                it = alarmj.FindMember("id");
                if (it == alarmj.MemberEnd() || !it->value.IsUint())
                {
                    std::cerr << "Bad or missing alarm id\n";
                    return false;
                }
                alarm.id = it->value.GetUint();

                it = alarmj.FindMember("low_temperature_watch");
                if (it == alarmj.MemberEnd() || !it->value.IsBool())
                {
                    std::cerr << "Bad or missing alarm low_temperature_watch\n";
                    return false;
                }
                alarm.descriptor.lowTemperatureWatch = it->value.GetBool();

                it = alarmj.FindMember("low_temperature");
                if (it == alarmj.MemberEnd() || !it->value.IsDouble())
                {
                    std::cerr << "Bad or missing alarm low_temperature\n";
                    return false;
                }
                alarm.descriptor.lowTemperature = static_cast<float>(it->value.GetDouble());

                it = alarmj.FindMember("high_temperature_watch");
                if (it == alarmj.MemberEnd() || !it->value.IsBool())
                {
                    std::cerr << "Bad or missing alarm high_temperature_watch\n";
                    return false;
                }
                alarm.descriptor.highTemperatureWatch = it->value.GetBool();

                it = alarmj.FindMember("high_temperature");
                if (it == alarmj.MemberEnd() || !it->value.IsDouble())
                {
                    std::cerr << "Bad or missing alarm high_temperature\n";
                    return false;
                }
                alarm.descriptor.highTemperature = static_cast<float>(it->value.GetDouble());

                it = alarmj.FindMember("low_humidity_watch");
                if (it == alarmj.MemberEnd() || !it->value.IsBool())
                {
                    std::cerr << "Bad or missing alarm low_humidity_watch\n";
                    return false;
                }
                alarm.descriptor.lowHumidityWatch = it->value.GetBool();

                it = alarmj.FindMember("low_humidity");
                if (it == alarmj.MemberEnd() || !it->value.IsDouble())
                {
                    std::cerr << "Bad or missing alarm low_humidity\n";
                    return false;
                }
                alarm.descriptor.lowHumidity = static_cast<float>(it->value.GetDouble());

                it = alarmj.FindMember("high_humidity_watch");
                if (it == alarmj.MemberEnd() || !it->value.IsBool())
                {
                    std::cerr << "Bad or missing alarm high_humidity_watch\n";
                    return false;
                }
                alarm.descriptor.highHumidityWatch = it->value.GetBool();

                it = alarmj.FindMember("high_humidity");
                if (it == alarmj.MemberEnd() || !it->value.IsDouble())
                {
                    std::cerr << "Bad or missing alarm high_humidity\n";
                    return false;
                }
                alarm.descriptor.highHumidity = static_cast<float>(it->value.GetDouble());

                it = alarmj.FindMember("low_vcc_watch");
                if (it == alarmj.MemberEnd() || !it->value.IsBool())
                {
                    std::cerr << "Bad or missing alarm low_vcc_watch\n";
                    return false;
                }
                alarm.descriptor.lowVccWatch = it->value.GetBool();

                it = alarmj.FindMember("low_signal_watch");
                if (it == alarmj.MemberEnd() || !it->value.IsBool())
                {
                    std::cerr << "Bad or missing alarm low_signal_watch\n";
                    return false;
                }
                alarm.descriptor.lowSignalWatch = it->value.GetBool();

                it = alarmj.FindMember("sensor_errors_watch");
                if (it == alarmj.MemberEnd() || !it->value.IsBool())
                {
                    std::cerr << "Bad or missing alarm sensor_errors_watch\n";
                    return false;
                }
                alarm.descriptor.sensorErrorsWatch = it->value.GetBool();

                it = alarmj.FindMember("send_email_action");
                if (it == alarmj.MemberEnd() || !it->value.IsBool())
                {
                    std::cerr << "Bad or missing alarm send_email_action\n";
                    return false;
                }
                alarm.descriptor.sendEmailAction = it->value.GetBool();

                it = alarmj.FindMember("email_recipient");
                if (it == alarmj.MemberEnd() || !it->value.IsString())
                {
                    std::cerr << "Bad or missing config email_recipient\n";
                    return false;
                }
                alarm.descriptor.emailRecipient = it->value.GetString();

                data.alarms.push_back(alarm);
            }
        }

        file.close();
    }


    {
        std::ifstream file(m_dbFilename);
        if (!file.is_open())
        {
            std::cerr << "Failed to open " << m_dbFilename << " file: " << std::strerror(errno) << "\n";
            return false;
        }

        uint32_t sensorCount = 0;
        if (!read(file, sensorCount))
        {
            std::cerr << "Failed to read the DB: " << std::strerror(errno) << "\n";
            return false;
        }
        if (sensorCount > 10000)
        {
            std::cerr << "DB seems to be corrupted\n";
            return false;
        }

        for (uint32_t i = 0; i < sensorCount; i++)
        {
            uint32_t sensorId = 0;
            uint32_t measurementsCount = 0;
            if (!read(file, sensorId) || !read(file, measurementsCount))
            {
                std::cerr << "Failed to read the DB: " << std::strerror(errno) << "\n";
                return false;
            }
            if (measurementsCount > 100000000)
            {
                std::cerr << "DB seems to be corrupted\n";
                return false;
            }

            auto sensorIt = std::find_if(data.sensors.begin(), data.sensors.end(), [&sensorId](Sensor const& sensor) { return sensor.id == sensorId; });
            if (sensorIt == data.sensors.end())
            {
                std::cerr << "Cannot find sensor\n";
                return false;
            }
            Sensor& sensor = *sensorIt;

            StoredMeasurements& storedMeasurements = data.measurements[sensorId];

            storedMeasurements.resize(measurementsCount);
            if (file.read(reinterpret_cast<char*>(storedMeasurements.data()), storedMeasurements.size() * sizeof(StoredMeasurement)).bad())
            {
                std::cerr << "Failed to read the DB: " << std::strerror(errno) << "\n";
                return false;
            }

            uint32_t crc = 0;
            if (!read(file, crc))
            {
                std::cerr << "Failed to read the DB: " << std::strerror(errno) << "\n";
                return false;
            }

            uint32_t computedCrc = crc32(storedMeasurements.data(), storedMeasurements.size() * sizeof(StoredMeasurement));
            if (computedCrc != crc)
            {
                std::cerr << "DB crc failed\n";
                return false;
            }

            for (StoredMeasurement const& sm: storedMeasurements)
            {
                Measurement measurement = unpack(sensorId, sm);
                sensor.isLastMeasurementValid = true;
                sensor.lastMeasurement = measurement;
            }
        }

        file.close();
    }

    m_mainData = data;

    //refresh the sensor triggered alarms
    for (Sensor& sensor: m_mainData.sensors)
    {
        sensor.triggeredAlarms = computeTriggeredAlarm(sensor.id);
    }

    std::cout << "Time to load DB:" << std::chrono::duration<float>(Clock::now() - start).count() << "s\n";
    std::cout.flush();

    return true;
}

//////////////////////////////////////////////////////////////////////////

size_t DB::getSensorCount() const
{
    return m_mainData.sensors.size();
}

//////////////////////////////////////////////////////////////////////////

DB::Sensor const& DB::getSensor(size_t index) const
{
    assert(index < m_mainData.sensors.size());
    return m_mainData.sensors[index];
}

//////////////////////////////////////////////////////////////////////////

bool DB::setConfig(ConfigDescriptor const& descriptor)
{
    if (descriptor.name.empty())
    {
        return false;
    }
    if (descriptor.commsPeriod < std::chrono::seconds(1))
    {
        return false;
    }
    if (descriptor.measurementPeriod < std::chrono::seconds(1))
    {
        return false;
    }
    if (descriptor.commsPeriod < descriptor.measurementPeriod)
    {
        return false;
    }

    Clock::time_point baselineTimePoint = computeBaselineTimePoint(m_mainData.config, descriptor);

    emit configWillBeChanged();

    m_mainData.config.descriptor = descriptor;
    m_mainData.config.baselineTimePoint = baselineTimePoint;

    emit configChanged();

    triggerSave();

    return true;
}

//////////////////////////////////////////////////////////////////////////

DB::Config const& DB::getConfig() const
{
    return m_mainData.config;
}

//////////////////////////////////////////////////////////////////////////

DB::Clock::time_point DB::computeBaselineTimePoint(Config const& oldconfig, ConfigDescriptor const& newDescriptor)
{
    //This is computed when creating the config so that this equation holds for any config:
    // measurement_time_point = config.baseline_time_point + measurement_index * config.measurement_period
    //
    //So when creating a new config, this is how to calculate the baseline:
    // m = some measurement (any)
    // config.baseline_time_point = m.time_point - m.index * config.measurement_period
    //
    //The reason for this is to keep the indices valid in all configs

    Clock::time_point newBaseline;

    bool found = false;
    for (auto const& pair: m_mainData.measurements)
    {
        if (pair.second.empty())
        {
            continue;
        }

        Measurement m = unpack(pair.first, pair.second.back());
        Clock::time_point tp = m.timePoint - m.index * newDescriptor.measurementPeriod;
        if (!found)
        {
            found = true;
        }
        else
        {
            Clock::duration d = tp - newBaseline;

            //std::abs doesn't work with durations
            if (d < Clock::duration::zero())
            {
                d = -d;
            }

            //check for errors
            if (d > std::chrono::minutes(1))
            {
                assert(false);
            }
        }
        newBaseline = tp;
    }

    return found ? newBaseline : oldconfig.baselineTimePoint;
}

//////////////////////////////////////////////////////////////////////////

bool DB::addSensor(SensorDescriptor const& descriptor)
{
    if (findSensorIndexByName(descriptor.name) >= 0)
    {
        return false;
    }

    {
        Sensor sensor;
        sensor.descriptor.name = descriptor.name;
        sensor.id = ++m_lastSensorId;
        sensor.state = Sensor::State::Unbound;

        emit sensorWillBeAdded(sensor.id);
        m_mainData.sensors.push_back(sensor);
        emit sensorAdded(sensor.id);
    }

    {
        Sensor& sensor = m_mainData.sensors.back();
        sensor.triggeredAlarms = computeTriggeredAlarm(sensor.id);
        if (sensor.triggeredAlarms != 0)
        {
            emit sensorTriggeredAlarmsChanged(sensor.id);
        }
    }

    triggerSave();

    return true;
}

//////////////////////////////////////////////////////////////////////////

bool DB::bindSensor(SensorDescriptor const& descriptor, SensorAddress address)
{
    int32_t index = findSensorIndexByName(descriptor.name);
    if (index < 0)
    {
        assert(false);
        return false;
    }
    auto it = std::find_if(m_mainData.sensors.begin(), m_mainData.sensors.end(), [&address](Sensor const& sensor) { return sensor.address == address; });
    if (it != m_mainData.sensors.end())
    {
        assert(false);
        return false;
    }

    Sensor& sensor = m_mainData.sensors[index];

    sensor.address = address;
    sensor.state = Sensor::State::Active;

    emit sensorBound(sensor.id);

    triggerSave();

    return true;
}

//////////////////////////////////////////////////////////////////////////

void DB::removeSensor(size_t index)
{
    assert(index < m_mainData.sensors.size());

    SensorId id = m_mainData.sensors[index].id;

    {
        auto it = m_mainData.measurements.find(id);
        if (it != m_mainData.measurements.end())
        {
            emit measurementsWillBeRemoved(id);

            for (StoredMeasurement const& sm: it->second)
            {
                PrimaryKey pk = computePrimaryKey(id, sm);
                auto sit = std::lower_bound(m_sortedPrimaryKeys.begin(), m_sortedPrimaryKeys.end(), pk);
                if (sit != m_sortedPrimaryKeys.end() && *sit == pk)
                {
                    m_sortedPrimaryKeys.erase(sit);
                }
            }

            m_mainData.measurements.erase(it);
            emit measurementsRemoved(id);
        }
    }

    emit sensorWillBeRemoved(id);
    m_mainData.sensors.erase(m_mainData.sensors.begin() + index);
    emit sensorRemoved(id);

    triggerSave();
}

//////////////////////////////////////////////////////////////////////////

int32_t DB::findSensorIndexByName(std::string const& name) const
{
    auto it = std::find_if(m_mainData.sensors.begin(), m_mainData.sensors.end(), [&name](Sensor const& sensor) { return sensor.descriptor.name == name; });
    if (it == m_mainData.sensors.end())
    {
        return -1;
    }
    return std::distance(m_mainData.sensors.begin(), it);
}

//////////////////////////////////////////////////////////////////////////

int32_t DB::findSensorIndexById(SensorId id) const
{
    auto it = std::find_if(m_mainData.sensors.begin(), m_mainData.sensors.end(), [&id](Sensor const& sensor) { return sensor.id == id; });
    if (it == m_mainData.sensors.end())
    {
        return -1;
    }
    return std::distance(m_mainData.sensors.begin(), it);
}

//////////////////////////////////////////////////////////////////////////

size_t DB::getAlarmCount() const
{
    return m_mainData.alarms.size();
}

//////////////////////////////////////////////////////////////////////////

DB::Alarm const& DB::getAlarm(size_t index) const
{
    assert(index < m_mainData.alarms.size());
    return m_mainData.alarms[index];
}

//////////////////////////////////////////////////////////////////////////

bool DB::addAlarm(AlarmDescriptor const& descriptor)
{
    if (findAlarmIndexByName(descriptor.name) >= 0)
    {
        assert(false);
        return false;
    }
    Alarm alarm;
    alarm.descriptor = descriptor;
    alarm.id = ++m_lastAlarmId;

    emit alarmWillBeAdded(alarm.id);
    m_mainData.alarms.push_back(alarm);
    emit alarmAdded(alarm.id);

    triggerSave();

    return true;
}

//////////////////////////////////////////////////////////////////////////

void DB::removeAlarm(size_t index)
{
    assert(index < m_mainData.alarms.size());
    AlarmId id = m_mainData.alarms[index].id;

    emit alarmWillBeRemoved(id);
    m_mainData.alarms.erase(m_mainData.alarms.begin() + index);
    emit alarmRemoved(id);

    triggerSave();
}

//////////////////////////////////////////////////////////////////////////

int32_t DB::findAlarmIndexByName(std::string const& name) const
{
    auto it = std::find_if(m_mainData.alarms.begin(), m_mainData.alarms.end(), [&name](Alarm const& alarm) { return alarm.descriptor.name == name; });
    if (it == m_mainData.alarms.end())
    {
        return -1;
    }
    return std::distance(m_mainData.alarms.begin(), it);
}

//////////////////////////////////////////////////////////////////////////

uint8_t DB::computeTriggeredAlarm(Measurement const& measurement) const
{
    uint8_t triggered = 0;

    for (Alarm const& alarm: m_mainData.alarms)
    {
        AlarmDescriptor const& descriptor = alarm.descriptor;
        if (descriptor.highTemperatureWatch && measurement.temperature > descriptor.highTemperature)
        {
            triggered |= TriggeredAlarm::Temperature;
        }
        if (descriptor.lowTemperatureWatch && measurement.temperature < descriptor.lowTemperature)
        {
            triggered |= TriggeredAlarm::Temperature;
        }

        if (descriptor.highHumidityWatch && measurement.humidity > descriptor.highHumidity)
        {
            triggered |= TriggeredAlarm::Humidity;
        }
        if (descriptor.lowHumidityWatch && measurement.humidity < descriptor.lowHumidity)
        {
            triggered |= TriggeredAlarm::Humidity;
        }

        if (descriptor.lowVccWatch && measurement.vcc <= k_alertVcc)
        {
            triggered |= TriggeredAlarm::LowVcc;
        }
        if (descriptor.sensorErrorsWatch && measurement.sensorErrors != 0)
        {
            triggered |= TriggeredAlarm::SensorErrors;
        }
        if (descriptor.lowSignalWatch && std::min(measurement.s2b, measurement.b2s) <= k_alertSignal)
        {
            triggered |= TriggeredAlarm::LowSignal;
        }
    }

    return triggered;
}

//////////////////////////////////////////////////////////////////////////

uint8_t DB::computeTriggeredAlarm(SensorId sensorId) const
{
    uint8_t triggered = 0;

    auto it = m_mainData.measurements.find(sensorId);
    if (it == m_mainData.measurements.end())
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

//////////////////////////////////////////////////////////////////////////

bool DB::addMeasurement(Measurement const& measurement)
{
    int32_t sensorIndex = findSensorIndexById(measurement.sensorId);
    if (sensorIndex < 0)
    {
        assert(false);
        return false;
    }

    PrimaryKey pk = computePrimaryKey(measurement);

    //check for duplicates
    auto it = std::lower_bound(m_sortedPrimaryKeys.begin(), m_sortedPrimaryKeys.end(), pk);
    if (it != m_sortedPrimaryKeys.end() && *it == pk)
    {
        assert(false);
        return false;
    }

    emit measurementsWillBeAdded(measurement.sensorId);
    m_mainData.measurements[measurement.sensorId].push_back(pack(measurement));
    m_sortedPrimaryKeys.insert(it, pk);
    emit measurementsAdded(measurement.sensorId);

    Sensor& sensor = m_mainData.sensors[sensorIndex];
    sensor.isLastMeasurementValid = true;
    sensor.lastMeasurement = measurement;

    emit sensorMeasurementChanged(sensor.id);

    uint8_t triggeredAlarms = sensor.triggeredAlarms;
    sensor.triggeredAlarms |= computeTriggeredAlarm(measurement);
    if (sensor.triggeredAlarms != triggeredAlarms)
    {
        emit sensorTriggeredAlarmsChanged(sensor.id);
    }

    triggerSave();

    return true;
}

//////////////////////////////////////////////////////////////////////////

std::vector<DB::Measurement> DB::getAllMeasurements() const
{
    std::vector<DB::Measurement> result;
    result.reserve(8192);

    for (auto const& pair : m_mainData.measurements)
    {
        for (StoredMeasurement const& sm: pair.second)
        {
            result.push_back(unpack(pair.first, sm));
        }
    }
    return result;
}

//////////////////////////////////////////////////////////////////////////

size_t DB::getAllMeasurementCount() const
{
    size_t count = 0;
    for (auto const& pair : m_mainData.measurements)
    {
        count += pair.second.size();
    }
    return count;
}

//////////////////////////////////////////////////////////////////////////

std::vector<DB::Measurement> DB::getFilteredMeasurements(Filter const& filter) const
{
    std::vector<DB::Measurement> result;
    result.reserve(8192);

    for (auto const& pair : m_mainData.measurements)
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

//////////////////////////////////////////////////////////////////////////

size_t DB::getFilteredMeasurementCount(Filter const& filter) const
{
    size_t count = 0;
    for (auto const& pair : m_mainData.measurements)
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

//////////////////////////////////////////////////////////////////////////

bool DB::getLastMeasurementForSensor(SensorId sensor_id, Measurement& measurement) const
{
    auto it = m_mainData.measurements.find(sensor_id);
    if (it == m_mainData.measurements.end())
    {
        assert(false);
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

//////////////////////////////////////////////////////////////////////////

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
    if (filter.useSensorErrorsFilter)
    {
        bool has_errors = measurement.sensorErrors != 0;
        if (has_errors != filter.sensorErrorsFilter)
        {
            return false;
        }
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////

inline DB::StoredMeasurement DB::pack(Measurement const& m)
{
    StoredMeasurement sm;
    sm.index = m.index;
    sm.timePoint = Clock::to_time_t(m.timePoint);
    sm.temperature = static_cast<int16_t>(std::max(std::min(m.temperature, 320.f), -320.f) * 100.f);
    sm.humidity = static_cast<int16_t>(std::max(std::min(m.humidity, 320.f), -320.f) * 100.f);
    sm.vcc = static_cast<uint8_t>(std::max(std::min((m.vcc - 2.f), 2.55f), 0.f) * 100.f);
    sm.b2s = m.b2s;
    sm.s2b = m.s2b;
    sm.sensorErrors = m.sensorErrors;
    return sm;
}

//////////////////////////////////////////////////////////////////////////

inline DB::Measurement DB::unpack(SensorId sensor_id, StoredMeasurement const& sm)
{
    Measurement m;
    m.sensorId = sensor_id;
    m.index = sm.index;
    m.timePoint = Clock::from_time_t(sm.timePoint);
    m.temperature = static_cast<float>(sm.temperature) / 100.f;
    m.humidity = static_cast<float>(sm.humidity) / 100.f;
    m.vcc = static_cast<float>(sm.vcc) / 100.f + 2.f;
    m.b2s = sm.b2s;
    m.s2b = sm.s2b;
    m.sensorErrors = sm.sensorErrors;
    return m;
}

//////////////////////////////////////////////////////////////////////////

inline DB::PrimaryKey DB::computePrimaryKey(Measurement const& m)
{
    return (PrimaryKey(m.sensorId) << 32) | PrimaryKey(m.index);
}

//////////////////////////////////////////////////////////////////////////

inline DB::PrimaryKey DB::computePrimaryKey(SensorId sensor_id, StoredMeasurement const& m)
{
    return (PrimaryKey(sensor_id) << 32) | PrimaryKey(m.index);
}

//////////////////////////////////////////////////////////////////////////

void DB::triggerSave()
{
    {
        std::unique_lock<std::mutex> lg(m_storeMutex);
        m_storeData = m_mainData;
        m_saveTriggered = true;
    }
    m_storeCV.notify_all();

    std::cout << "Save triggered\n";
}

//////////////////////////////////////////////////////////////////////////

void DB::save(Data const& data) const
{
    Clock::time_point start = Clock::now();

    {
        rapidjson::Document document;
        document.SetObject();
        {
            rapidjson::Value configj;
            configj.SetObject();
            configj.AddMember("name", rapidjson::Value(data.config.descriptor.name.c_str(), document.GetAllocator()), document.GetAllocator());
            configj.AddMember("sensors_sleeping", data.config.descriptor.sensorsSleeping, document.GetAllocator());
            configj.AddMember("measurement_period", static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(data.config.descriptor.measurementPeriod).count()), document.GetAllocator());
            configj.AddMember("comms_period", static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(data.config.descriptor.commsPeriod).count()), document.GetAllocator());
            configj.AddMember("baseline", static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(data.config.baselineTimePoint.time_since_epoch()).count()), document.GetAllocator());
            document.AddMember("config", configj, document.GetAllocator());
        }

        {
            rapidjson::Value sensorsj;
            sensorsj.SetArray();
            for (Sensor const& sensor: data.sensors)
            {
                rapidjson::Value sensorj;
                sensorj.SetObject();
                sensorj.AddMember("name", rapidjson::Value(sensor.descriptor.name.c_str(), document.GetAllocator()), document.GetAllocator());
                sensorj.AddMember("id", sensor.id, document.GetAllocator());
                sensorj.AddMember("address", sensor.address, document.GetAllocator());
                sensorj.AddMember("state", static_cast<int>(sensor.state), document.GetAllocator());
                sensorj.AddMember("next_measurement", static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(sensor.nextMeasurementTimePoint.time_since_epoch()).count()), document.GetAllocator());
                sensorj.AddMember("next_comms", static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(sensor.nextCommsTimePoint.time_since_epoch()).count()), document.GetAllocator());
                sensorsj.PushBack(sensorj, document.GetAllocator());
            }
            document.AddMember("sensors", sensorsj, document.GetAllocator());
        }

        {
            rapidjson::Value alarmsj;
            alarmsj.SetArray();
            for (Alarm const& alarm: data.alarms)
            {
                rapidjson::Value alarmj;
                alarmj.SetObject();
                alarmj.AddMember("name", rapidjson::Value(alarm.descriptor.name.c_str(), document.GetAllocator()), document.GetAllocator());
                alarmj.AddMember("id", alarm.id, document.GetAllocator());
                alarmj.AddMember("low_temperature_watch", alarm.descriptor.lowTemperatureWatch, document.GetAllocator());
                alarmj.AddMember("low_temperature", alarm.descriptor.lowTemperature, document.GetAllocator());
                alarmj.AddMember("high_temperature_watch", alarm.descriptor.highTemperatureWatch, document.GetAllocator());
                alarmj.AddMember("high_temperature", alarm.descriptor.highTemperature, document.GetAllocator());
                alarmj.AddMember("low_humidity_watch", alarm.descriptor.lowHumidityWatch, document.GetAllocator());
                alarmj.AddMember("low_humidity", alarm.descriptor.lowHumidity, document.GetAllocator());
                alarmj.AddMember("high_humidity_watch", alarm.descriptor.highHumidityWatch, document.GetAllocator());
                alarmj.AddMember("high_humidity", alarm.descriptor.highHumidity, document.GetAllocator());
                alarmj.AddMember("low_vcc_watch", alarm.descriptor.lowVccWatch, document.GetAllocator());
                alarmj.AddMember("low_signal_watch", alarm.descriptor.lowSignalWatch, document.GetAllocator());
                alarmj.AddMember("sensor_errors_watch", alarm.descriptor.sensorErrorsWatch, document.GetAllocator());
                alarmj.AddMember("send_email_action", alarm.descriptor.sendEmailAction, document.GetAllocator());
                alarmj.AddMember("email_recipient", rapidjson::Value(alarm.descriptor.emailRecipient.c_str(), document.GetAllocator()), document.GetAllocator());
                alarmsj.PushBack(alarmj, document.GetAllocator());
            }
            document.AddMember("alarms", alarmsj, document.GetAllocator());
        }

        std::string tempFileName = m_dataFilename + "_temp";
        std::ofstream file(tempFileName);
        if (!file.is_open())
        {
            std::cerr << "Failed to open " << tempFileName << " file: " << std::strerror(errno) << "\n";
        }
        else
        {
            rapidjson::BasicOStreamWrapper<std::ofstream> buffer{file};
            //rapidjson::Writer<rapidjson::BasicOStreamWrapper<std::ofstream>> writer(buffer);
            rapidjson::PrettyWriter<rapidjson::BasicOStreamWrapper<std::ofstream>> writer(buffer);
            document.Accept(writer);
        }
        file.close();

        if (!renameFile(tempFileName.c_str(), m_dataFilename.c_str()))
        {
            std::perror("Error renaming");
        }
    }


//    {
//        rapidjson::Document document;
//        document.SetArray();

//        for (auto const& pair: data.measurements)
//        {
//            rapidjson::Value measurementsPerSensorj;
//            measurementsPerSensorj.SetObject();

//            rapidjson::Value measurementsj;
//            measurementsj.SetArray();
//            for (StoredMeasurement const& sm: pair.second)
//            {
//                rapidjson::Value mj;
//                mj.SetArray();
//                mj.PushBack(sm.index, document.GetAllocator());
//                mj.PushBack(static_cast<uint64_t>(sm.timePoint), document.GetAllocator());
//                mj.PushBack(sm.temperature, document.GetAllocator());
//                mj.PushBack(sm.humidity, document.GetAllocator());
//                mj.PushBack(sm.vcc, document.GetAllocator());
//                mj.PushBack(sm.b2s, document.GetAllocator());
//                mj.PushBack(sm.s2b, document.GetAllocator());
//                mj.PushBack(sm.sensorErrors, document.GetAllocator());
//                measurementsj.PushBack(mj, document.GetAllocator());
//            }
//            measurementsPerSensorj.AddMember("id", pair.first, document.GetAllocator());
//            measurementsPerSensorj.AddMember("m", std::move(measurementsj), document.GetAllocator());

//            document.PushBack(std::move(measurementsPerSensorj), document.GetAllocator());
//        }

//        std::string tempFileName = m_dbFilename + "_temp";
//        std::ofstream file(tempFileName);
//        if (!file.is_open())
//        {
//            std::cerr << "Failed to open " << tempFileName << " file: " << std::strerror(errno) << "\n";
//        }
//        else
//        {
//            rapidjson::BasicOStreamWrapper<std::ofstream> buffer{file};
//            rapidjson::Writer<rapidjson::BasicOStreamWrapper<std::ofstream>> writer(buffer);
//            //rapidjson::PrettyWriter<rapidjson::BasicOStreamWrapper<std::ofstream>> writer(buffer);
//            document.Accept(writer);
//        }
//        file.close();

//        if (!renameFile(tempFileName.c_str(), m_dbFilename.c_str()))
//        {
//            std::perror("Error renaming");
//        }
//    }
    {
        std::string tempFileName = m_dbFilename + "_temp";
        std::ofstream file(tempFileName, std::ios_base::binary);
        if (!file.is_open())
        {
            std::cerr << "Failed to open " << tempFileName << " file: " << std::strerror(errno) << "\n";
        }
        else
        {
            write(file, static_cast<uint32_t>(data.measurements.size()));
            for (auto const& pair: data.measurements)
            {
                write(file, static_cast<uint32_t>(pair.first));
                write(file, static_cast<uint32_t>(pair.second.size()));
                file.write(reinterpret_cast<const char*>(pair.second.data()), pair.second.size() * sizeof(StoredMeasurement));
                uint32_t crc = crc32(pair.second.data(), pair.second.size() * sizeof(StoredMeasurement));
                write(file, crc);
            }
            file.close();

            if (!renameFile(tempFileName.c_str(), m_dbFilename.c_str()))
            {
                std::perror("Error renaming");
            }
        }
    }

    std::cout << "Time to save DB:" << std::chrono::duration<float>(Clock::now() - start).count() << "s\n";
    std::cout.flush();
}

//////////////////////////////////////////////////////////////////////////

void DB::storeThreadProc()
{
    while (!m_storeThreadExit)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        Data data;
        {
            //wait for data
            std::unique_lock<std::mutex> lg(m_storeMutex);
            if (!m_saveTriggered)
            {
                m_storeCV.wait(lg, [this]{ return m_saveTriggered || m_storeThreadExit; });
            }
            if (m_storeThreadExit)
            {
                break;
            }

            data = std::move(m_storeData);
            m_storeData = Data();
            m_saveTriggered = false;
        }
        save(data);
    }
}

//////////////////////////////////////////////////////////////////////////


