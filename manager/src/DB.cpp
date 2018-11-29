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

#define USE_DB_ENCRYPTION
//#define USE_DATA_ENCRYPTION

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


const DB::Clock::duration MEASUREMENT_JITTER = std::chrono::seconds(10);
const DB::Clock::duration COMMS_DURATION = std::chrono::seconds(10);

//////////////////////////////////////////////////////////////////////////

DB::DB()
{
    m_storeThread = std::thread(std::bind(&DB::storeThreadProc, this));
}

//////////////////////////////////////////////////////////////////////////

DB::~DB()
{
    if (m_delayedTriggerSave)
    {
        std::cout << "A DB save was scheduled. Performing it now...\n";
        triggerSave();
        while (m_storeThreadTriggered) //wait for the thread to finish saving
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        std::cout << "Finished scheduled save\n";
    }

    m_threadsExit = true;

    m_storeCV.notify_all();
    if (m_storeThread.joinable())
    {
        m_storeThread.join();
    }
}

//////////////////////////////////////////////////////////////////////////

Result<void> DB::create(std::string const& name)
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

    return success;
}

//////////////////////////////////////////////////////////////////////////

void DB::process()
{
    bool trigger = false;
    {
        std::lock_guard<std::mutex> lg(m_delayedTriggerSaveMutex);
        if (m_delayedTriggerSave)
        {
            trigger = Clock::now() >= m_delayedTriggerSaveTP;
        }
    }

    if (trigger)
    {
        triggerSave();
    }
}

//////////////////////////////////////////////////////////////////////////

