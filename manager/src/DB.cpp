#include "DB.h"
#include <algorithm>
#include <functional>
#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>
#include <QApplication>
#include <QDateTime>
#include <QDir>

#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/writer.h"
#include "rapidjson/reader.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/ostreamwrapper.h"
#include "rapidjson/istreamwrapper.h"
#include "rapidjson/error/en.h"

#include "Utils.h"
#include "Crypt.h"
#include "Logger.h"

extern Logger s_logger;

extern std::string s_dataFolder;

constexpr float k_alertVcc = 2.2f;
constexpr int8_t k_alertSignal = -110;

constexpr uint64_t k_fileEncryptionKey = 4353123543565ULL;

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
    m_threadsExit = true;

    m_storeCV.notify_all();
    if (m_storeThread.joinable())
    {
        m_storeThread.join();
    }
}

//////////////////////////////////////////////////////////////////////////

bool DB::create(std::string const& name)
{
    m_dbName = "sense-" + name + ".db";
    m_dataName = "sense-" + name + ".data";
    m_mainData.measurements.clear();

    std::string dataFilename = s_dataFolder + "/" + m_dataName;
    std::string dbFilename = s_dataFolder + "/" + m_dbName;

    moveToBackup(m_dbName, dbFilename, s_dataFolder + "/backups/deleted", 50);
    moveToBackup(m_dataName, dataFilename, s_dataFolder + "/backups/deleted", 50);

    remove((dbFilename).c_str());
    remove((dataFilename).c_str());

    m_mainData = Data();
    save(m_mainData);

    s_logger.logVerbose(QString("Creating DB files: '%1' & '%2'").arg(m_dataName.c_str()).arg(m_dbName.c_str()));

    return true;
}

//////////////////////////////////////////////////////////////////////////