Result<void> DB::load(std::string const& name)
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
                return Error("Cannot open DB");
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
                return Error("Cannot open DB");
            }
        }

        if (!document.IsObject())
        {
            s_logger.logCritical(QString("Failed to load '%1': Bad document").arg(dataFilename.c_str()));
            return Error("Cannot open DB");
        }

        //////////////////////////////////////////////////////////////////////////////////

        auto it = document.FindMember("sensors_configs");
        if (it == document.MemberEnd() || !it->value.IsArray())
        {
            s_logger.logCritical(QString("Failed to load '%1': Bad or missing sensor config array").arg(dataFilename.c_str()));
            return Error("Cannot open DB");
        }
        {
            rapidjson::Value const& configsj = it->value;
            for (size_t i = 0; i < configsj.Size(); i++)
            {
                SensorsConfig config;
                rapidjson::Value const& configj = configsj[i];
                auto it = configj.FindMember("name");
                if (it == configj.MemberEnd() || !it->value.IsString())
                {
                    s_logger.logCritical(QString("Failed to load '%1': Bad or missing config name").arg(dataFilename.c_str()));
                    return Error("Cannot open DB");
                }
                config.descriptor.name = it->value.GetString();

                it = configj.FindMember("sensors_power");
                if (it == configj.MemberEnd() || !it->value.IsInt())
                {
                    s_logger.logCritical(QString("Failed to load '%1': Bad or missing config sensors_power").arg(dataFilename.c_str()));
                    return Error("Cannot open DB");
                }
                config.descriptor.sensorsPower = static_cast<int8_t>(it->value.GetInt());

                it = configj.FindMember("measurement_period");
                if (it == configj.MemberEnd() || !it->value.IsUint64())
                {
                    s_logger.logCritical(QString("Failed to load '%1': Bad or missing config measurement_period").arg(dataFilename.c_str()));
                    return Error("Cannot open DB");
                }
                config.descriptor.measurementPeriod = std::chrono::seconds(it->value.GetUint64());

                it = configj.FindMember("comms_period");
                if (it == configj.MemberEnd() || !it->value.IsUint64())
                {
                    s_logger.logCritical(QString("Failed to load '%1': Bad or missing config comms_period").arg(dataFilename.c_str()));
                    return Error("Cannot open DB");
                }
                config.descriptor.commsPeriod = std::chrono::seconds(it->value.GetUint64());

                it = configj.FindMember("baseline_measurement_tp");
                if (it == configj.MemberEnd() || !it->value.IsUint64())
                {
                    s_logger.logCritical(QString("Failed to load '%1': Bad or missing config baseline_measurement_tp").arg(dataFilename.c_str()));
                    return Error("Cannot open DB");
                }
                config.baselineMeasurementTimePoint = Clock::from_time_t(static_cast<time_t>(it->value.GetUint64()));

                it = configj.FindMember("baseline_measurement_index");
                if (it == configj.MemberEnd() || !it->value.IsUint())
                {
                    s_logger.logCritical(QString("Failed to load '%1': Bad or missing config baseline_measurement_index").arg(dataFilename.c_str()));
                    return Error("Cannot open DB");
                }
                config.baselineMeasurementIndex = it->value.GetUint();
                data.sensorsConfigs.push_back(config);
            }
        }

        //////////////////////////////////////////////////////////////////////////////////

        it = document.FindMember("sensors");
        if (it == document.MemberEnd() || !it->value.IsArray())
        {
            s_logger.logCritical(QString("Failed to load '%1': Bad or missing sensors array").arg(dataFilename.c_str()));
            return Error("Cannot open DB");
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
                    return Error("Cannot open DB");
                }
                sensor.descriptor.name = it->value.GetString();

                it = sensorj.FindMember("id");
                if (it == sensorj.MemberEnd() || !it->value.IsUint())
                {
                    s_logger.logCritical(QString("Failed to load '%1': Bad or missing sensor id").arg(dataFilename.c_str()));
                    return Error("Cannot open DB");
                }
                sensor.id = it->value.GetUint();

                it = sensorj.FindMember("address");
                if (it == sensorj.MemberEnd() || !it->value.IsUint())
                {
                    s_logger.logCritical(QString("Failed to load '%1': Bad or missing sensor address").arg(dataFilename.c_str()));
                    return Error("Cannot open DB");
                }
                sensor.address = it->value.GetUint();

                it = sensorj.FindMember("state");
                if (it == sensorj.MemberEnd() || !it->value.IsUint())
                {
                    s_logger.logCritical(QString("Failed to load '%1': Bad or missing sensor state").arg(dataFilename.c_str()));
                    return Error("Cannot open DB");
                }
                sensor.state = static_cast<Sensor::State>(it->value.GetUint());

                it = sensorj.FindMember("should_sleep");
                if (it == sensorj.MemberEnd() || !it->value.IsBool())
                {
                    s_logger.logCritical(QString("Failed to load '%1': Bad or missing sensor should_sleep").arg(dataFilename.c_str()));
                    return Error("Cannot open DB");
                }
                sensor.shouldSleep = it->value.GetBool();

                it = sensorj.FindMember("sleep_state_tp");
                if (it == sensorj.MemberEnd() || !it->value.IsUint64())
                {
                    s_logger.logCritical(QString("Failed to load '%1': Bad or missing config sleep_state_tp").arg(dataFilename.c_str()));
                    return Error("Cannot open DB");
                }
                sensor.sleepStateTimePoint = Clock::from_time_t(static_cast<time_t>(it->value.GetUint64()));

                it = sensorj.FindMember("serial_number");
                if (it == sensorj.MemberEnd() || !it->value.IsUint())
                {
                    s_logger.logCritical(QString("Failed to load '%1': Bad or missing sensor serial_number").arg(dataFilename.c_str()));
                    return Error("Cannot open DB");
                }
                sensor.serialNumber = it->value.GetUint();

                it = sensorj.FindMember("last_confirmed_measurement_index");
                if (it == sensorj.MemberEnd() || !it->value.IsUint())
                {
                    s_logger.logCritical(QString("Failed to load '%1': Bad or missing sensor last_confirmed_measurement_index").arg(dataFilename.c_str()));
                    return Error("Cannot open DB");
                }
                sensor.lastConfirmedMeasurementIndex = it->value.GetUint();

                it = sensorj.FindMember("last_comms");
                if (it != sensorj.MemberEnd() && it->value.IsUint64())
                {
                    if (!it->value.IsUint64())
                    {
                        s_logger.logCritical(QString("Failed to load '%1': Bad or missing sensor next_comms").arg(dataFilename.c_str()));
                        return Error("Cannot open DB");
                    }
                    sensor.lastCommsTimePoint = Clock::from_time_t(static_cast<time_t>(it->value.GetUint64()));
                }

                it = sensorj.FindMember("temperature_bias");
                if (it == sensorj.MemberEnd() || !it->value.IsNumber())
                {
                    s_logger.logCritical(QString("Failed to load '%1': Bad or missing sensor temperature_bias").arg(dataFilename.c_str()));
                    return Error("Cannot open DB");
                }
                sensor.calibration.temperatureBias = static_cast<float>(it->value.GetDouble());

                it = sensorj.FindMember("humidity_bias");
                if (it == sensorj.MemberEnd() || !it->value.IsNumber())
                {
                    s_logger.logCritical(QString("Failed to load '%1': Bad or missing sensor humidity_bias").arg(dataFilename.c_str()));
                    return Error("Cannot open DB");
                }
                sensor.calibration.humidityBias = static_cast<float>(it->value.GetDouble());

                it = sensorj.FindMember("error_counter_comms");
                if (it == sensorj.MemberEnd() || !it->value.IsUint())
                {
                    s_logger.logCritical(QString("Failed to load '%1': Bad or missing sensor error_counter_comms").arg(dataFilename.c_str()));
                    return Error("Cannot open DB");
                }
                sensor.errorCounters.comms = it->value.GetUint();

                it = sensorj.FindMember("error_counter_reboot_unknown");
                if (it == sensorj.MemberEnd() || !it->value.IsUint())
                {
                    s_logger.logCritical(QString("Failed to load '%1': Bad or missing sensor error_counter_reboot_unknown").arg(dataFilename.c_str()));
                    return Error("Cannot open DB");
                }
                sensor.errorCounters.unknownReboots = it->value.GetUint();

                it = sensorj.FindMember("error_counter_reboot_power_on");
                if (it == sensorj.MemberEnd() || !it->value.IsUint())
                {
                    s_logger.logCritical(QString("Failed to load '%1': Bad or missing sensor error_counter_reboot_power_on").arg(dataFilename.c_str()));
                    return Error("Cannot open DB");
                }
                sensor.errorCounters.powerOnReboots = it->value.GetUint();

                it = sensorj.FindMember("error_counter_reboot_reset");
                if (it == sensorj.MemberEnd() || !it->value.IsUint())
                {
                    s_logger.logCritical(QString("Failed to load '%1': Bad or missing sensor error_counter_reboot_reset").arg(dataFilename.c_str()));
                    return Error("Cannot open DB");
                }
                sensor.errorCounters.resetReboots = it->value.GetUint();

                it = sensorj.FindMember("error_counter_reboot_brownout");
                if (it == sensorj.MemberEnd() || !it->value.IsUint())
                {
                    s_logger.logCritical(QString("Failed to load '%1': Bad or missing sensor error_counter_reboot_brownout").arg(dataFilename.c_str()));
                    return Error("Cannot open DB");
                }
                sensor.errorCounters.brownoutReboots = it->value.GetUint();

                it = sensorj.FindMember("error_counter_reboot_watchdog");
                if (it == sensorj.MemberEnd() || !it->value.IsUint())
                {
                    s_logger.logCritical(QString("Failed to load '%1': Bad or missing sensor error_counter_reboot_watchdog").arg(dataFilename.c_str()));
                    return Error("Cannot open DB");
                }
                sensor.errorCounters.watchdogReboots = it->value.GetUint();

                data.lastSensorAddress = std::max(data.lastSensorAddress, sensor.address);
                data.lastSensorId = std::max(data.lastSensorId, sensor.id);
                data.sensors.push_back(sensor);
            }
        }

        //////////////////////////////////////////////////////////////////////////////////

        it = document.FindMember("alarms");
        if (it == document.MemberEnd() || !it->value.IsArray())
        {
            s_logger.logCritical(QString("Failed to load '%1': Bad or missing alarms array").arg(dataFilename.c_str()));
            return Error("Cannot open DB");
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
                    return Error("Cannot open DB");
                }
                alarm.descriptor.name = it->value.GetString();

                it = alarmj.FindMember("id");
                if (it == alarmj.MemberEnd() || !it->value.IsUint())
                {
                    s_logger.logCritical(QString("Failed to load '%1': Bad or missing alarm id").arg(dataFilename.c_str()));
                    return Error("Cannot open DB");
                }
                alarm.id = it->value.GetUint();

                it = alarmj.FindMember("filter_sensors");
                if (it != alarmj.MemberEnd() && !it->value.IsBool())
                {
                    s_logger.logCritical(QString("Failed to load '%1': Bad alarm filter_sensors").arg(dataFilename.c_str()));
                    return Error("Cannot open DB");
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
                        return Error("Cannot open DB");
                    }
                    rapidjson::Value const& sensorsj = it->value;
                    for (size_t si = 0; si < sensorsj.Size(); si++)
                    {
                        rapidjson::Value const& sensorj = sensorsj[si];
                        if (!sensorj.IsUint())
                        {
                            s_logger.logCritical(QString("Failed to load '%1': Bad or missing alarm sensor id").arg(dataFilename.c_str()));
                            return Error("Cannot open DB");
                        }
                        alarm.descriptor.sensors.insert(sensorj.GetUint());
                    }
                }


                it = alarmj.FindMember("low_temperature_watch");
                if (it == alarmj.MemberEnd() || !it->value.IsBool())
                {
                    s_logger.logCritical(QString("Failed to load '%1': Bad or missing alarm low_temperature_watch").arg(dataFilename.c_str()));
                    return Error("Cannot open DB");
                }
                alarm.descriptor.lowTemperatureWatch = it->value.GetBool();

                it = alarmj.FindMember("low_temperature");
                if (it == alarmj.MemberEnd() || !it->value.IsDouble())
                {
                    s_logger.logCritical(QString("Failed to load '%1': Bad or missing alarm low_temperature").arg(dataFilename.c_str()));
                    return Error("Cannot open DB");
                }
                alarm.descriptor.lowTemperature = static_cast<float>(it->value.GetDouble());

                it = alarmj.FindMember("high_temperature_watch");
                if (it == alarmj.MemberEnd() || !it->value.IsBool())
                {
                    s_logger.logCritical(QString("Failed to load '%1': Bad or missing alarm high_temperature_watch").arg(dataFilename.c_str()));
                    return Error("Cannot open DB");
                }
                alarm.descriptor.highTemperatureWatch = it->value.GetBool();

                it = alarmj.FindMember("high_temperature");
                if (it == alarmj.MemberEnd() || !it->value.IsDouble())
                {
                    s_logger.logCritical(QString("Failed to load '%1': Bad or missing alarm high_temperature").arg(dataFilename.c_str()));
                    return Error("Cannot open DB");
                }
                alarm.descriptor.highTemperature = static_cast<float>(it->value.GetDouble());

                it = alarmj.FindMember("low_humidity_watch");
                if (it == alarmj.MemberEnd() || !it->value.IsBool())
                {
                    s_logger.logCritical(QString("Failed to load '%1': Bad or missing alarm low_humidity_watch").arg(dataFilename.c_str()));
                    return Error("Cannot open DB");
                }
                alarm.descriptor.lowHumidityWatch = it->value.GetBool();

                it = alarmj.FindMember("low_humidity");
                if (it == alarmj.MemberEnd() || !it->value.IsDouble())
                {
                    s_logger.logCritical(QString("Failed to load '%1': Bad or missing alarm low_humidity").arg(dataFilename.c_str()));
                    return Error("Cannot open DB");
                }
                alarm.descriptor.lowHumidity = static_cast<float>(it->value.GetDouble());

                it = alarmj.FindMember("high_humidity_watch");
                if (it == alarmj.MemberEnd() || !it->value.IsBool())
                {
                    s_logger.logCritical(QString("Failed to load '%1': Bad or missing alarm high_humidity_watch").arg(dataFilename.c_str()));
                    return Error("Cannot open DB");
                }
                alarm.descriptor.highHumidityWatch = it->value.GetBool();

                it = alarmj.FindMember("high_humidity");
                if (it == alarmj.MemberEnd() || !it->value.IsDouble())
                {
                    s_logger.logCritical(QString("Failed to load '%1': Bad or missing alarm high_humidity").arg(dataFilename.c_str()));
                    return Error("Cannot open DB");
                }
                alarm.descriptor.highHumidity = static_cast<float>(it->value.GetDouble());

                it = alarmj.FindMember("low_vcc_watch");
                if (it == alarmj.MemberEnd() || !it->value.IsBool())
                {
                    s_logger.logCritical(QString("Failed to load '%1': Bad or missing alarm low_vcc_watch").arg(dataFilename.c_str()));
                    return Error("Cannot open DB");
                }
                alarm.descriptor.lowVccWatch = it->value.GetBool();

                it = alarmj.FindMember("low_signal_watch");
                if (it == alarmj.MemberEnd() || !it->value.IsBool())
                {
                    s_logger.logCritical(QString("Failed to load '%1': Bad or missing alarm low_signal_watch").arg(dataFilename.c_str()));
                    return Error("Cannot open DB");
                }
                alarm.descriptor.lowSignalWatch = it->value.GetBool();

//                it = alarmj.FindMember("sensor_errors_watch");
//                if (it == alarmj.MemberEnd() || !it->value.IsBool())
//                {
//                    s_logger.logCritical(QString("Failed to load '%1': Bad or missing alarm sensor_errors_watch").arg(dataFilename.c_str()));
//                    return Error("Cannot open DB");
//                }
//                alarm.descriptor.sensorErrorsWatch = it->value.GetBool();

                it = alarmj.FindMember("send_email_action");
                if (it == alarmj.MemberEnd() || !it->value.IsBool())
                {
                    s_logger.logCritical(QString("Failed to load '%1': Bad or missing alarm send_email_action").arg(dataFilename.c_str()));
                    return Error("Cannot open DB");
                }
                alarm.descriptor.sendEmailAction = it->value.GetBool();

                data.lastAlarmId = std::max(data.lastAlarmId, alarm.id);
                data.alarms.push_back(alarm);
            }
        }

        //////////////////////////////////////////////////////////////////////////////////

        it = document.FindMember("reports");
        if (it == document.MemberEnd() || !it->value.IsArray())
        {
            s_logger.logCritical(QString("Failed to load '%1': Bad or missing reports array").arg(dataFilename.c_str()));
            return Error("Cannot open DB");
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
                    return Error("Cannot open DB");
                }
                report.descriptor.name = it->value.GetString();

                it = reportj.FindMember("id");
                if (it == reportj.MemberEnd() || !it->value.IsUint())
                {
                    s_logger.logCritical(QString("Failed to load '%1': Bad or missing report id").arg(dataFilename.c_str()));
                    return Error("Cannot open DB");
                }
                report.id = it->value.GetUint();

                it = reportj.FindMember("last_triggered");
                if (it == reportj.MemberEnd() || !it->value.IsUint64())
                {
                    s_logger.logCritical(QString("Failed to load '%1': Bad or missing sensor report last_triggered").arg(dataFilename.c_str()));
                    return Error("Cannot open DB");
                }
                report.lastTriggeredTimePoint = Clock::from_time_t(static_cast<time_t>(it->value.GetUint64()));

                it = reportj.FindMember("filter_sensors");
                if (it == reportj.MemberEnd() || !it->value.IsBool())
                {
                    s_logger.logCritical(QString("Failed to load '%1': Bad report filter_sensors").arg(dataFilename.c_str()));
                    return Error("Cannot open DB");
                }
                report.descriptor.filterSensors = it->value.GetBool();

                if (report.descriptor.filterSensors)
                {
                    it = reportj.FindMember("sensors");
                    if (it == reportj.MemberEnd() || !it->value.IsArray())
                    {
                        s_logger.logCritical(QString("Failed to load '%1': Bad or missing report sensors").arg(dataFilename.c_str()));
                        return Error("Cannot open DB");
                    }
                    rapidjson::Value const& sensorsj = it->value;
                    for (size_t si = 0; si < sensorsj.Size(); si++)
                    {
                        rapidjson::Value const& sensorj = sensorsj[si];
                        if (!sensorj.IsUint())
                        {
                            s_logger.logCritical(QString("Failed to load '%1': Bad or missing report sensor id").arg(dataFilename.c_str()));
                            return Error("Cannot open DB");
                        }
                        report.descriptor.sensors.insert(sensorj.GetUint());
                    }
                }

                it = reportj.FindMember("period");
                if (it == reportj.MemberEnd() || !it->value.IsInt())
                {
                    s_logger.logCritical(QString("Failed to load '%1': Bad or missing report period").arg(dataFilename.c_str()));
                    return Error("Cannot open DB");
                }
                report.descriptor.period = static_cast<DB::ReportDescriptor::Period>(it->value.GetInt());

                it = reportj.FindMember("custom_period");
                if (it == reportj.MemberEnd() || !it->value.IsUint64())
                {
                    s_logger.logCritical(QString("Failed to load '%1': Bad or missing report custom_period").arg(dataFilename.c_str()));
                    return Error("Cannot open DB");
                }
                report.descriptor.customPeriod = std::chrono::seconds(it->value.GetUint64());

                it = reportj.FindMember("data");
                if (it == reportj.MemberEnd() || !it->value.IsInt())
                {
                    s_logger.logCritical(QString("Failed to load '%1': Bad or missing report data").arg(dataFilename.c_str()));
                    return Error("Cannot open DB");
                }
                report.descriptor.data = static_cast<DB::ReportDescriptor::Data>(it->value.GetInt());

                data.lastReportId = std::max(data.lastReportId, report.id);
                data.reports.push_back(report);
            }
        }
    }

    //////////////////////////////////////////////////////////////////////////////////

    {
        std::string streamData;
        {
            std::ifstream file(dbFilename, std::ios_base::binary);
            if (!file.is_open())
            {
                s_logger.logCritical(QString("Failed to open '%1': %2").arg(dbFilename.c_str()).arg(std::strerror(errno)));
                return Error("Cannot open DB");
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
            stream = std::stringstream(std::string(streamData.data(), streamData.size())); //try unencrypted
            if (!read(stream, sensorCount))
            {
                s_logger.logCritical(QString("Failed to read '%1': %2").arg(dbFilename.c_str()).arg(std::strerror(errno)));
                return Error("Cannot open DB");
            }
        }
        if (sensorCount > 10000)
        {
            s_logger.logCritical(QString("Failed to read '%1': file seems to be corrupted").arg(dbFilename.c_str()));
            return Error("Cannot open DB");
        }

        for (uint32_t i = 0; i < sensorCount; i++)
        {
            uint32_t sensorId = 0;
            uint32_t measurementsCount = 0;
            if (!read(stream, sensorId) || !read(stream, measurementsCount))
            {
                s_logger.logCritical(QString("Failed to read '%1': %2").arg(dbFilename.c_str()).arg(std::strerror(errno)));
                return Error("Cannot open DB");
            }
            if (measurementsCount > 100000000)
            {
                s_logger.logCritical(QString("Failed to read '%1': file seems to be corrupted").arg(dbFilename.c_str()));
                return Error("Cannot open DB");
            }

            auto sensorIt = std::find_if(data.sensors.begin(), data.sensors.end(), [&sensorId](Sensor const& sensor) { return sensor.id == sensorId; });
            if (sensorIt == data.sensors.end())
            {
                s_logger.logCritical(QString("Failed to read '%1': cannot find sensor").arg(dbFilename.c_str()));
                return Error("Cannot open DB");
            }
            Sensor& sensor = *sensorIt;

            StoredMeasurements& storedMeasurements = data.measurements[sensorId];

            size_t recordSize = sizeof(StoredMeasurement);
            storedMeasurements.resize(measurementsCount);
            if (stream.read(reinterpret_cast<char*>(storedMeasurements.data()), storedMeasurements.size() * recordSize).bad())
            {
                s_logger.logCritical(QString("Failed to read '%1': %2").arg(dbFilename.c_str()).arg(std::strerror(errno)));
                return Error("Cannot open DB");
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
    //refresh signal strengths
    for (Sensor& sensor: m_mainData.sensors)
    {
        sensor.averageSignalStrength = computeAverageSignalStrength(sensor.id, m_mainData);
        sensor.averageCombinedSignalStrength = std::min(sensor.averageSignalStrength.b2s, sensor.averageSignalStrength.s2b);
    }
    //refresh computed comms period
    if (!m_mainData.sensorsConfigs.empty())
    {
        SensorsConfig& config = m_mainData.sensorsConfigs.back();
        config.computedCommsPeriod = computeCommsPeriod();
    }

    s_logger.logVerbose(QString("Done loading DB from '%1' & '%2'. Time: %3s").arg(m_dataName.c_str()).arg(m_dbName.c_str()).arg(std::chrono::duration<float>(Clock::now() - start).count()));

    return success;
}

//void DB::test()
//{
//    for (StoredMeasurement const& sm: m_mainData.measurements.begin()->second)
//    {
//        Measurement measurement = unpack(m_mainData.measurements.begin()->first, sm);
//        computeTriggeredAlarm(measurement.descriptor);
//    }
//}

///////////////////////////////////////////////////////////////////////////////////////////

DB::Clock::duration DB::computeCommsPeriod() const
{
    SensorsConfig const& config = getLastSensorsConfig();

    Clock::duration max_period = m_mainData.sensors.size() * COMMS_DURATION;
    Clock::duration period = std::max(config.descriptor.commsPeriod, max_period);
    return std::max(period, config.descriptor.measurementPeriod + MEASUREMENT_JITTER);
}

///////////////////////////////////////////////////////////////////////////////////////////

DB::Clock::time_point DB::computeNextMeasurementTimePoint(Sensor const& sensor) const
{
    uint32_t nextMeasurementIndex = computeNextMeasurementIndex(sensor);
    SensorsConfig const& config = findSensorsConfigForMeasurementIndex(nextMeasurementIndex);
    Clock::time_point tp = config.baselineMeasurementTimePoint + config.descriptor.measurementPeriod * (nextMeasurementIndex - config.baselineMeasurementIndex);
    return tp;
}

///////////////////////////////////////////////////////////////////////////////////////////

DB::Clock::time_point DB::computeNextCommsTimePoint(Sensor const& /*sensor*/, size_t sensorIndex) const
{
    SensorsConfig const& config = getLastSensorsConfig();
    Clock::duration period = computeCommsPeriod();

    Clock::time_point now = Clock::now();
    uint32_t index = static_cast<uint32_t>(std::ceil(((now - config.baselineMeasurementTimePoint) / period))) + 1;

    Clock::time_point start = config.baselineMeasurementTimePoint + period * index;
    return start + sensorIndex * COMMS_DURATION;
}

///////////////////////////////////////////////////////////////////////////////////////////

uint32_t DB::computeNextMeasurementIndex(Sensor const& sensor) const
{
    uint32_t nextSensorMeasurementIndex = sensor.firstStoredMeasurementIndex + sensor.storedMeasurementCount;

    //make sure the next measurement index doesn't fall below the last non-sleeping config
    //next_sensor_measurement_index = std::max(next_sensor_measurement_index, compute_baseline_measurement_index());

    return nextSensorMeasurementIndex;
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
    Sensor const& sensor = m_mainData.sensors[index];
    return sensor;
}

///////////////////////////////////////////////////////////////////////////////////////////

DB::SensorOutputDetails DB::computeSensorOutputDetails(SensorId id) const
{
    int32_t _index = findSensorIndexById(id);
    if (_index < 0)
    {
        return {};
    }

    size_t index = static_cast<size_t>(_index);
    Sensor const& sensor = m_mainData.sensors[index];
    SensorsConfig const& config = getLastSensorsConfig();

    SensorOutputDetails details;
    details.commsPeriod = computeCommsPeriod();
    details.nextCommsTimePoint = computeNextCommsTimePoint(sensor, index);
    details.measurementPeriod = config.descriptor.measurementPeriod;
    details.nextMeasurementTimePoint = computeNextMeasurementTimePoint(sensor);
    details.nextRealTimeMeasurementIndex = computeNextRealTimeMeasurementIndex();
    details.nextMeasurementIndex = computeNextMeasurementIndex(sensor);
    return details;
}

///////////////////////////////////////////////////////////////////////////////////////////

uint32_t DB::computeNextRealTimeMeasurementIndex() const
{
    SensorsConfig const& config = getLastSensorsConfig();
    Clock::time_point now = Clock::now();
    uint32_t index = static_cast<uint32_t>(std::ceil((now - config.baselineMeasurementTimePoint) / config.descriptor.measurementPeriod)) + config.baselineMeasurementIndex;
    return index;
}

//////////////////////////////////////////////////////////////////////////

Result<void> DB::addSensorsConfig(SensorsConfigDescriptor const& descriptor)
{
    if (descriptor.name.empty())
    {
        return Error("Config without a name");
    }
    if (descriptor.commsPeriod < std::chrono::seconds(30))
    {
        return Error("Comms period cannot be lower than 30 seconds");
    }
    if (descriptor.measurementPeriod < std::chrono::seconds(10))
    {
        return Error("Measurement period cannot be lower than 10 seconds");
    }
    if (descriptor.commsPeriod < descriptor.measurementPeriod)
    {
        return Error("Comms period cannot be lower than the measurement period");
    }

    SensorsConfig config;
    config.descriptor = descriptor;

    if (!m_mainData.sensorsConfigs.empty())
    {
        uint32_t nextRealTimeMeasurementIndex = computeNextRealTimeMeasurementIndex();

        SensorsConfig const& c = findSensorsConfigForMeasurementIndex(nextRealTimeMeasurementIndex);
        Clock::time_point nextMeasurementTP = c.baselineMeasurementTimePoint + c.descriptor.measurementPeriod * (nextRealTimeMeasurementIndex - c.baselineMeasurementIndex);

        m_mainData.sensorsConfigs.erase(std::remove_if(m_mainData.sensorsConfigs.begin(), m_mainData.sensorsConfigs.end(), [&nextRealTimeMeasurementIndex](SensorsConfig const& c)
        {
            return c.baselineMeasurementIndex == nextRealTimeMeasurementIndex;
        }), m_mainData.sensorsConfigs.end());
        config.baselineMeasurementIndex = nextRealTimeMeasurementIndex;
        config.baselineMeasurementTimePoint = nextMeasurementTP;
    }
    else
    {
        config.baselineMeasurementIndex = 0;
        config.baselineMeasurementTimePoint = Clock::now();
    }

    emit sensorsConfigWillBeAdded();

    m_mainData.sensorsConfigs.push_back(config);
    while (m_mainData.sensorsConfigs.size() > 100)
    {
        m_mainData.sensorsConfigs.erase(m_mainData.sensorsConfigs.begin());
    }
    m_mainData.sensorsConfigs.back().computedCommsPeriod = computeCommsPeriod(); //make sure the computed commd config is up-to-date

    emit sensorsConfigAdded();

    s_logger.logInfo("Added sensors config");

    triggerDelayedSave(std::chrono::seconds(1));

    return success;
}

//////////////////////////////////////////////////////////////////////////

Result<void> DB::setSensorsConfigs(std::vector<SensorsConfig> const& configs)
{
    emit sensorsConfigWillBeChanged();

    m_mainData.sensorsConfigs = configs;
    for (SensorsConfig& config: m_mainData.sensorsConfigs)
    {
        config.computedCommsPeriod = config.descriptor.commsPeriod;
    }
    if (!m_mainData.sensorsConfigs.empty())
    {
        SensorsConfig& config = m_mainData.sensorsConfigs.back();
        config.computedCommsPeriod = computeCommsPeriod();
    }

    emit sensorsConfigChanged();

    s_logger.logInfo("Changed sensors configs");

    triggerDelayedSave(std::chrono::seconds(1));
    return success;
}

//////////////////////////////////////////////////////////////////////////

DB::SensorsConfig const& DB::getLastSensorsConfig() const
{
    static SensorsConfig s_empty;
    return m_mainData.sensorsConfigs.empty() ? s_empty : m_mainData.sensorsConfigs.back();
}

//////////////////////////////////////////////////////////////////////////

size_t DB::getSensorsConfigCount() const
{
    return m_mainData.sensorsConfigs.size();
}

//////////////////////////////////////////////////////////////////////////

DB::SensorsConfig const& DB::getSensorsConfig(size_t index) const
{
    assert(index < m_mainData.sensorsConfigs.size());
    return m_mainData.sensorsConfigs[index];
}

//////////////////////////////////////////////////////////////////////////

DB::SensorsConfig const& DB::findSensorsConfigForMeasurementIndex(uint32_t index) const
{
    for (auto it = m_mainData.sensorsConfigs.rbegin(); it != m_mainData.sensorsConfigs.rend(); ++it)
    {
        SensorsConfig const& c = *it;
        if (index >= c.baselineMeasurementIndex)
        {
            return c;
        }
    }
    return getLastSensorsConfig();
}

//////////////////////////////////////////////////////////////////////////

Result<void> DB::addSensor(SensorDescriptor const& descriptor)
{
    if (findSensorIndexByName(descriptor.name) >= 0)
    {
        return Error("Name '" + descriptor.name + "' already in use");
    }
    auto it = std::find_if(m_mainData.sensors.begin(), m_mainData.sensors.end(), [](Sensor const& sensor) { return sensor.state == Sensor::State::Unbound; });
    if (it != m_mainData.sensors.end())
    {
        return Error("There is already an unbound sensor");
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

    //refresh computed comms period as new sensors are added
    if (!m_mainData.sensorsConfigs.empty())
    {
        SensorsConfig& config = m_mainData.sensorsConfigs.back();
        config.computedCommsPeriod = computeCommsPeriod();
        emit sensorsConfigChanged();
    }

    triggerDelayedSave(std::chrono::seconds(1));

    return success;
}

//////////////////////////////////////////////////////////////////////////

Result<void> DB::setSensor(SensorId id, SensorDescriptor const& descriptor)
{
    int32_t _index = findSensorIndexByName(descriptor.name);
    if (_index >= 0 && getSensor(static_cast<size_t>(_index)).id != id)
    {
        return Error("Name '" + descriptor.name + "' already in use");
    }

    _index = findSensorIndexById(id);
    if (_index < 0)
    {
        return Error("Trying to change non-existing sensor");
    }

    size_t index = static_cast<size_t>(_index);
    m_mainData.sensors[index].descriptor = descriptor;
    emit sensorChanged(id);

    s_logger.logInfo(QString("Changed sensor '%1'").arg(descriptor.name.c_str()));

    triggerDelayedSave(std::chrono::seconds(1));

    return success;
}

//////////////////////////////////////////////////////////////////////////

Result<DB::SensorId> DB::bindSensor(uint32_t serialNumber, uint8_t sensorType, uint8_t hardwareVersion, uint8_t softwareVersion, Sensor::Calibration const& calibration)
{
    int32_t _index = findUnboundSensorIndex();
    if (_index < 0)
    {
        return Error("No unbound sensor");
    }

    size_t index = static_cast<size_t>(_index);
    Sensor& sensor = m_mainData.sensors[index];
    if (sensor.state != Sensor::State::Unbound)
    {
        return Error("No unbound sensor (inconsistent internal state)");
    }

    sensor.address = ++m_mainData.lastSensorAddress;
    sensor.sensorType = sensorType;
    sensor.hardwareVersion = hardwareVersion;
    sensor.softwareVersion = softwareVersion;
    sensor.calibration = calibration;
    sensor.state = Sensor::State::Active;
    sensor.serialNumber = serialNumber;

    emit sensorBound(sensor.id);
    emit sensorChanged(sensor.id);

    s_logger.logInfo(QString("Sensor '%1' bound").arg(sensor.descriptor.name.c_str()));

    //refresh computed comms period as new sensors are added
    if (!m_mainData.sensorsConfigs.empty())
    {
        SensorsConfig& config = m_mainData.sensorsConfigs.back();
        config.computedCommsPeriod = computeCommsPeriod();
        emit sensorsConfigChanged();
    }

    triggerDelayedSave(std::chrono::seconds(1));

    return sensor.id;
}

//////////////////////////////////////////////////////////////////////////

Result<void> DB::setSensorCalibration(SensorId id, Sensor::Calibration const& calibration)
{
    int32_t _index = findSensorIndexById(id);
    if (_index < 0)
    {
        return Error("Invalid sensor id");
    }
    size_t index = static_cast<size_t>(_index);
    Sensor& sensor = m_mainData.sensors[index];
    if (sensor.state == Sensor::State::Unbound)
    {
        return Error("Cannot change calibration for unbound sensor");
    }

    sensor.calibration = calibration;

    emit sensorChanged(sensor.id);

    s_logger.logInfo(QString("Sensor '%1' calibration changed").arg(sensor.descriptor.name.c_str()));

    triggerDelayedSave(std::chrono::seconds(1));

    return success;
}

//////////////////////////////////////////////////////////////////////////

Result<void> DB::setSensorSleep(SensorId id, bool sleep)
{
    int32_t _index = findSensorIndexById(id);
    if (_index < 0)
    {
        return Error("Invalid sensor id");
    }
    size_t index = static_cast<size_t>(_index);
    Sensor& sensor = m_mainData.sensors[index];
    if (sensor.state == Sensor::State::Unbound)
    {
        return Error("Cannot put unbound sensor to sleep");
    }

    if (sensor.shouldSleep == sleep)
    {
        return success;
    }

    DB::SensorsConfig config = getLastSensorsConfig();
    bool hasMeasurements = sensor.estimatedStoredMeasurementCount > 0;
    bool commedRecently = (DB::Clock::now() - sensor.lastCommsTimePoint) < config.descriptor.commsPeriod * 2;
    bool allowedToSleep = !hasMeasurements && commedRecently;
    if (sleep && !sensor.shouldSleep && !allowedToSleep)
    {
        if (hasMeasurements)
        {
            return Error("Cannot put sensor to sleep because it has stored measurements");
        }
        return Error("Cannot put sensor to sleep because it didn't communicate recently");
    }

    sensor.shouldSleep = sleep;

    emit sensorChanged(sensor.id);

    s_logger.logInfo(QString("Sensor '%1' sleep state changed to %2").arg(sensor.descriptor.name.c_str()).arg(sleep));

    triggerDelayedSave(std::chrono::seconds(1));

    return success;
}

//////////////////////////////////////////////////////////////////////////

bool DB::setSensorInputDetails(SensorInputDetails const& details)
{
    return setSensorsInputDetails({details});
}

//////////////////////////////////////////////////////////////////////////

bool DB::setSensorsInputDetails(std::vector<SensorInputDetails> const& details)
{
    bool ok = true;
    for (SensorInputDetails const& d: details)
    {
        int32_t _index = findSensorIndexById(d.id);
        if (_index < 0)
        {
            ok = false;
            continue;
        }

        size_t index = static_cast<size_t>(_index);
        Sensor& sensor = m_mainData.sensors[index];
        if (sensor.state == Sensor::State::Unbound)
        {
            ok = false;
            continue;
        }

        //sensor.nextMeasurementTimePoint = d.nextMeasurementTimePoint;
        if (d.hasLastCommsTimePoint)
        {
            if (d.lastCommsTimePoint > sensor.lastCommsTimePoint)
            {
                sensor.lastCommsTimePoint = d.lastCommsTimePoint;
            }
        }
        if (d.hasStoredData)
        {
            sensor.firstStoredMeasurementIndex = d.firstStoredMeasurementIndex;
            sensor.storedMeasurementCount = d.storedMeasurementCount;

            if (sensor.firstStoredMeasurementIndex > 0)
            {
                //we'll never receive measurements lower than sensor->first_recorded_measurement_index
                //so at worst, max_confirmed_measurement_index = sensor->first_recorded_measurement_index - 1 (the one just before the first recorded index)
                sensor.lastConfirmedMeasurementIndex = std::max(sensor.lastConfirmedMeasurementIndex, sensor.firstStoredMeasurementIndex - 1);
            }

            {
                //if storing 1 measurement starting from index 15, the last stored index is 15 (therefore the -1 at the end)
                int64_t lastStoredIndex = static_cast<int64_t>(sensor.firstStoredMeasurementIndex) +
                        static_cast<int64_t>(sensor.storedMeasurementCount) - 1;
                lastStoredIndex = std::max<int64_t>(lastStoredIndex, 0); //make sure we don't get negatives
                int64_t count = std::max<int64_t>(lastStoredIndex - static_cast<int64_t>(sensor.lastConfirmedMeasurementIndex), 0);
                sensor.estimatedStoredMeasurementCount = static_cast<uint32_t>(count);
            }
        }

        if (d.hasSleepingData)
        {
            if (sensor.state != Sensor::State::Unbound)
            {
                if (sensor.state == Sensor::State::Sleeping && !d.sleeping)
                {
                    //the sensor has slept, now it wakes up and start measuring from the current index
                    sensor.lastConfirmedMeasurementIndex = computeNextRealTimeMeasurementIndex();
                }
                if (sensor.state != Sensor::State::Sleeping && d.sleeping)
                {
                    //mark the moment the sensor went to sleep
                    sensor.sleepStateTimePoint = Clock::now();
                }
                sensor.state = d.sleeping ? Sensor::State::Sleeping : Sensor::State::Active;
            }
        }

        if (d.hasSignalStrength)
        {
            sensor.lastSignalStrengthB2S = d.signalStrengthB2S;
        }

        if (d.hasErrorCountersDelta)
        {
            sensor.errorCounters.comms += d.errorCountersDelta.comms;
            sensor.errorCounters.resetReboots += d.errorCountersDelta.resetReboots;
            sensor.errorCounters.unknownReboots += d.errorCountersDelta.unknownReboots;
            sensor.errorCounters.brownoutReboots += d.errorCountersDelta.brownoutReboots;
            sensor.errorCounters.powerOnReboots += d.errorCountersDelta.powerOnReboots;
            sensor.errorCounters.watchdogReboots += d.errorCountersDelta.watchdogReboots;
        }

        emit sensorDataChanged(sensor.id);
    }

    triggerDelayedSave(std::chrono::seconds(1));

    return ok;
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

    //refresh computed comms period as new sensors are removed
    if (!m_mainData.sensorsConfigs.empty())
    {
        SensorsConfig& config = m_mainData.sensorsConfigs.back();
        config.computedCommsPeriod = computeCommsPeriod();
        emit sensorsConfigChanged();
    }

    triggerDelayedSave(std::chrono::seconds(1));
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

int32_t DB::findSensorIndexByAddress(SensorAddress address) const
{
    auto it = std::find_if(m_mainData.sensors.begin(), m_mainData.sensors.end(), [&address](Sensor const& sensor) { return sensor.address == address; });
    if (it == m_mainData.sensors.end())
    {
        return -1;
    }
    return std::distance(m_mainData.sensors.begin(), it);
}

//////////////////////////////////////////////////////////////////////////

int32_t DB::findSensorIndexBySerialNumber(SensorSerialNumber serialNumber) const
{
    auto it = std::find_if(m_mainData.sensors.begin(), m_mainData.sensors.end(), [&serialNumber](Sensor const& sensor) { return sensor.serialNumber == serialNumber; });
    if (it == m_mainData.sensors.end())
    {
        return -1;
    }
    return std::distance(m_mainData.sensors.begin(), it);
}

//////////////////////////////////////////////////////////////////////////

int32_t DB::findUnboundSensorIndex() const
{
    auto it = std::find_if(m_mainData.sensors.begin(), m_mainData.sensors.end(), [](Sensor const& sensor) { return sensor.state == Sensor::State::Unbound; });
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

Result<void> DB::addAlarm(AlarmDescriptor const& descriptor)
{
    if (findAlarmIndexByName(descriptor.name) >= 0)
    {
        return Error("Name '" + descriptor.name + "' already in use");
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

    triggerDelayedSave(std::chrono::seconds(1));

    return success;
}

//////////////////////////////////////////////////////////////////////////

Result<void> DB::setAlarm(AlarmId id, AlarmDescriptor const& descriptor)
{
    int32_t _index = findAlarmIndexByName(descriptor.name);
    if (_index >= 0 && getAlarm(static_cast<size_t>(_index)).id != id)
    {
        return Error("Name to '" + descriptor.name + "' already in use");
    }

    _index = findAlarmIndexById(id);
    if (_index < 0)
    {
        return Error("Trying to change non-existing alarm");
    }

    size_t index = static_cast<size_t>(_index);
    m_mainData.alarms[index].descriptor = descriptor;
    emit alarmChanged(id);

    s_logger.logInfo(QString("Changed alarm '%1'").arg(descriptor.name.c_str()));

    triggerDelayedSave(std::chrono::seconds(1));

    return success;
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

    triggerDelayedSave(std::chrono::seconds(1));
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

uint8_t DB::computeTriggeredAlarm(Measurement const& m)
{
    uint8_t allTriggered = 0;

    for (Alarm& alarm: m_mainData.alarms)
    {
        AlarmDescriptor const& ad = alarm.descriptor;
        if (ad.filterSensors)
        {
            if (ad.sensors.find(m.descriptor.sensorId) == ad.sensors.end())
            {
                continue;
            }
        }

        uint8_t triggered = 0;
        if (ad.highTemperatureWatch && m.descriptor.temperature > ad.highTemperature)
        {
            triggered |= TriggeredAlarm::Temperature;
        }
        if (ad.lowTemperatureWatch && m.descriptor.temperature < ad.lowTemperature)
        {
            triggered |= TriggeredAlarm::Temperature;
        }

        if (ad.highHumidityWatch && m.descriptor.humidity > ad.highHumidity)
        {
            triggered |= TriggeredAlarm::Humidity;
        }
        if (ad.lowHumidityWatch && m.descriptor.humidity < ad.lowHumidity)
        {
            triggered |= TriggeredAlarm::Humidity;
        }

        if (ad.lowVccWatch && m.descriptor.vcc <= k_alertVcc)
        {
            triggered |= TriggeredAlarm::LowVcc;
        }
//        if (ad.sensorErrorsWatch && m.descriptor.sensorErrors != 0)
//        {
//            triggered |= TriggeredAlarm::SensorErrors;
//        }
        if (ad.lowSignalWatch && std::min(m.descriptor.signalStrength.s2b, m.descriptor.signalStrength.b2s) <= k_alertSignal)
        {
            triggered |= TriggeredAlarm::LowSignal;
        }

        auto it = alarm.triggeringSensors.find(m.descriptor.sensorId);
        if (triggered)
        {
            if (it == alarm.triggeringSensors.end())
            {
                alarm.triggeringSensors.insert(m.descriptor.sensorId);
                s_logger.logInfo(QString("Alarm '%1' was triggered by measurement index %2").arg(ad.name.c_str()).arg(m.descriptor.index));

                emit alarmWasTriggered(alarm.id, m);
            }
        }
        else
        {
            if (it != alarm.triggeringSensors.end())
            {
                alarm.triggeringSensors.erase(it);
                s_logger.logInfo(QString("Alarm '%1' stopped being triggered by measurement index %2").arg(ad.name.c_str()).arg(m.descriptor.index));

                emit alarmWasUntriggered(alarm.id, m);
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

Result<void> DB::addReport(ReportDescriptor const& descriptor)
{
    if (findReportIndexByName(descriptor.name) >= 0)
    {
        return Error("Name '" + descriptor.name + "' already in use");
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

    triggerDelayedSave(std::chrono::seconds(1));

    return success;
}

//////////////////////////////////////////////////////////////////////////

Result<void> DB::setReport(ReportId id, ReportDescriptor const& descriptor)
{
    int32_t _index = findReportIndexByName(descriptor.name);
    if (_index >= 0 && getReport(static_cast<size_t>(_index)).id != id)
    {
        return Error("Name to '" + descriptor.name + "' already in use");
    }

    _index = findReportIndexById(id);
    if (_index < 0)
    {
        return Error("Trying to change non-existing report");
    }

    size_t index = static_cast<size_t>(_index);
    m_mainData.reports[index].descriptor = descriptor;
    emit reportChanged(id);

    s_logger.logInfo(QString("Changed report '%1'").arg(descriptor.name.c_str()));

    triggerDelayedSave(std::chrono::seconds(1));

    return success;
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

    triggerDelayedSave(std::chrono::seconds(1));
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
    int32_t _index = findReportIndexById(id);
    if (_index < 0)
    {
        return false;
    }

    size_t index = static_cast<size_t>(_index);
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
    int32_t _index = findReportIndexById(id);
    if (_index < 0)
    {
        return;
    }

    size_t index = static_cast<size_t>(_index);
    Report& report = m_mainData.reports[index];
    report.lastTriggeredTimePoint = Clock::now();

    triggerDelayedSave(std::chrono::seconds(1));
}

//////////////////////////////////////////////////////////////////////////

bool DB::addMeasurement(MeasurementDescriptor const& md)
{
    return _addMeasurements(md.sensorId, {md});
}

//////////////////////////////////////////////////////////////////////////

bool DB::addMeasurements(std::vector<MeasurementDescriptor> const& mds)
{
    std::map<SensorId, std::vector<MeasurementDescriptor>> mdPerSensor;
    for (MeasurementDescriptor const& md: mds)
    {
        mdPerSensor[md.sensorId].push_back(md);
    }

    bool ok = true;
    for (auto& pair: mdPerSensor)
    {
        ok &= _addMeasurements(pair.first, std::move(pair.second));
    }
    return ok;
}

//////////////////////////////////////////////////////////////////////////

bool DB::_addMeasurements(SensorId sensorId, std::vector<MeasurementDescriptor> mds)
{
    int32_t _sensorIndex = findSensorIndexById(sensorId);
    if (_sensorIndex < 0)
    {
        return false;
    }
    size_t sensorIndex = static_cast<size_t>(_sensorIndex);
    Sensor& sensor = m_mainData.sensors[sensorIndex];


    //first remove past measurements
    mds.erase(std::remove_if(mds.begin(), mds.end(), [&sensor](MeasurementDescriptor const& d)
    {
        return d.index < sensor.lastConfirmedMeasurementIndex;
    }), mds.end());

    if (mds.empty())
    {
        return true;
    }

    sensor.isLastMeasurementValid = true;

    emit measurementsWillBeAdded(sensorId);

    for (MeasurementDescriptor const& md: mds)
    {
        MeasurementId id = computeMeasurementId(md);

        //advance the last confirmed index
        if (md.index == sensor.lastConfirmedMeasurementIndex + 1)
        {
            sensor.lastConfirmedMeasurementIndex = md.index;
        }

        //check for duplicates
        auto it = std::lower_bound(m_mainData.sortedMeasurementIds.begin(), m_mainData.sortedMeasurementIds.end(), id);
        if (it != m_mainData.sortedMeasurementIds.end() && *it == id)
        {
            continue;
        }

        Measurement measurement;
        measurement.id = id;
        measurement.descriptor = md;
        measurement.timePoint = computeMeasurementTimepoint(md);
        measurement.triggeredAlarms = computeTriggeredAlarm(measurement);
        measurement.combinedSignalStrength = std::min(md.signalStrength.b2s, md.signalStrength.s2b);

        //insert sorted id
        m_mainData.sortedMeasurementIds.insert(it, id);

        //insert sorted data
        {
            StoredMeasurements& storedMeasurements = m_mainData.measurements[md.sensorId];

            uint64_t mdtp = static_cast<uint64_t>(Clock::to_time_t(measurement.timePoint));
            auto smit = std::lower_bound(storedMeasurements.begin(), storedMeasurements.end(), mdtp, [](StoredMeasurement const& sm, uint64_t mdtp)
            {
                return sm.timePoint < mdtp;
            });

            storedMeasurements.insert(smit, pack(measurement));
        }
        sensor.lastMeasurement = measurement;

        s_logger.logVerbose(QString("Added measurement index %1, sensor '%2'").arg(md.index).arg(sensor.descriptor.name.c_str()));
    }

    emit measurementsAdded(sensorId);

    sensor.averageSignalStrength = computeAverageSignalStrength(sensor.id, m_mainData);
    sensor.averageCombinedSignalStrength = std::min(sensor.averageSignalStrength.b2s, sensor.averageSignalStrength.s2b);

    emit sensorDataChanged(sensor.id);

    triggerDelayedSave(std::chrono::seconds(1));

    return true;
}

//////////////////////////////////////////////////////////////////////////

DB::Clock::time_point DB::computeMeasurementTimepoint(MeasurementDescriptor const& md) const
{
    SensorsConfig const& config = findSensorsConfigForMeasurementIndex(md.index);
    Clock::time_point tp = config.baselineMeasurementTimePoint + config.descriptor.measurementPeriod * (md.index - config.baselineMeasurementIndex);
    return tp;
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
            if (filter.sensorIds.find(sensorId) == filter.sensorIds.end())
            {
                continue;
            }
        }

        StoredMeasurements const& storedMeasurements = pair.second;

        //find the first date
        if (filter.useTimePointFilter)
        {
            uint64_t mintp = static_cast<uint64_t>(Clock::to_time_t(filter.timePointFilter.min));
            uint64_t maxtp = static_cast<uint64_t>(Clock::to_time_t(filter.timePointFilter.max));
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

Result<DB::Measurement> DB::getLastMeasurementForSensor(SensorId sensor_id) const
{
    auto it = m_mainData.measurements.find(sensor_id);
    if (it == m_mainData.measurements.end())
    {
        assert(false);
        return false;
    }

    bool found = false;
    Clock::time_point best_time_point = Clock::time_point(Clock::duration::zero());
    for (StoredMeasurement const& sm: it->second)
    {
        Measurement m = unpack(sensor_id, sm);
        if (m.timePoint > best_time_point)
        {
            found = true;
            measurement = m;
            best_time_point = m.timePoint;
        }
    }

    return found;
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
    if (filter.useSignalStrengthFilter)
    {
        if (m.combinedSignalStrength < filter.signalStrengthFilter.min || m.combinedSignalStrength > filter.signalStrengthFilter.max)
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
    sm.timePoint = static_cast<uint64_t>(Clock::to_time_t(m.timePoint));
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
    m.timePoint = Clock::from_time_t(static_cast<time_t>(sm.timePoint));
    m.descriptor.temperature = static_cast<float>(sm.temperature) / 100.f;
    m.descriptor.humidity = static_cast<float>(sm.humidity) / 100.f;
    m.descriptor.vcc = static_cast<float>(sm.vcc) / 100.f + 2.f;
    m.descriptor.signalStrength.b2s = sm.b2s;
    m.descriptor.signalStrength.s2b = sm.s2b;
    m.combinedSignalStrength = std::min(sm.b2s, sm.s2b);
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
        return {};
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
        if (counts2b >= 10 && countb2s >= 10)
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
        std::lock_guard<std::mutex> lg(m_delayedTriggerSaveMutex);
        m_delayedTriggerSave = false;
    }

    {
        std::unique_lock<std::mutex> lg(m_storeMutex);
        m_storeData = m_mainData;
        m_storeThreadTriggered = true;
    }
    m_storeCV.notify_all();

    //s_logger.logVerbose("DB save triggered");
}

//////////////////////////////////////////////////////////////////////////

void DB::triggerDelayedSave(Clock::duration i_dt)
{
    if (i_dt < std::chrono::milliseconds(100))
    {
        triggerSave();
        return;
    }

    std::lock_guard<std::mutex> lg(m_delayedTriggerSaveMutex);
    if (m_delayedTriggerSave && m_delayedTriggerSaveTP < Clock::now() + i_dt)
    {
        //if the previous schedule is sooner than the new one, leave the previous intact
        return;
    }

    m_delayedTriggerSaveTP = Clock::now() + i_dt;
    m_delayedTriggerSave = true;
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
//        {
//            rapidjson::Value ssj;
//            ssj.SetObject();
//            ssj.AddMember("name", rapidjson::Value(data.sensorSettings.descriptor.name.c_str(), document.GetAllocator()), document.GetAllocator());
//            ssj.AddMember("sensors_sleeping", data.sensorSettings.descriptor.sensorsSleeping, document.GetAllocator());
//            ssj.AddMember("measurement_period", static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(data.sensorSettings.descriptor.measurementPeriod).count()), document.GetAllocator());
//            ssj.AddMember("comms_period", static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(data.sensorSettings.descriptor.commsPeriod).count()), document.GetAllocator());
////            ssj.AddMember("baseline", static_cast<uint64_t>(Clock::to_time_t(data.sensorSettings.baselineTimePoint)), document.GetAllocator());
//            document.AddMember("sensor_settings", ssj, document.GetAllocator());
//        }

        {
            rapidjson::Value configsj;
            configsj.SetArray();
            for (SensorsConfig const& config: data.sensorsConfigs)
            {
                rapidjson::Value configj;
                configj.SetObject();
                configj.AddMember("name", rapidjson::Value(config.descriptor.name.c_str(), document.GetAllocator()), document.GetAllocator());
                configj.AddMember("sensors_power", static_cast<int32_t>(config.descriptor.sensorsPower), document.GetAllocator());
                configj.AddMember("measurement_period", static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(config.descriptor.measurementPeriod).count()), document.GetAllocator());
                configj.AddMember("comms_period", static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(config.descriptor.commsPeriod).count()), document.GetAllocator());
                configj.AddMember("baseline_measurement_tp", static_cast<uint64_t>(Clock::to_time_t(config.baselineMeasurementTimePoint)), document.GetAllocator());
                configj.AddMember("baseline_measurement_index", config.baselineMeasurementIndex, document.GetAllocator());
                configsj.PushBack(configj, document.GetAllocator());
            }
            document.AddMember("sensors_configs", configsj, document.GetAllocator());
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
                sensorj.AddMember("should_sleep", sensor.shouldSleep, document.GetAllocator());
                sensorj.AddMember("sleep_state_tp", static_cast<uint64_t>(Clock::to_time_t(sensor.sleepStateTimePoint)), document.GetAllocator());
                sensorj.AddMember("last_comms", static_cast<uint64_t>(Clock::to_time_t(sensor.lastCommsTimePoint)), document.GetAllocator());
                sensorj.AddMember("temperature_bias", sensor.calibration.temperatureBias, document.GetAllocator());
                sensorj.AddMember("humidity_bias", sensor.calibration.humidityBias, document.GetAllocator());
                sensorj.AddMember("serial_number", sensor.serialNumber, document.GetAllocator());
                sensorj.AddMember("last_confirmed_measurement_index", sensor.lastConfirmedMeasurementIndex, document.GetAllocator());
                sensorj.AddMember("error_counter_comms", sensor.errorCounters.comms, document.GetAllocator());
                sensorj.AddMember("error_counter_reboot_unknown", sensor.errorCounters.unknownReboots, document.GetAllocator());
                sensorj.AddMember("error_counter_reboot_power_on", sensor.errorCounters.powerOnReboots, document.GetAllocator());
                sensorj.AddMember("error_counter_reboot_reset", sensor.errorCounters.resetReboots, document.GetAllocator());
                sensorj.AddMember("error_counter_reboot_brownout", sensor.errorCounters.brownoutReboots, document.GetAllocator());
                sensorj.AddMember("error_counter_reboot_watchdog", sensor.errorCounters.watchdogReboots, document.GetAllocator());
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
        crypt.setCompressionLevel(-1); //default
        crypt.setKey(k_fileEncryptionKey);
#ifdef USE_DATA_ENCRYPTION
        QByteArray dataToWrite = crypt.encryptToByteArray(QByteArray(buffer.GetString(), buffer.GetSize()));
#else
        QByteArray dataToWrite = QByteArray(buffer.GetString(), buffer.GetSize());
#endif


        std::string tempFilename = (s_dataFolder + "/" + m_dataName + "_temp");
        {
            std::ofstream file(tempFilename, std::ios_base::binary);
            if (!file.is_open())
            {
                s_logger.logCritical(QString("Failed to open '%1': %2").arg(tempFilename.c_str()).arg(std::strerror(errno)));
            }
            else
            {
                file.write(dataToWrite.data(), dataToWrite.size());
            }
            file.flush();
            file.close();
        }

        copyToBackup(m_dataName, dataFilename, s_dataFolder + "/backups/incremental", 50);

        if (!renameFile(tempFilename, dataFilename))
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
#ifdef USE_DB_ENCRYPTION
        QByteArray dataToWrite = crypt.encryptToByteArray(QByteArray(buffer.data(), buffer.size()));
#else
        QByteArray dataToWrite = QByteArray(buffer.data(), buffer.size());
#endif

        std::string tempFilename = (s_dataFolder + "/" + m_dbName + "_temp");
        {
            std::ofstream file(tempFilename, std::ios_base::binary);
            if (!file.is_open())
            {
                s_logger.logCritical(QString("Failed to open '%1': %2").arg(tempFilename.c_str()).arg(std::strerror(errno)));
            }
            else
            {
                file.write(dataToWrite.data(), dataToWrite.size());
            }
            file.flush();
            file.close();
        }

        copyToBackup(m_dbName, dbFilename, s_dataFolder + "/backups/incremental", 50);

        if (!renameFile(tempFilename, dbFilename))
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
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
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