bool DB::load(std::string const& name)
{
    Clock::time_point start = Clock::now();

    m_dbName = "sense-" + name + ".db";
    m_dataName = "sense-" + name + ".data";

    std::string dataFilename = (s_dataFolder + "/" + m_dataName);
    std::string dbFilename = (s_dataFolder + "/" + m_dbName);


    Data data;

    {
        std::string streamData;
        {
            std::ifstream file(dataFilename, std::ios_base::binary);
            if (!file.is_open())
            {
                s_logger.logCritical(QString("Failed to open '%1': %2").arg(dataFilename.c_str()).arg(std::strerror(errno)));
                return false;
            }

            streamData = std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            file.close();
        }

        Crypt crypt;
        crypt.setKey(k_fileEncryptionKey);
        QByteArray decryptedData = crypt.decryptToByteArray(QByteArray(streamData.data(), streamData.size()));

        rapidjson::Document document;
        document.Parse(decryptedData.data(), decryptedData.size());
        if (document.GetParseError() != rapidjson::ParseErrorCode::kParseErrorNone)
        {
            document.Parse(streamData.data(), streamData.size());
            if (document.GetParseError() != rapidjson::ParseErrorCode::kParseErrorNone)
            {
                s_logger.logCritical(QString("Failed to open '%1': %2").arg(dataFilename.c_str()).arg(rapidjson::GetParseError_En(document.GetParseError())));
                return false;
            }
        }

        if (!document.IsObject())
        {
            s_logger.logCritical(QString("Failed to load '%1': Bad document").arg(dataFilename.c_str()));
            return false;
        }

        auto it = document.FindMember("sensor_settings");
        if (it == document.MemberEnd() || !it->value.IsObject())
        {
            s_logger.logCritical(QString("Failed to load '%1': Bad or missing sensor settings object").arg(dataFilename.c_str()));
            return false;
        }

        {
            rapidjson::Value const& ssj = it->value;
            auto it = ssj.FindMember("name");
            if (it == ssj.MemberEnd() || !it->value.IsString())
            {
                s_logger.logCritical(QString("Failed to load '%1': Bad or missing sensor settings name").arg(dataFilename.c_str()));
                return false;
            }
            data.sensorSettings.descriptor.name = it->value.GetString();

            it = ssj.FindMember("sensors_sleeping");
            if (it == ssj.MemberEnd() || !it->value.IsBool())
            {
                s_logger.logCritical(QString("Failed to load '%1': Bad or missing sensor settings sensors_sleeping").arg(dataFilename.c_str()));
                return false;
            }
            data.sensorSettings.descriptor.sensorsSleeping = it->value.GetBool();

            it = ssj.FindMember("measurement_period");
            if (it == ssj.MemberEnd() || !it->value.IsUint64())
            {
                s_logger.logCritical(QString("Failed to load '%1': Bad or missing sensor settings measurement_period").arg(dataFilename.c_str()));
                return false;
            }
            data.sensorSettings.descriptor.measurementPeriod = std::chrono::seconds(it->value.GetUint64());

            it = ssj.FindMember("comms_period");
            if (it == ssj.MemberEnd() || !it->value.IsUint64())
            {
                s_logger.logCritical(QString("Failed to load '%1': Bad or missing sensor settings comms_period").arg(dataFilename.c_str()));
                return false;
            }
            data.sensorSettings.descriptor.commsPeriod = std::chrono::seconds(it->value.GetUint64());
            data.sensorSettings.computedCommsPeriod = data.sensorSettings.descriptor.commsPeriod;

            it = ssj.FindMember("baseline");
            if (it == ssj.MemberEnd() || !it->value.IsUint64())
            {
                s_logger.logCritical(QString("Failed to load '%1': Bad or missing sensor settings baseline").arg(dataFilename.c_str()));
                return false;
            }
            data.sensorSettings.baselineTimePoint = Clock::from_time_t(it->value.GetUint64());
        }

        it = document.FindMember("sensors");
        if (it == document.MemberEnd() || !it->value.IsArray())
        {
            s_logger.logCritical(QString("Failed to load '%1': Bad or missing sensors array").arg(dataFilename.c_str()));
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
                    s_logger.logCritical(QString("Failed to load '%1': Bad or missing sensor name").arg(dataFilename.c_str()));
                    return false;
                }
                sensor.descriptor.name = it->value.GetString();

                it = sensorj.FindMember("id");
                if (it == sensorj.MemberEnd() || !it->value.IsUint())
                {
                    s_logger.logCritical(QString("Failed to load '%1': Bad or missing sensor id").arg(dataFilename.c_str()));
                    return false;
                }
                sensor.id = it->value.GetUint();

                it = sensorj.FindMember("address");
                if (it == sensorj.MemberEnd() || !it->value.IsUint())
                {
                    s_logger.logCritical(QString("Failed to load '%1': Bad or missing sensor address").arg(dataFilename.c_str()));
                    return false;
                }
                sensor.address = it->value.GetUint();

                it = sensorj.FindMember("state");
                if (it == sensorj.MemberEnd() || !it->value.IsUint())
                {
                    s_logger.logCritical(QString("Failed to load '%1': Bad or missing sensor state").arg(dataFilename.c_str()));
                    return false;
                }
                sensor.state = static_cast<Sensor::State>(it->value.GetUint());

                it = sensorj.FindMember("serial_number");
                if (it == sensorj.MemberEnd() || !it->value.IsUint())
                {
                    s_logger.logCritical(QString("Failed to load '%1': Bad or missing sensor serial_number").arg(dataFilename.c_str()));
                    return false;
                }
                sensor.serialNumber = it->value.GetUint();

                it = sensorj.FindMember("next_measurement");
                if (it == sensorj.MemberEnd() || !it->value.IsUint64())
                {
                    s_logger.logCritical(QString("Failed to load '%1': Bad or missing sensor next_measurement").arg(dataFilename.c_str()));
                    return false;
                }
                sensor.nextMeasurementTimePoint = Clock::from_time_t(it->value.GetUint64());

                it = sensorj.FindMember("last_comms");
                if (it != sensorj.MemberEnd() && it->value.IsUint64())
                {
                    if (!it->value.IsUint64())
                    {
                        s_logger.logCritical(QString("Failed to load '%1': Bad or missing sensor next_comms").arg(dataFilename.c_str()));
                        return false;
                    }
                    sensor.lastCommsTimePoint = Clock::from_time_t(it->value.GetUint64());
                }

                it = sensorj.FindMember("temperature_bias");
                if (it == sensorj.MemberEnd() || !it->value.IsNumber())
                {
                    s_logger.logCritical(QString("Failed to load '%1': Bad or missing sensor temperature_bias").arg(dataFilename.c_str()));
                    return false;
                }
                sensor.calibration.temperatureBias = static_cast<float>(it->value.GetDouble());

                it = sensorj.FindMember("humidity_bias");
                if (it == sensorj.MemberEnd() || !it->value.IsNumber())
                {
                    s_logger.logCritical(QString("Failed to load '%1': Bad or missing sensor humidity_bias").arg(dataFilename.c_str()));
                    return false;
                }
                sensor.calibration.humidityBias = static_cast<float>(it->value.GetDouble());

                data.lastSensorId = std::max(data.lastSensorId, sensor.id);
                data.sensors.push_back(sensor);
            }
        }

        it = document.FindMember("alarms");
        if (it == document.MemberEnd() || !it->value.IsArray())
        {
            s_logger.logCritical(QString("Failed to load '%1': Bad or missing alarms array").arg(dataFilename.c_str()));
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
                    s_logger.logCritical(QString("Failed to load '%1': Bad or missing alarm name").arg(dataFilename.c_str()));
                    return false;
                }
                alarm.descriptor.name = it->value.GetString();

                it = alarmj.FindMember("id");
                if (it == alarmj.MemberEnd() || !it->value.IsUint())
                {
                    s_logger.logCritical(QString("Failed to load '%1': Bad or missing alarm id").arg(dataFilename.c_str()));
                    return false;
                }
                alarm.id = it->value.GetUint();

                it = alarmj.FindMember("filter_sensors");
                if (it != alarmj.MemberEnd() && !it->value.IsBool())
                {
                    s_logger.logCritical(QString("Failed to load '%1': Bad alarm filter_sensors").arg(dataFilename.c_str()));
                    return false;
                }
                if (it != alarmj.MemberEnd())
                {
                    alarm.descriptor.filterSensors = it->value.GetBool();
                }

                if (alarm.descriptor.filterSensors)
                {
                    it = alarmj.FindMember("sensors");
                    if (it == alarmj.MemberEnd() || !it->value.IsArray())
                    {
                        s_logger.logCritical(QString("Failed to load '%1': Bad or missing alarm sensors").arg(dataFilename.c_str()));
                        return false;
                    }
                    rapidjson::Value const& sensorsj = it->value;
                    for (size_t si = 0; si < sensorsj.Size(); si++)
                    {
                        rapidjson::Value const& sensorj = sensorsj[si];
                        if (!sensorj.IsUint())
                        {
                            s_logger.logCritical(QString("Failed to load '%1': Bad or missing alarm sensor id").arg(dataFilename.c_str()));
                            return false;
                        }
                        alarm.descriptor.sensors.push_back(sensorj.GetUint());
                    }
                }


                it = alarmj.FindMember("low_temperature_watch");
                if (it == alarmj.MemberEnd() || !it->value.IsBool())
                {
                    s_logger.logCritical(QString("Failed to load '%1': Bad or missing alarm low_temperature_watch").arg(dataFilename.c_str()));
                    return false;
                }
                alarm.descriptor.lowTemperatureWatch = it->value.GetBool();

                it = alarmj.FindMember("low_temperature");
                if (it == alarmj.MemberEnd() || !it->value.IsDouble())
                {
                    s_logger.logCritical(QString("Failed to load '%1': Bad or missing alarm low_temperature").arg(dataFilename.c_str()));
                    return false;
                }
                alarm.descriptor.lowTemperature = static_cast<float>(it->value.GetDouble());

                it = alarmj.FindMember("high_temperature_watch");
                if (it == alarmj.MemberEnd() || !it->value.IsBool())
                {
                    s_logger.logCritical(QString("Failed to load '%1': Bad or missing alarm high_temperature_watch").arg(dataFilename.c_str()));
                    return false;
                }
                alarm.descriptor.highTemperatureWatch = it->value.GetBool();

                it = alarmj.FindMember("high_temperature");
                if (it == alarmj.MemberEnd() || !it->value.IsDouble())
                {
                    s_logger.logCritical(QString("Failed to load '%1': Bad or missing alarm high_temperature").arg(dataFilename.c_str()));
                    return false;
                }
                alarm.descriptor.highTemperature = static_cast<float>(it->value.GetDouble());

                it = alarmj.FindMember("low_humidity_watch");
                if (it == alarmj.MemberEnd() || !it->value.IsBool())
                {
                    s_logger.logCritical(QString("Failed to load '%1': Bad or missing alarm low_humidity_watch").arg(dataFilename.c_str()));
                    return false;
                }
                alarm.descriptor.lowHumidityWatch = it->value.GetBool();

                it = alarmj.FindMember("low_humidity");
                if (it == alarmj.MemberEnd() || !it->value.IsDouble())
                {
                    s_logger.logCritical(QString("Failed to load '%1': Bad or missing alarm low_humidity").arg(dataFilename.c_str()));
                    return false;
                }
                alarm.descriptor.lowHumidity = static_cast<float>(it->value.GetDouble());

                it = alarmj.FindMember("high_humidity_watch");
                if (it == alarmj.MemberEnd() || !it->value.IsBool())
                {
                    s_logger.logCritical(QString("Failed to load '%1': Bad or missing alarm high_humidity_watch").arg(dataFilename.c_str()));
                    return false;
                }
                alarm.descriptor.highHumidityWatch = it->value.GetBool();

                it = alarmj.FindMember("high_humidity");
                if (it == alarmj.MemberEnd() || !it->value.IsDouble())
                {
                    s_logger.logCritical(QString("Failed to load '%1': Bad or missing alarm high_humidity").arg(dataFilename.c_str()));
                    return false;
                }
                alarm.descriptor.highHumidity = static_cast<float>(it->value.GetDouble());

                it = alarmj.FindMember("low_vcc_watch");
                if (it == alarmj.MemberEnd() || !it->value.IsBool())
                {
                    s_logger.logCritical(QString("Failed to load '%1': Bad or missing alarm low_vcc_watch").arg(dataFilename.c_str()));
                    return false;
                }
                alarm.descriptor.lowVccWatch = it->value.GetBool();

                it = alarmj.FindMember("low_signal_watch");
                if (it == alarmj.MemberEnd() || !it->value.IsBool())
                {
                    s_logger.logCritical(QString("Failed to load '%1': Bad or missing alarm low_signal_watch").arg(dataFilename.c_str()));
                    return false;
                }
                alarm.descriptor.lowSignalWatch = it->value.GetBool();

//                it = alarmj.FindMember("sensor_errors_watch");
//                if (it == alarmj.MemberEnd() || !it->value.IsBool())
//                {
//                    s_logger.logCritical(QString("Failed to load '%1': Bad or missing alarm sensor_errors_watch").arg(dataFilename.c_str()));
//                    return false;
//                }
//                alarm.descriptor.sensorErrorsWatch = it->value.GetBool();

                it = alarmj.FindMember("send_email_action");
                if (it == alarmj.MemberEnd() || !it->value.IsBool())
                {
                    s_logger.logCritical(QString("Failed to load '%1': Bad or missing alarm send_email_action").arg(dataFilename.c_str()));
                    return false;
                }
                alarm.descriptor.sendEmailAction = it->value.GetBool();

                data.lastAlarmId = std::max(data.lastAlarmId, alarm.id);
                data.alarms.push_back(alarm);
            }
        }


        it = document.FindMember("reports");
        if (it == document.MemberEnd() || !it->value.IsArray())
        {
            s_logger.logCritical(QString("Failed to load '%1': Bad or missing reports array").arg(dataFilename.c_str()));
            return false;
        }

        {
            rapidjson::Value const& reportsj = it->value;
            for (size_t i = 0; i < reportsj.Size(); i++)
            {
                Report report;
                rapidjson::Value const& reportj = reportsj[i];
                auto it = reportj.FindMember("name");
                if (it == reportj.MemberEnd() || !it->value.IsString())
                {
                    s_logger.logCritical(QString("Failed to load '%1': Bad or missing report name").arg(dataFilename.c_str()));
                    return false;
                }
                report.descriptor.name = it->value.GetString();

                it = reportj.FindMember("id");
                if (it == reportj.MemberEnd() || !it->value.IsUint())
                {
                    s_logger.logCritical(QString("Failed to load '%1': Bad or missing report id").arg(dataFilename.c_str()));
                    return false;
                }
                report.id = it->value.GetUint();

                it = reportj.FindMember("last_triggered");
                if (it == reportj.MemberEnd() || !it->value.IsUint64())
                {
                    s_logger.logCritical(QString("Failed to load '%1': Bad or missing sensor report last_triggered").arg(dataFilename.c_str()));
                    return false;
                }
                report.lastTriggeredTimePoint = Clock::from_time_t(it->value.GetUint64());

                it = reportj.FindMember("filter_sensors");
                if (it == reportj.MemberEnd() || !it->value.IsBool())
                {
                    s_logger.logCritical(QString("Failed to load '%1': Bad report filter_sensors").arg(dataFilename.c_str()));
                    return false;
                }
                report.descriptor.filterSensors = it->value.GetBool();

                if (report.descriptor.filterSensors)
                {
                    it = reportj.FindMember("sensors");
                    if (it == reportj.MemberEnd() || !it->value.IsArray())
                    {
                        s_logger.logCritical(QString("Failed to load '%1': Bad or missing report sensors").arg(dataFilename.c_str()));
                        return false;
                    }
                    rapidjson::Value const& sensorsj = it->value;
                    for (size_t si = 0; si < sensorsj.Size(); si++)
                    {
                        rapidjson::Value const& sensorj = sensorsj[si];
                        if (!sensorj.IsUint())
                        {
                            s_logger.logCritical(QString("Failed to load '%1': Bad or missing report sensor id").arg(dataFilename.c_str()));
                            return false;
                        }
                        report.descriptor.sensors.push_back(sensorj.GetUint());
                    }
                }

                it = reportj.FindMember("period");
                if (it == reportj.MemberEnd() || !it->value.IsInt())
                {
                    s_logger.logCritical(QString("Failed to load '%1': Bad or missing report period").arg(dataFilename.c_str()));
                    return false;
                }
                report.descriptor.period = static_cast<DB::ReportDescriptor::Period>(it->value.GetInt());

                it = reportj.FindMember("custom_period");
                if (it == reportj.MemberEnd() || !it->value.IsUint64())
                {
                    s_logger.logCritical(QString("Failed to load '%1': Bad or missing report custom_period").arg(dataFilename.c_str()));
                    return false;
                }
                report.descriptor.customPeriod = std::chrono::seconds(it->value.GetUint64());

                it = reportj.FindMember("data");
                if (it == reportj.MemberEnd() || !it->value.IsInt())
                {
                    s_logger.logCritical(QString("Failed to load '%1': Bad or missing report data").arg(dataFilename.c_str()));
                    return false;
                }
                report.descriptor.data = static_cast<DB::ReportDescriptor::Data>(it->value.GetInt());

                data.lastReportId = std::max(data.lastReportId, report.id);
                data.reports.push_back(report);
            }
        }
    }


    {
        std::string streamData;
        {
            std::ifstream file(dbFilename, std::ios_base::binary);
            if (!file.is_open())
            {
                s_logger.logCritical(QString("Failed to open '%1': %2").arg(dbFilename.c_str()).arg(std::strerror(errno)));
                return false;
            }

            streamData = std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            file.close();
        }

        Crypt crypt;
        crypt.setKey(k_fileEncryptionKey);
        QByteArray decryptedData = crypt.decryptToByteArray(QByteArray(streamData.data(), streamData.size()));

        std::stringstream stream(std::string(decryptedData.data(), decryptedData.size()));

        uint32_t sensorCount = 0;
        if (!read(stream, sensorCount))
        {
            s_logger.logCritical(QString("Failed to read '%1': %2").arg(dbFilename.c_str()).arg(std::strerror(errno)));
            return false;
        }
        if (sensorCount > 10000)
        {
            s_logger.logCritical(QString("Failed to read '%1': file seems to be corrupted").arg(dbFilename.c_str()));
            return false;
        }

        for (uint32_t i = 0; i < sensorCount; i++)
        {
            uint32_t sensorId = 0;
            uint32_t measurementsCount = 0;
            if (!read(stream, sensorId) || !read(stream, measurementsCount))
            {
                s_logger.logCritical(QString("Failed to read '%1': %2").arg(dbFilename.c_str()).arg(std::strerror(errno)));
                return false;
            }
            if (measurementsCount > 100000000)
            {
                s_logger.logCritical(QString("Failed to read '%1': file seems to be corrupted").arg(dbFilename.c_str()));
                return false;
            }

            auto sensorIt = std::find_if(data.sensors.begin(), data.sensors.end(), [&sensorId](Sensor const& sensor) { return sensor.id == sensorId; });
            if (sensorIt == data.sensors.end())
            {
                s_logger.logCritical(QString("Failed to read '%1': cannot find sensor").arg(dbFilename.c_str()));
                return false;
            }
            Sensor& sensor = *sensorIt;

            StoredMeasurements& storedMeasurements = data.measurements[sensorId];

            size_t recordSize = sizeof(StoredMeasurement);
            storedMeasurements.resize(measurementsCount);
            if (stream.read(reinterpret_cast<char*>(storedMeasurements.data()), storedMeasurements.size() * recordSize).bad())
            {
                s_logger.logCritical(QString("Failed to read '%1': %2").arg(dbFilename.c_str()).arg(std::strerror(errno)));
                return false;
            }

            std::sort(storedMeasurements.begin(), storedMeasurements.end(), [](StoredMeasurement const& a, StoredMeasurement const& b)
            {
                return a.timePoint < b.timePoint;
            });

            for (auto it = storedMeasurements.begin(); it != storedMeasurements.end();)
            {
                StoredMeasurement& sm = *it;
                Measurement measurement = unpack(sensorId, sm);
                MeasurementId id = computeMeasurementId(measurement.descriptor);

//                std::cout << "index: " << std::to_string(measurement.descriptor.index) << " s: " << std::to_string(measurement.descriptor.sensorId) << "\n";


                //check for duplicates
                auto lbit = std::lower_bound(data.sortedMeasurementIds.begin(), data.sortedMeasurementIds.end(), id);
                if (lbit != data.sortedMeasurementIds.end() && *lbit == id)
                {
                    s_logger.logWarning(QString("Duplicate measurement index %1 found. Deleting it").arg(sm.index));
                    storedMeasurements.erase(it);
                }
                else
                {
                    data.sortedMeasurementIds.insert(lbit, id);
                    sensor.isLastMeasurementValid = true;
                    sensor.lastMeasurement = measurement;
                    ++it;
                }
            }

            if (!storedMeasurements.empty())
            {
                Measurement measurement = unpack(sensorId, storedMeasurements.back());
                sensor.isLastMeasurementValid = true;
                sensor.lastMeasurement = measurement;
            }
        }
    }

    //refresh signal strengths
    for (Sensor& sensor: data.sensors)
    {
        sensor.averageSignalStrength = computeAverageSignalStrength(sensor.id, data);
    }

    std::flush(std::cout);

    std::pair<std::string, time_t> bkf = getMostRecentBackup(dbFilename, s_dataFolder + "/backups/daily");
    if (bkf.first.empty())
    {
        m_lastDailyBackupTP = DB::Clock::now();
    }
    else
    {
        m_lastDailyBackupTP = DB::Clock::from_time_t(bkf.second);
    }
    bkf = getMostRecentBackup(dbFilename, s_dataFolder + "/backups/weekly");
    if (bkf.first.empty())
    {
        m_lastWeeklyBackupTP = DB::Clock::now();
    }
    else
    {
        m_lastWeeklyBackupTP = DB::Clock::from_time_t(bkf.second);
    }

    m_mainData = data;

    s_logger.logVerbose(QString("Done loading DB from '%1' & '%2'. Time: %3s").arg(m_dataName.c_str()).arg(m_dbName.c_str()).arg(std::chrono::duration<float>(Clock::now() - start).count()));

    return true;
}

//void DB::test()
//{
//    for (StoredMeasurement const& sm: m_mainData.measurements.begin()->second)
//    {
//        Measurement measurement = unpack(m_mainData.measurements.begin()->first, sm);
//        computeTriggeredAlarm(measurement.descriptor);
//    }
//}

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

bool DB::setSensorSettings(SensorSettingsDescriptor const& descriptor)
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

    Clock::time_point baselineTimePoint = computeBaselineTimePoint(m_mainData.sensorSettings, descriptor);

    emit sensorSettingsWillBeChanged();
    m_mainData.sensorSettings.descriptor = descriptor;
    m_mainData.sensorSettings.baselineTimePoint = baselineTimePoint;
    m_mainData.sensorSettings.computedCommsPeriod = descriptor.commsPeriod;
    emit sensorSettingsChanged();

    s_logger.logInfo("Changed sensor settings");

    triggerSave();

    return true;
}

//////////////////////////////////////////////////////////////////////////

DB::SensorSettings const& DB::getSensorSettings() const
{
    return m_mainData.sensorSettings;
}

//////////////////////////////////////////////////////////////////////////

DB::Clock::time_point DB::computeBaselineTimePoint(SensorSettings const& oldSensorSettings, SensorSettingsDescriptor const& newDescriptor)
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
        Clock::time_point tp = m.descriptor.timePoint - m.descriptor.index * newDescriptor.measurementPeriod;
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

    return found ? newBaseline : oldSensorSettings.baselineTimePoint;
}

//////////////////////////////////////////////////////////////////////////

bool DB::addSensor(SensorDescriptor const& descriptor)
{
    if (findSensorIndexByName(descriptor.name) >= 0)
    {
        return false;
    }
    auto it = std::find_if(m_mainData.sensors.begin(), m_mainData.sensors.end(), [](Sensor const& sensor) { return sensor.state == Sensor::State::Unbound; });
    if (it != m_mainData.sensors.end())
    {
        return false;
    }

    {
        Sensor sensor;
        sensor.descriptor.name = descriptor.name;
        sensor.id = ++m_mainData.lastSensorId;
        sensor.state = Sensor::State::Unbound;

        emit sensorWillBeAdded(sensor.id);
        m_mainData.sensors.push_back(sensor);
        emit sensorAdded(sensor.id);

        s_logger.logInfo(QString("Added sensor '%1'").arg(descriptor.name.c_str()));
    }

    triggerSave();

    return true;
}

//////////////////////////////////////////////////////////////////////////

bool DB::setSensor(SensorId id, SensorDescriptor const& descriptor)
{
    int32_t index = findSensorIndexByName(descriptor.name);
    if (index >= 0 && getSensor(index).id != id)
    {
        return false;
    }

    index = findSensorIndexById(id);
    if (index < 0)
    {
        return false;
    }

    m_mainData.sensors[index].descriptor = descriptor;
    emit sensorChanged(id);

    s_logger.logInfo(QString("Changed sensor '%1'").arg(descriptor.name.c_str()));

    triggerSave();

    return true;
}

//////////////////////////////////////////////////////////////////////////

bool DB::bindSensor(SensorId id, SensorAddress address, uint32_t serialNumber, Sensor::Calibration const& calibration)
{
    int32_t index = findSensorIndexById(id);
    if (index < 0)
    {
        return false;
    }
    auto it = std::find_if(m_mainData.sensors.begin(), m_mainData.sensors.end(), [&address](Sensor const& sensor) { return sensor.address == address; });
    if (it != m_mainData.sensors.end())
    {
        return false;
    }

    Sensor& sensor = m_mainData.sensors[index];
    if (sensor.state != Sensor::State::Unbound)
    {
        return false;
    }

    sensor.address = address;
    sensor.state = Sensor::State::Active;
    sensor.calibration = calibration;
    sensor.serialNumber = serialNumber;

    emit sensorBound(sensor.id);
    emit sensorChanged(sensor.id);

    s_logger.logInfo(QString("Sensor '%1' bound").arg(sensor.descriptor.name.c_str()));

    triggerSave();

    return true;
}

//////////////////////////////////////////////////////////////////////////

bool DB::setSensorState(SensorId id, Sensor::State state)
{
    int32_t index = findSensorIndexById(id);
    if (index < 0)
    {
        return false;
    }
    if (state == Sensor::State::Unbound)
    {
        return false;
    }

    Sensor& sensor = m_mainData.sensors[index];
    sensor.state = state;

    emit sensorChanged(sensor.id);

    s_logger.logInfo(QString("Sensor '%1' changed state to %2").arg(sensor.descriptor.name.c_str()).arg(static_cast<int>(state)));

    triggerSave();

    return true;
}

//////////////////////////////////////////////////////////////////////////

bool DB::setSensorDetails(SensorId id, Clock::time_point nextMeasurementTimePoint, Clock::time_point lastCommsTimePoint, uint32_t storedMeasurementCount)
{
    int32_t index = findSensorIndexById(id);
    if (index < 0)
    {
        return false;
    }

    Sensor& sensor = m_mainData.sensors[index];
    if (sensor.state == Sensor::State::Unbound)
    {
        return false;
    }

    sensor.nextMeasurementTimePoint = nextMeasurementTimePoint;
    if (lastCommsTimePoint > sensor.lastCommsTimePoint)
    {
        sensor.lastCommsTimePoint = lastCommsTimePoint;
    }
    sensor.storedMeasurementCount = storedMeasurementCount;

    emit sensorDataChanged(sensor.id);

    triggerSave();

    return true;
}

//////////////////////////////////////////////////////////////////////////

void DB::removeSensor(size_t index)
{
    assert(index < m_mainData.sensors.size());

    SensorId sensorId = m_mainData.sensors[index].id;

    {
        auto it = m_mainData.measurements.find(sensorId);
        if (it != m_mainData.measurements.end())
        {
            emit measurementsWillBeRemoved(sensorId);

            for (StoredMeasurement const& sm: it->second)
            {
                MeasurementId id = computeMeasurementId(sensorId, sm);
                auto sit = std::lower_bound(m_mainData.sortedMeasurementIds.begin(), m_mainData.sortedMeasurementIds.end(), id);
                if (sit != m_mainData.sortedMeasurementIds.end() && *sit == id)
                {
                    m_mainData.sortedMeasurementIds.erase(sit);
                }
            }

            m_mainData.measurements.erase(it);
            emit measurementsRemoved(sensorId);
        }
    }

    s_logger.logInfo(QString("Removing sensor '%1'").arg(m_mainData.sensors[index].descriptor.name.c_str()));

    emit sensorWillBeRemoved(sensorId);
    m_mainData.sensors.erase(m_mainData.sensors.begin() + index);
    emit sensorRemoved(sensorId);

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
        return false;
    }

    Alarm alarm;
    alarm.descriptor = descriptor;
    if (!alarm.descriptor.filterSensors)
    {
        alarm.descriptor.sensors.clear();
    }

    alarm.id = ++m_mainData.lastAlarmId;

    emit alarmWillBeAdded(alarm.id);
    m_mainData.alarms.push_back(alarm);
    emit alarmAdded(alarm.id);

    s_logger.logInfo(QString("Added alarm '%1'").arg(descriptor.name.c_str()));

    triggerSave();

    return true;
}

//////////////////////////////////////////////////////////////////////////

bool DB::setAlarm(AlarmId id, AlarmDescriptor const& descriptor)
{
    int32_t index = findAlarmIndexByName(descriptor.name);
    if (index >= 0 && getAlarm(index).id != id)
    {
        return false;
    }

    index = findAlarmIndexById(id);
    if (index < 0)
    {
        return false;
    }

    m_mainData.alarms[index].descriptor = descriptor;
    emit alarmChanged(id);

    s_logger.logInfo(QString("Changed alarm '%1'").arg(descriptor.name.c_str()));

    triggerSave();

    return true;
}

//////////////////////////////////////////////////////////////////////////

void DB::removeAlarm(size_t index)
{
    assert(index < m_mainData.alarms.size());
    AlarmId id = m_mainData.alarms[index].id;

    s_logger.logInfo(QString("Removed alarm '%1'").arg(m_mainData.alarms[index].descriptor.name.c_str()));

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

int32_t DB::findAlarmIndexById(AlarmId id) const
{
    auto it = std::find_if(m_mainData.alarms.begin(), m_mainData.alarms.end(), [id](Alarm const& alarm) { return alarm.id == id; });
    if (it == m_mainData.alarms.end())
    {
        return -1;
    }
    return std::distance(m_mainData.alarms.begin(), it);
}

//////////////////////////////////////////////////////////////////////////

uint8_t DB::computeTriggeredAlarm(MeasurementDescriptor const& md)
{
    uint8_t allTriggered = 0;

    for (Alarm& alarm: m_mainData.alarms)
    {
        AlarmDescriptor const& ad = alarm.descriptor;
        if (ad.filterSensors)
        {
            if (std::find(ad.sensors.begin(), ad.sensors.end(), md.sensorId) == ad.sensors.end())
            {
                continue;
            }
        }

        uint8_t triggered = 0;
        if (ad.highTemperatureWatch && md.temperature > ad.highTemperature)
        {
            triggered |= TriggeredAlarm::Temperature;
        }
        if (ad.lowTemperatureWatch && md.temperature < ad.lowTemperature)
        {
            triggered |= TriggeredAlarm::Temperature;
        }

        if (ad.highHumidityWatch && md.humidity > ad.highHumidity)
        {
            triggered |= TriggeredAlarm::Humidity;
        }
        if (ad.lowHumidityWatch && md.humidity < ad.lowHumidity)
        {
            triggered |= TriggeredAlarm::Humidity;
        }

        if (ad.lowVccWatch && md.vcc <= k_alertVcc)
        {
            triggered |= TriggeredAlarm::LowVcc;
        }
//        if (ad.sensorErrorsWatch && md.sensorErrors != 0)
//        {
//            triggered |= TriggeredAlarm::SensorErrors;
//        }
        if (ad.lowSignalWatch && std::min(md.signalStrength.s2b, md.signalStrength.b2s) <= k_alertSignal)
        {
            triggered |= TriggeredAlarm::LowSignal;
        }

        auto it = std::find(alarm.triggeringSensors.begin(), alarm.triggeringSensors.end(), md.sensorId);
        if (triggered)
        {
            if (it == alarm.triggeringSensors.end())
            {
                alarm.triggeringSensors.push_back(md.sensorId);
                s_logger.logInfo(QString("Alarm '%1' was triggered by measurement index %2").arg(ad.name.c_str()).arg(md.index));

                emit alarmWasTriggered(alarm.id, md.sensorId, md);
            }
        }
        else
        {
            if (it != alarm.triggeringSensors.end())
            {
                alarm.triggeringSensors.erase(it);
                s_logger.logInfo(QString("Alarm '%1' stopped being triggered by measurement index %2").arg(ad.name.c_str()).arg(md.index));

                emit alarmWasUntriggered(alarm.id, md.sensorId, md);
            }
        }

        allTriggered |= triggered;
    }

    return allTriggered;
}

//////////////////////////////////////////////////////////////////////////

size_t DB::getReportCount() const
{
    return m_mainData.reports.size();
}

//////////////////////////////////////////////////////////////////////////

DB::Report const& DB::getReport(size_t index) const
{
    assert(index < m_mainData.reports.size());
    return m_mainData.reports[index];
}

//////////////////////////////////////////////////////////////////////////

bool DB::addReport(ReportDescriptor const& descriptor)
{
    if (findReportIndexByName(descriptor.name) >= 0)
    {
        return false;
    }

    Report report;
    report.descriptor = descriptor;
    if (!report.descriptor.filterSensors)
    {
        report.descriptor.sensors.clear();
    }

    report.id = ++m_mainData.lastReportId;

    emit reportWillBeAdded(report.id);
    m_mainData.reports.push_back(report);
    emit reportAdded(report.id);

    s_logger.logInfo(QString("Added report '%1'").arg(descriptor.name.c_str()));

    triggerSave();

    return true;
}

//////////////////////////////////////////////////////////////////////////

bool DB::setReport(ReportId id, ReportDescriptor const& descriptor)
{
    int32_t index = findReportIndexByName(descriptor.name);
    if (index >= 0 && getReport(index).id != id)
    {
        return false;
    }

    index = findReportIndexById(id);
    if (index < 0)
    {
        return false;
    }

    m_mainData.reports[index].descriptor = descriptor;
    emit reportChanged(id);

    s_logger.logInfo(QString("Changed report '%1'").arg(descriptor.name.c_str()));

    triggerSave();

    return true;
}

//////////////////////////////////////////////////////////////////////////

void DB::removeReport(size_t index)
{
    assert(index < m_mainData.reports.size());
    ReportId id = m_mainData.reports[index].id;

    s_logger.logInfo(QString("Removed report '%1'").arg(m_mainData.reports[index].descriptor.name.c_str()));

    emit reportWillBeRemoved(id);
    m_mainData.reports.erase(m_mainData.reports.begin() + index);
    emit reportRemoved(id);

    triggerSave();
}

//////////////////////////////////////////////////////////////////////////

int32_t DB::findReportIndexByName(std::string const& name) const
{
    auto it = std::find_if(m_mainData.reports.begin(), m_mainData.reports.end(), [&name](Report const& report) { return report.descriptor.name == name; });
    if (it == m_mainData.reports.end())
    {
        return -1;
    }
    return std::distance(m_mainData.reports.begin(), it);
}

//////////////////////////////////////////////////////////////////////////

int32_t DB::findReportIndexById(ReportId id) const
{
    auto it = std::find_if(m_mainData.reports.begin(), m_mainData.reports.end(), [id](Report const& report) { return report.id == id; });
    if (it == m_mainData.reports.end())
    {
        return -1;
    }
    return std::distance(m_mainData.reports.begin(), it);
}

//////////////////////////////////////////////////////////////////////////

bool DB::isReportTriggered(ReportId id) const
{
    int32_t index = findReportIndexById(id);
    if (index < 0)
    {
        return false;
    }

    Report const& report = m_mainData.reports[index];

    if (report.descriptor.period == ReportDescriptor::Period::Daily)
    {
        QDateTime dt = QDateTime::currentDateTime();
        dt.setTime(QTime(9, 0));
        if (Clock::to_time_t(report.lastTriggeredTimePoint) < dt.toTime_t() && Clock::to_time_t(Clock::now()) >= dt.toTime_t())
        {
            return true;
        }
    }
    else if (report.descriptor.period == ReportDescriptor::Period::Weekly)
    {
        QDateTime dt = QDateTime::currentDateTime();
        dt.setDate(dt.date().addDays(-dt.date().dayOfWeek()));
        dt.setTime(QTime(9, 0));
        if (Clock::to_time_t(report.lastTriggeredTimePoint) < dt.toTime_t() && Clock::to_time_t(Clock::now()) >= dt.toTime_t())
        {
            return true;
        }
    }
    else if (report.descriptor.period == ReportDescriptor::Period::Weekly)
    {
        QDate date = QDate::currentDate();
        date = QDate(date.year(), date.month(), 1);
        QDateTime dt(date, QTime(9, 0));

        if (Clock::to_time_t(report.lastTriggeredTimePoint) < dt.toTime_t() && Clock::to_time_t(Clock::now()) >= dt.toTime_t())
        {
            return true;
        }
    }
    else if (report.descriptor.period == ReportDescriptor::Period::Custom)
    {
        Clock::time_point now = Clock::now();
        Clock::duration d = now - report.lastTriggeredTimePoint;
        if (d >= report.descriptor.customPeriod)
        {
            return true;
        }
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////

void DB::setReportExecuted(ReportId id)
{
    int32_t index = findReportIndexById(id);
    if (index < 0)
    {
        return;
    }

    Report& report = m_mainData.reports[index];
    report.lastTriggeredTimePoint = Clock::now();

    triggerSave();
}

//////////////////////////////////////////////////////////////////////////

bool DB::addMeasurement(MeasurementDescriptor const& md)
{
    int32_t sensorIndex = findSensorIndexById(md.sensorId);
    if (sensorIndex < 0)
    {
        return false;
    }

    MeasurementId id = computeMeasurementId(md);

    //check for duplicates
    auto it = std::lower_bound(m_mainData.sortedMeasurementIds.begin(), m_mainData.sortedMeasurementIds.end(), id);
    if (it != m_mainData.sortedMeasurementIds.end() && *it == id)
    {
        return true;
    }

    Measurement measurement;
    measurement.id = id;
    measurement.descriptor = md;
    measurement.triggeredAlarms = computeTriggeredAlarm(md);

    emit measurementsWillBeAdded(md.sensorId);

    //insert sorted id
    m_mainData.sortedMeasurementIds.insert(it, id);

    //insert sorted data
    {
        StoredMeasurements& storedMeasurements = m_mainData.measurements[md.sensorId];

        uint64_t mdtp = Clock::to_time_t(md.timePoint);
        auto smit = std::lower_bound(storedMeasurements.begin(), storedMeasurements.end(), mdtp, [](StoredMeasurement const& sm, uint64_t mdtp)
        {
            return sm.timePoint < mdtp;
        });

        storedMeasurements.insert(smit, pack(measurement));
    }

    emit measurementsAdded(md.sensorId);

    Sensor& sensor = m_mainData.sensors[sensorIndex];
    sensor.isLastMeasurementValid = true;
    sensor.lastMeasurement = measurement;
    sensor.averageSignalStrength = computeAverageSignalStrength(sensor.id, m_mainData);

    s_logger.logInfo(QString("Added measurement index %1, sensor '%2'").arg(md.index).arg(sensor.descriptor.name.c_str()));

    emit sensorDataChanged(sensor.id);

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
    _getFilteredMeasurements(filter, &result);
    return result;
}

//////////////////////////////////////////////////////////////////////////

size_t DB::getFilteredMeasurementCount(Filter const& filter) const
{
    return _getFilteredMeasurements(filter, nullptr);
}

//////////////////////////////////////////////////////////////////////////

size_t DB::_getFilteredMeasurements(Filter const& filter, std::vector<DB::Measurement>* result) const
{
    size_t count = 0;
    for (auto const& pair : m_mainData.measurements)
    {
        SensorId sensorId = pair.first;
        if (filter.useSensorFilter)
        {
            if (std::find(filter.sensorIds.begin(), filter.sensorIds.end(), sensorId) == filter.sensorIds.end())
            {
                continue;
            }
        }

        StoredMeasurements const& storedMeasurements = pair.second;

        //find the first date
        if (filter.useTimePointFilter)
        {
            uint64_t mintp = Clock::to_time_t(filter.timePointFilter.min);
            uint64_t maxtp = Clock::to_time_t(filter.timePointFilter.max);
            auto smit = std::lower_bound(storedMeasurements.begin(), storedMeasurements.end(), mintp, [](StoredMeasurement const& sm, uint64_t mintp)
            {
                return sm.timePoint < mintp;
            });

            //get all results
            for (auto it = smit; it != storedMeasurements.end() && it->timePoint <= maxtp; ++it)
            {
                Measurement m = unpack(sensorId, *it);
                if (cull(m, filter))
                {
                    if (result)
                    {
                        result->push_back(m);
                    }
                    count++;
                }
            }
        }
        else
        {
            for (StoredMeasurement const& sm: storedMeasurements)
            {
                Measurement m = unpack(sensorId, sm);
                if (cull(m, filter))
                {
                    if (result)
                    {
                        result->push_back(m);
                    }
                    count++;
                }
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
        if (m.descriptor.timePoint > best_time_point)
        {
            measurement = m;
            best_time_point = m.descriptor.timePoint;
        }
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////

bool DB::cull(Measurement const& m, Filter const& filter) const
{
//    if (filter.useSensorFilter)
//    {
//        if (std::find(filter.sensorIds.begin(), filter.sensorIds.end(), m.descriptor.sensorId) == filter.sensorIds.end())
//        {
//            return false;
//        }
//    }
    if (filter.useIndexFilter)
    {
        if (m.descriptor.index < filter.indexFilter.min || m.descriptor.index > filter.indexFilter.max)
        {
            return false;
        }
    }
//    if (filter.useTimePointFilter)
//    {
//        if (m.descriptor.timePoint < filter.timePointFilter.min || m.descriptor.timePoint > filter.timePointFilter.max)
//        {
//            return false;
//        }
//    }
    if (filter.useTemperatureFilter)
    {
        if (m.descriptor.temperature < filter.temperatureFilter.min || m.descriptor.temperature > filter.temperatureFilter.max)
        {
            return false;
        }
    }
    if (filter.useHumidityFilter)
    {
        if (m.descriptor.humidity < filter.humidityFilter.min || m.descriptor.humidity > filter.humidityFilter.max)
        {
            return false;
        }
    }
    if (filter.useVccFilter)
    {
        if (m.descriptor.vcc < filter.vccFilter.min || m.descriptor.vcc > filter.vccFilter.max)
        {
            return false;
        }
    }
    if (filter.useB2SFilter)
    {
        if (m.descriptor.signalStrength.b2s < filter.b2sFilter.min || m.descriptor.signalStrength.b2s > filter.b2sFilter.max)
        {
            return false;
        }
    }
    if (filter.useS2BFilter)
    {
        if (m.descriptor.signalStrength.s2b < filter.s2bFilter.min || m.descriptor.signalStrength.s2b > filter.s2bFilter.max)
        {
            return false;
        }
    }
//    if (filter.useSensorErrorsFilter)
//    {
//        bool has_errors = m.descriptor.sensorErrors != 0;
//        if (has_errors != filter.sensorErrorsFilter)
//        {
//            return false;
//        }
//    }

    return true;
}

//////////////////////////////////////////////////////////////////////////

inline DB::StoredMeasurement DB::pack(Measurement const& m)
{
    StoredMeasurement sm;
    sm.index = m.descriptor.index;
    sm.timePoint = Clock::to_time_t(m.descriptor.timePoint);
    sm.temperature = static_cast<int16_t>(std::max(std::min(m.descriptor.temperature, 320.f), -320.f) * 100.f);
    sm.humidity = static_cast<int16_t>(std::max(std::min(m.descriptor.humidity, 320.f), -320.f) * 100.f);
    sm.vcc = static_cast<uint8_t>(std::max(std::min((m.descriptor.vcc - 2.f), 2.55f), 0.f) * 100.f);
    sm.b2s = m.descriptor.signalStrength.b2s;
    sm.s2b = m.descriptor.signalStrength.s2b;
    sm.sensorErrors = m.descriptor.sensorErrors;
    sm.triggeredAlarms = m.triggeredAlarms;
    return sm;
}

//////////////////////////////////////////////////////////////////////////

inline DB::Measurement DB::unpack(SensorId sensorId, StoredMeasurement const& sm)
{
    Measurement m;
    m.id = computeMeasurementId(sensorId, sm);
    m.descriptor.sensorId = sensorId;
    m.descriptor.index = sm.index;
    m.descriptor.timePoint = Clock::from_time_t(sm.timePoint);
    m.descriptor.temperature = static_cast<float>(sm.temperature) / 100.f;
    m.descriptor.humidity = static_cast<float>(sm.humidity) / 100.f;
    m.descriptor.vcc = static_cast<float>(sm.vcc) / 100.f + 2.f;
    m.descriptor.signalStrength.b2s = sm.b2s;
    m.descriptor.signalStrength.s2b = sm.s2b;
    m.descriptor.sensorErrors = sm.sensorErrors;
    m.triggeredAlarms = sm.triggeredAlarms;
    return m;
}

//////////////////////////////////////////////////////////////////////////

inline DB::MeasurementId DB::computeMeasurementId(MeasurementDescriptor const& md)
{
    return (MeasurementId(md.sensorId) << 32) | MeasurementId(md.index);
}

//////////////////////////////////////////////////////////////////////////

inline DB::MeasurementId DB::computeMeasurementId(SensorId sensor_id, StoredMeasurement const& m)
{
    return (MeasurementId(sensor_id) << 32) | MeasurementId(m.index);
}

//////////////////////////////////////////////////////////////////////////

DB::SignalStrength DB::computeAverageSignalStrength(SensorId sensorId, Data const& data) const
{
    auto sit = data.measurements.find(sensorId);
    if (sit == data.measurements.end())
    {
        assert(false);
        return SignalStrength();
    }

    int64_t counts2b = 0;
    int64_t avgs2b = 0;
    int64_t countb2s = 0;
    int64_t avgb2s = 0;
    for (auto it = sit->second.rbegin(); it != sit->second.rend(); ++it)
    {
        Measurement m = unpack(sensorId, *it);
        if (m.descriptor.signalStrength.b2s != 0)
        {
            avgb2s += m.descriptor.signalStrength.b2s;
            countb2s++;
        }
        if (m.descriptor.signalStrength.s2b != 0)
        {
            avgs2b += m.descriptor.signalStrength.s2b;
            counts2b++;
        }
        if (counts2b >= 100 && countb2s >= 100)
        {
            break;
        }
    }

    if (countb2s > 0)
    {
        avgb2s /= countb2s;
    }
    if (counts2b > 0)
    {
        avgs2b /= counts2b;
    }
    SignalStrength avg;
    avg.b2s = static_cast<int8_t>(avgb2s);
    avg.s2b = static_cast<int8_t>(avgs2b);
    return avg;
}

//////////////////////////////////////////////////////////////////////////

void DB::triggerSave()
{
    {
        std::unique_lock<std::mutex> lg(m_storeMutex);
        m_storeData = m_mainData;
        m_storeThreadTriggered = true;
    }
    m_storeCV.notify_all();

    //s_logger.logVerbose("DB save triggered");
}

//////////////////////////////////////////////////////////////////////////

void DB::save(Data const& data) const
{
    bool dailyBackup = false;
    bool weeklyBackup = false;
    Clock::time_point now = Clock::now();
    if (now - m_lastDailyBackupTP >= std::chrono::hours(24))
    {
        m_lastDailyBackupTP = now;
        dailyBackup = true;
    }
    if (now - m_lastWeeklyBackupTP >= std::chrono::hours(24 * 7))
    {
        m_lastWeeklyBackupTP = now;
        weeklyBackup = true;
    }

    std::string dataFilename = (s_dataFolder + "/" + m_dataName);
    std::string dbFilename = (s_dataFolder + "/" + m_dbName);

    Clock::time_point start = now;

    {
        rapidjson::Document document;
        document.SetObject();
        {
            rapidjson::Value ssj;
            ssj.SetObject();
            ssj.AddMember("name", rapidjson::Value(data.sensorSettings.descriptor.name.c_str(), document.GetAllocator()), document.GetAllocator());
            ssj.AddMember("sensors_sleeping", data.sensorSettings.descriptor.sensorsSleeping, document.GetAllocator());
            ssj.AddMember("measurement_period", static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(data.sensorSettings.descriptor.measurementPeriod).count()), document.GetAllocator());
            ssj.AddMember("comms_period", static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(data.sensorSettings.descriptor.commsPeriod).count()), document.GetAllocator());
            ssj.AddMember("baseline", static_cast<uint64_t>(Clock::to_time_t(data.sensorSettings.baselineTimePoint)), document.GetAllocator());
            document.AddMember("sensor_settings", ssj, document.GetAllocator());
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
                sensorj.AddMember("next_measurement", static_cast<uint64_t>(Clock::to_time_t(sensor.nextMeasurementTimePoint)), document.GetAllocator());
                sensorj.AddMember("last_comms", static_cast<uint64_t>(Clock::to_time_t(sensor.lastCommsTimePoint)), document.GetAllocator());
                sensorj.AddMember("temperature_bias", sensor.calibration.temperatureBias, document.GetAllocator());
                sensorj.AddMember("humidity_bias", sensor.calibration.humidityBias, document.GetAllocator());
                sensorj.AddMember("serial_number", sensor.serialNumber, document.GetAllocator());
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
                alarmj.AddMember("id", alarm.id, document.GetAllocator());
                alarmj.AddMember("name", rapidjson::Value(alarm.descriptor.name.c_str(), document.GetAllocator()), document.GetAllocator());
                alarmj.AddMember("filter_sensors", alarm.descriptor.filterSensors, document.GetAllocator());
                if (alarm.descriptor.filterSensors)
                {
                    rapidjson::Value sensorsj;
                    sensorsj.SetArray();
                    for (SensorId const& sensorId: alarm.descriptor.sensors)
                    {
                        sensorsj.PushBack(sensorId, document.GetAllocator());
                    }
                    alarmj.AddMember("sensors", sensorsj, document.GetAllocator());
                }
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
//                alarmj.AddMember("sensor_errors_watch", alarm.descriptor.sensorErrorsWatch, document.GetAllocator());
                alarmj.AddMember("send_email_action", alarm.descriptor.sendEmailAction, document.GetAllocator());
                alarmsj.PushBack(alarmj, document.GetAllocator());
            }
            document.AddMember("alarms", alarmsj, document.GetAllocator());
        }

        {
            rapidjson::Value reportsj;
            reportsj.SetArray();
            for (Report const& report: data.reports)
            {
                rapidjson::Value reportj;
                reportj.SetObject();
                reportj.AddMember("id", report.id, document.GetAllocator());
                reportj.AddMember("last_triggered", static_cast<uint64_t>(Clock::to_time_t(report.lastTriggeredTimePoint)), document.GetAllocator());
                reportj.AddMember("name", rapidjson::Value(report.descriptor.name.c_str(), document.GetAllocator()), document.GetAllocator());
                reportj.AddMember("filter_sensors", report.descriptor.filterSensors, document.GetAllocator());
                if (report.descriptor.filterSensors)
                {
                    rapidjson::Value sensorsj;
                    sensorsj.SetArray();
                    for (SensorId const& sensorId: report.descriptor.sensors)
                    {
                        sensorsj.PushBack(sensorId, document.GetAllocator());
                    }
                    reportj.AddMember("sensors", sensorsj, document.GetAllocator());
                }
                reportj.AddMember("period", static_cast<int32_t>(report.descriptor.period), document.GetAllocator());
                reportj.AddMember("custom_period", static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(report.descriptor.customPeriod).count()), document.GetAllocator());
                reportj.AddMember("data", static_cast<int32_t>(report.descriptor.data), document.GetAllocator());
                reportsj.PushBack(reportj, document.GetAllocator());
            }
            document.AddMember("reports", reportsj, document.GetAllocator());
        }

        rapidjson::StringBuffer buffer;
        rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
        document.Accept(writer);

        Crypt crypt;
        crypt.setCompressionLevel(1);
        crypt.setKey(k_fileEncryptionKey);
        QByteArray encryptedData = crypt.encryptToByteArray(QByteArray(buffer.GetString(), buffer.GetSize()));
//        QByteArray encryptedData = QByteArray(buffer.GetString(), buffer.GetSize());

        std::string tempFilename = (s_dataFolder + "/" + m_dataName + "_temp");
        {
            std::ofstream file(tempFilename, std::ios_base::binary);
            if (!file.is_open())
            {
                s_logger.logCritical(QString("Failed to open '%1': %2").arg(tempFilename.c_str()).arg(std::strerror(errno)));
            }
            else
            {
                file.write(encryptedData.data(), encryptedData.size());
            }
            file.flush();
            file.close();
        }

        copyToBackup(m_dataName, dataFilename, s_dataFolder + "/backups/incremental", 50);

        if (!renameFile(tempFilename.c_str(), dataFilename.c_str()))
        {
            s_logger.logCritical(QString("Failed to rename file '%1' to '%2': %3").arg(tempFilename.c_str()).arg(dataFilename.c_str()).arg(getLastErrorAsString().c_str()));
        }
    }

    {
        std::string buffer;
        {
            std::stringstream stream;
            {
                write(stream, static_cast<uint32_t>(data.measurements.size()));
                for (auto const& pair: data.measurements)
                {
                    write(stream, static_cast<uint32_t>(pair.first));
                    write(stream, static_cast<uint32_t>(pair.second.size()));
                    stream.write(reinterpret_cast<const char*>(pair.second.data()), pair.second.size() * sizeof(StoredMeasurement));
                }
            }
            buffer = stream.str();
        }

        Crypt crypt;
        crypt.setCompressionLevel(1);
        crypt.setKey(k_fileEncryptionKey);
        QByteArray encryptedData = crypt.encryptToByteArray(QByteArray(buffer.data(), buffer.size()));
//        QByteArray encryptedData = QByteArray(buffer.GetString(), buffer.GetSize());

        std::string tempFilename = (s_dataFolder + "/" + m_dbName + "_temp");
        {
            std::ofstream file(tempFilename, std::ios_base::binary);
            if (!file.is_open())
            {
                s_logger.logCritical(QString("Failed to open '%1': %2").arg(tempFilename.c_str()).arg(std::strerror(errno)));
            }
            else
            {
                file.write(encryptedData.data(), encryptedData.size());
            }
            file.flush();
            file.close();
        }

        copyToBackup(m_dbName, dbFilename, s_dataFolder + "/backups/incremental", 50);

        if (!renameFile(tempFilename.c_str(), dbFilename.c_str()))
        {
            s_logger.logCritical(QString("Failed to rename file '%1' to '%2': %3").arg(tempFilename.c_str()).arg(dbFilename.c_str()).arg(getLastErrorAsString().c_str()));
        }
    }

    s_logger.logVerbose(QString("Done saving DB to '%1' & '%2'. Time: %3s").arg(m_dataName.c_str()).arg(m_dbName.c_str()).arg(std::chrono::duration<float>(Clock::now() - start).count()));

    if (dailyBackup)
    {
        copyToBackup(m_dataName, dataFilename, s_dataFolder + "/backups/daily", 10);
        copyToBackup(m_dbName, dbFilename, s_dataFolder + "/backups/daily", 10);
    }
    if (weeklyBackup)
    {
        copyToBackup(m_dataName, dataFilename, s_dataFolder + "/backups/weekly", 10);
        copyToBackup(m_dbName, dbFilename, s_dataFolder + "/backups/weekly", 10);
    }
}

//////////////////////////////////////////////////////////////////////////

void DB::storeThreadProc()
{
    while (!m_threadsExit)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        Data data;
        {
            //wait for data
            std::unique_lock<std::mutex> lg(m_storeMutex);
            if (!m_storeThreadTriggered)
            {
                m_storeCV.wait(lg, [this]{ return m_storeThreadTriggered || m_threadsExit; });
            }
            if (m_threadsExit)
            {
                break;
            }

            data = std::move(m_storeData);
            m_storeData = Data();
            m_storeThreadTriggered = false;
        }

        save(data);
    }
}

//////////////////////////////////////////////////////////////////////////

