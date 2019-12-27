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

#include "cereal/archives/json.hpp"
#include "cereal/types/vector.hpp"
#include "cereal/types/string.hpp"
#include "cereal/types/array.hpp"
#include "cereal/types/map.hpp"
#include "cereal/types/set.hpp"
#include "cereal/types/chrono.hpp"
#include "cereal/external/rapidjson/error/en.h"

#include "Utils.h"
#include "Crypt.h"
#include "Logger.h"

#ifdef NDEBUG
#   define USE_DB_ENCRYPTION
#   define USE_DATA_ENCRYPTION
#endif

extern Logger s_logger;

extern std::string s_dataFolder;

constexpr float k_alertPercentageVcc = 10.f; //%
constexpr int8_t k_alertPercentageSignal = 10.f; //%

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

extern std::string getMacStr(DB::BaseStationDescriptor::Mac const& mac);


constexpr float DB::k_maxBatteryLevel;
constexpr float DB::k_minBatteryLevel;

constexpr float DB::k_maxSignalLevel;
constexpr float DB::k_minSignalLevel;

#define NVP(x) cereal::make_nvp(#x, v.x)
template<class Archive> void save(Archive& archive, const DB::BaseStationDescriptor& v, std::uint32_t const version)
{
	char macStr[128];
	sprintf(macStr, "%X:%X:%X:%X:%X:%X", v.mac[0] & 0xFF, v.mac[1] & 0xFF, v.mac[2] & 0xFF, v.mac[3] & 0xFF, v.mac[4] & 0xFF, v.mac[5] & 0xFF);
	archive(NVP(name), cereal::make_nvp("mac", std::string(macStr)));
}
template<class Archive> void load(Archive& archive, DB::BaseStationDescriptor& v, std::uint32_t const version)
{
    std::string macStr;
	archive(NVP(name), cereal::make_nvp("mac", macStr));
	int m0, m1, m2, m3, m4, m5;
	if (sscanf(macStr.c_str(), "%X:%X:%X:%X:%X:%X", &m0, &m1, &m2, &m3, &m4, &m5) != 6)
	{
		throw cereal::Exception("Bad base station mac");
	}
	v.mac = { static_cast<uint8_t>(m0 & 0xFF), static_cast<uint8_t>(m1 & 0xFF), static_cast<uint8_t>(m2 & 0xFF), static_cast<uint8_t>(m3 & 0xFF), static_cast<uint8_t>(m4 & 0xFF), static_cast<uint8_t>(m5 & 0xFF) };
}
template<class Archive> void serialize(Archive& archive, DB::BaseStation& v, std::uint32_t const version)
{
	archive(NVP(descriptor), NVP(id));
}
template<class Archive> void save(Archive& archive, const DB::SensorsConfigDescriptor& v, std::uint32_t const version)
{
	int64_t mp = std::chrono::duration_cast<std::chrono::seconds>(v.measurementPeriod).count();
    int64_t cp = std::chrono::duration_cast<std::chrono::seconds>(v.commsPeriod).count();
	archive(NVP(sensorsPower), CEREAL_NVP(mp), CEREAL_NVP(cp));
}
template<class Archive> void load(Archive& archive, DB::SensorsConfigDescriptor& v, std::uint32_t const version)
{
    int64_t mp;
    int64_t cp;
	archive(NVP(sensorsPower), CEREAL_NVP(mp), CEREAL_NVP(cp));
	v.measurementPeriod = std::chrono::seconds(std::max<int64_t>(mp, 0));
    v.commsPeriod = std::chrono::seconds(std::max<int64_t>(cp, 0));
}
template<class Archive> void save(Archive& archive, const DB::SensorsConfig& v, std::uint32_t const version)
{
    int64_t ccp = std::chrono::duration_cast<std::chrono::seconds>(v.computedCommsPeriod).count();
	uint64_t bmtp = static_cast<uint64_t>(DB::Clock::to_time_t(v.baselineMeasurementTimePoint));
	archive(NVP(descriptor), CEREAL_NVP(ccp), CEREAL_NVP(bmtp), NVP(baselineMeasurementIndex));
}
template<class Archive> void load(Archive& archive, DB::SensorsConfig& v, std::uint32_t const version)
{
    int64_t ccp;
    uint64_t bmtp;
	archive(NVP(descriptor), CEREAL_NVP(ccp), CEREAL_NVP(bmtp), NVP(baselineMeasurementIndex));
	v.computedCommsPeriod = std::chrono::seconds(std::max<int64_t>(ccp, 0));
    v.baselineMeasurementTimePoint = DB::Clock::from_time_t(bmtp);
}
template<class Archive> void serialize(Archive& archive, DB::SignalStrength& v, std::uint32_t const version)
{
	archive(NVP(s2b), NVP(b2s));
}
template<class Archive> void serialize(Archive& archive, DB::ErrorCounters& v, std::uint32_t const version)
{
	archive(NVP(comms), NVP(unknownReboots), NVP(powerOnReboots), NVP(resetReboots), NVP(brownoutReboots), NVP(watchdogReboots));
}
template<class Archive> void serialize(Archive& archive, DB::SensorDescriptor& v, std::uint32_t const version)
{
	archive(NVP(name));
}
template<class Archive> void serialize(Archive& archive, DB::Sensor::Calibration& v, std::uint32_t const version)
{
	archive(NVP(temperatureBias), NVP(humidityBias));
}
template<class Archive> void serialize(Archive& archive, DB::Sensor::DeviceInfo& v, std::uint32_t const version)
{
	archive(NVP(sensorType), NVP(softwareVersion), NVP(hardwareVersion));
}
template<class Archive> void save(Archive& archive, const DB::Sensor& v, std::uint32_t const version)
{
    uint64_t sstp = static_cast<uint64_t>(DB::Clock::to_time_t(v.sleepStateTimePoint));
    uint64_t lctp = static_cast<uint64_t>(DB::Clock::to_time_t(v.lastCommsTimePoint));
	archive(NVP(descriptor), NVP(id), NVP(address), NVP(deviceInfo),
            NVP(calibration), NVP(serialNumber), NVP(state),
            NVP(shouldSleep), CEREAL_NVP(sstp), NVP(errorCounters),
			CEREAL_NVP(lctp), NVP(lastConfirmedMeasurementIndex), NVP(firstStoredMeasurementIndex),
            NVP(storedMeasurementCount), NVP(estimatedStoredMeasurementCount), NVP(lastSignalStrengthB2S),
            NVP(averageSignalStrength), NVP(averageCombinedSignalStrength),
            NVP(isMeasurementValid), NVP(measurementTemperature), NVP(measurementHumidity), NVP(measurementVcc));
}
template<class Archive> void load(Archive& archive, DB::Sensor& v, std::uint32_t const version)
{
    uint64_t sstp;
    uint64_t lctp;
	archive(NVP(descriptor), NVP(id), NVP(address), NVP(deviceInfo),
            NVP(calibration), NVP(serialNumber), NVP(state),
            NVP(shouldSleep), CEREAL_NVP(sstp), NVP(errorCounters),
			CEREAL_NVP(lctp), NVP(lastConfirmedMeasurementIndex), NVP(firstStoredMeasurementIndex),
            NVP(storedMeasurementCount), NVP(estimatedStoredMeasurementCount), NVP(lastSignalStrengthB2S),
            NVP(averageSignalStrength), NVP(averageCombinedSignalStrength),
            NVP(isMeasurementValid), NVP(measurementTemperature), NVP(measurementHumidity), NVP(measurementVcc));
    v.sleepStateTimePoint = DB::Clock::from_time_t(sstp);
    v.lastCommsTimePoint = DB::Clock::from_time_t(lctp);
}
template<class Archive> void serialize(Archive& archive, DB::AlarmDescriptor& v, std::uint32_t const version)
{
	archive(NVP(name), NVP(filterSensors), NVP(sensors), NVP(lowTemperatureWatch), NVP(lowTemperature), NVP(highTemperatureWatch), NVP(highTemperature),
            NVP(lowHumidityWatch), NVP(lowHumidity), NVP(highHumidityWatch), NVP(highHumidity), NVP(lowVccWatch), NVP(lowSignalWatch), NVP(sendEmailAction));
}
template<class Archive> void serialize(Archive& archive, DB::Alarm& v, std::uint32_t const version)
{
	archive(NVP(descriptor), NVP(id), NVP(triggersPerSensor));
}
template<class Archive> void save(Archive& archive, const DB::ReportDescriptor& v, std::uint32_t const version)
{
    int64_t cp = std::chrono::duration_cast<std::chrono::seconds>(v.customPeriod).count();
	archive(NVP(name), NVP(period), CEREAL_NVP(cp), NVP(data), NVP(filterSensors), NVP(sensors));
}
template<class Archive> void load(Archive& archive, DB::ReportDescriptor& v, std::uint32_t const version)
{
    int64_t cp;
	archive(NVP(name), NVP(period), CEREAL_NVP(cp), NVP(data), NVP(filterSensors), NVP(sensors));
    v.customPeriod = std::chrono::seconds(std::max<int64_t>(cp, 0));
}
template<class Archive> void save(Archive& archive, const DB::Report& v, std::uint32_t const version)
{
    uint64_t lttp = static_cast<uint64_t>(DB::Clock::to_time_t(v.lastTriggeredTimePoint));
	archive(NVP(descriptor), NVP(id), CEREAL_NVP(lttp));
}
template<class Archive> void load(Archive& archive, DB::Report& v, std::uint32_t const version)
{
    uint64_t lttp;
	archive(NVP(descriptor), NVP(id), CEREAL_NVP(lttp));
    v.lastTriggeredTimePoint = DB::Clock::from_time_t(lttp);
}

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

Result<void> DB::create()
{
    std::lock_guard<std::recursive_mutex> lg(m_mainDataMutex);

    m_dbName = "sense.db";
    m_dataName = "sense.data";
    m_mainData.measurements.clear();

    std::string dataFilename = s_dataFolder + "/" + m_dataName;
    std::string dbFilename = s_dataFolder + "/" + m_dbName;

    moveToBackup(m_dbName, dbFilename, s_dataFolder + "/backups/deleted", 50);
    moveToBackup(m_dataName, dataFilename, s_dataFolder + "/backups/deleted", 50);

    remove((dbFilename).c_str());
    remove((dataFilename).c_str());

    m_mainData = Data();
    addSensorsConfig(SensorsConfigDescriptor()).ignore();
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

Result<void> DB::load()
{
    Clock::time_point start = Clock::now();

    m_dbName = "sense.db";
    m_dataName = "sense.data";

    std::string dataFilename = (s_dataFolder + "/" + m_dataName);
    std::string dbFilename = (s_dataFolder + "/" + m_dbName);

    Data data;

	std::string streamData;
	{
		std::ifstream file(dataFilename, std::ios_base::binary);
		if (!file.is_open())
		{
            QString msg = QString("Failed to open '%1': %2").arg(dataFilename.c_str()).arg(std::strerror(errno));
			s_logger.logCritical(msg);
			return Error(msg.toUtf8().data());
		}
		streamData = std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	}

	try
	{
		Crypt crypt;
		crypt.setKey(k_fileEncryptionKey);
		QByteArray decryptedData = crypt.decryptToByteArray(QByteArray(streamData.data(), int32_t(streamData.size())));

		std::istringstream stream(std::string(decryptedData.data(), decryptedData.size()));
		cereal::JSONInputArchive archive(stream);
		archive(cereal::make_nvp("db", data));
	}
	catch (std::exception e)
	{
		try
		{
			std::istringstream stream(streamData);
			cereal::JSONInputArchive archive(stream);
			archive(cereal::make_nvp("db", data));
		}
		catch (std::exception e)
		{
			QString msg = QString("Failed to open '%1': %2").arg(dataFilename.c_str()).arg(e.what());
			s_logger.logCritical(msg);
			return Error(msg.toUtf8().data());
		}
	}

	for (const BaseStation& bs: data.baseStations)
	{
		data.lastBaseStationId = std::max(data.lastBaseStationId, bs.id);
	}
	for (const Sensor& sensor: data.sensors)
	{
		data.lastSensorAddress = std::max(data.lastSensorAddress, sensor.address);
		data.lastSensorId = std::max(data.lastSensorId, sensor.id);
	}
	for (const Alarm& alarm: data.alarms)
	{
		data.lastAlarmId = std::max(data.lastAlarmId, alarm.id);
	}
	for (const Report& report: data.reports)
	{
		data.lastReportId = std::max(data.lastReportId, report.id);
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
        QByteArray decryptedData = crypt.decryptToByteArray(QByteArray(streamData.data(), (int)streamData.size()));

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
                    ++it;
                }
            }

            if (!storedMeasurements.empty())
            {
                Measurement measurement = unpack(sensorId, storedMeasurements.back());
                sensor.isMeasurementValid = true;
                sensor.measurementTemperature = measurement.descriptor.temperature;
                sensor.measurementHumidity = measurement.descriptor.humidity;
                sensor.measurementVcc = measurement.descriptor.vcc;
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

    {
        std::lock_guard<std::recursive_mutex> lg(m_mainDataMutex);
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
            config.computedCommsPeriod = computeActualCommsPeriod(config.descriptor);
        }

        s_logger.logVerbose(QString("Done loading DB from '%1' & '%2'. Time: %3s").arg(m_dataName.c_str()).arg(m_dbName.c_str()).arg(std::chrono::duration<float>(Clock::now() - start).count()));
    }

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

DB::Clock::duration DB::computeActualCommsPeriod(SensorsConfigDescriptor const& config) const
{
    std::lock_guard<std::recursive_mutex> lg(m_mainDataMutex);

    Clock::duration max_period = m_mainData.sensors.size() * COMMS_DURATION;
    Clock::duration period = std::max(config.commsPeriod, max_period);
    return std::max(period, config.measurementPeriod + MEASUREMENT_JITTER);
}

///////////////////////////////////////////////////////////////////////////////////////////

DB::Clock::time_point DB::computeNextMeasurementTimePoint(Sensor const& sensor) const
{
    uint32_t nextMeasurementIndex = computeNextMeasurementIndex(sensor);
    SensorsConfig const& config = findSensorsConfigForMeasurementIndex(nextMeasurementIndex);

    uint32_t index = nextMeasurementIndex >= config.baselineMeasurementIndex ? nextMeasurementIndex - config.baselineMeasurementIndex : 0;
    Clock::time_point tp = config.baselineMeasurementTimePoint + config.descriptor.measurementPeriod * index;
    return tp;
}

///////////////////////////////////////////////////////////////////////////////////////////

DB::Clock::time_point DB::computeNextCommsTimePoint(Sensor const& /*sensor*/, size_t sensorIndex) const
{
    SensorsConfig const& config = getLastSensorsConfig();
    Clock::duration period = computeActualCommsPeriod(config.descriptor);

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

size_t DB::getBaseStationCount() const
{
    std::lock_guard<std::recursive_mutex> lg(m_mainDataMutex);

    return m_mainData.baseStations.size();
}

//////////////////////////////////////////////////////////////////////////

DB::BaseStation const& DB::getBaseStation(size_t index) const
{
    std::lock_guard<std::recursive_mutex> lg(m_mainDataMutex);

    assert(index < m_mainData.baseStations.size());
    return m_mainData.baseStations[index];
}

//////////////////////////////////////////////////////////////////////////

bool DB::addBaseStation(BaseStationDescriptor const& descriptor)
{
    std::lock_guard<std::recursive_mutex> lg(m_mainDataMutex);

    if (findBaseStationIndexByName(descriptor.name) >= 0)
    {
        return false;
    }
    if (findBaseStationIndexByMac(descriptor.mac) >= 0)
    {
        return false;
    }

    BaseStationDescriptor::Mac const& mac = descriptor.mac;
    char macStr[128];
    sprintf(macStr, "%X_%X_%X_%X_%X_%X", mac[0]&0xFF, mac[1]&0xFF, mac[2]&0xFF, mac[3]&0xFF, mac[4]&0xFF, mac[5]&0xFF);
    std::string dbName = macStr;

    std::unique_ptr<DB> db(new DB());
    if (db->load() != success)
    {
        if (db->create() != success)
        {
            s_logger.logCritical(QString("Cannot open nor create a DB for Base Station '%1' / %2").arg(descriptor.name.c_str()).arg(macStr).toUtf8().data());
            return false;
        }
    }

    s_logger.logInfo(QString("Adding base station '%1' / %2").arg(descriptor.name.c_str()).arg(getMacStr(descriptor.mac).c_str()));

    BaseStation baseStation;
    baseStation.descriptor = descriptor;
    baseStation.id = ++m_mainData.lastBaseStationId;

    m_mainData.baseStations.push_back(baseStation);
    emit baseStationAdded(baseStation.id);

    triggerSave();

    return true;
}

//////////////////////////////////////////////////////////////////////////

bool DB::setBaseStation(BaseStationId id, BaseStationDescriptor const& descriptor)
{
    std::lock_guard<std::recursive_mutex> lg(m_mainDataMutex);

    int32_t _index = findBaseStationIndexByName(descriptor.name);
    if (_index >= 0 && getBaseStation(static_cast<size_t>(_index)).id != id)
    {
        return false;
    }
    _index = findBaseStationIndexByMac(descriptor.mac);
    if (_index >= 0 && getBaseStation(static_cast<size_t>(_index)).id != id)
    {
        return false;
    }

    _index = findBaseStationIndexById(id);
    if (_index < 0)
    {
        return false;
    }

    size_t index = static_cast<size_t>(_index);

    s_logger.logInfo(QString("Changing base station '%1' / %2").arg(descriptor.name.c_str()).arg(getMacStr(descriptor.mac).c_str()));

    m_mainData.baseStations[index].descriptor = descriptor;
    emit baseStationChanged(id);

    triggerSave();

    return true;
}

//////////////////////////////////////////////////////////////////////////

void DB::removeBaseStation(size_t index)
{
    std::lock_guard<std::recursive_mutex> lg(m_mainDataMutex);

    assert(index < m_mainData.baseStations.size());
    BaseStationId id = m_mainData.baseStations[index].id;

    BaseStationDescriptor const& descriptor = m_mainData.baseStations[index].descriptor;
    s_logger.logInfo(QString("Removing base station '%1' / %2").arg(descriptor.name.c_str()).arg(getMacStr(descriptor.mac).c_str()));

    m_mainData.baseStations.erase(m_mainData.baseStations.begin() + index);
    emit baseStationRemoved(id);

    triggerSave();
}

//////////////////////////////////////////////////////////////////////////

int32_t DB::findBaseStationIndexByName(std::string const& name) const
{
    std::lock_guard<std::recursive_mutex> lg(m_mainDataMutex);

    auto it = std::find_if(m_mainData.baseStations.begin(), m_mainData.baseStations.end(), [&name](BaseStation const& baseStation) { return baseStation.descriptor.name == name; });
    if (it == m_mainData.baseStations.end())
    {
        return -1;
    }
    return std::distance(m_mainData.baseStations.begin(), it);
}

//////////////////////////////////////////////////////////////////////////

int32_t DB::findBaseStationIndexById(BaseStationId id) const
{
    std::lock_guard<std::recursive_mutex> lg(m_mainDataMutex);

    auto it = std::find_if(m_mainData.baseStations.begin(), m_mainData.baseStations.end(), [id](BaseStation const& baseStation) { return baseStation.id == id; });
    if (it == m_mainData.baseStations.end())
    {
        return -1;
    }
    return std::distance(m_mainData.baseStations.begin(), it);
}

//////////////////////////////////////////////////////////////////////////

int32_t DB::findBaseStationIndexByMac(BaseStationDescriptor::Mac const& mac) const
{
    std::lock_guard<std::recursive_mutex> lg(m_mainDataMutex);

    auto it = std::find_if(m_mainData.baseStations.begin(), m_mainData.baseStations.end(), [&mac](BaseStation const& baseStation) { return baseStation.descriptor.mac == mac; });
    if (it == m_mainData.baseStations.end())
    {
        return -1;
    }
    return std::distance(m_mainData.baseStations.begin(), it);
}

//////////////////////////////////////////////////////////////////////////

size_t DB::getSensorCount() const
{
    std::lock_guard<std::recursive_mutex> lg(m_mainDataMutex);
    return m_mainData.sensors.size();
}

//////////////////////////////////////////////////////////////////////////

DB::Sensor const& DB::getSensor(size_t index) const
{
    std::lock_guard<std::recursive_mutex> lg(m_mainDataMutex);
    assert(index < m_mainData.sensors.size());
    Sensor const& sensor = m_mainData.sensors[index];
    return sensor;
}

///////////////////////////////////////////////////////////////////////////////////////////

DB::SensorOutputDetails DB::computeSensorOutputDetails(SensorId id) const
{
    std::lock_guard<std::recursive_mutex> lg(m_mainDataMutex);

    int32_t _index = findSensorIndexById(id);
    if (_index < 0)
    {
        return {};
    }

    size_t index = static_cast<size_t>(_index);
    Sensor const& sensor = m_mainData.sensors[index];
    SensorsConfig const& config = getLastSensorsConfig();

    SensorOutputDetails details;
    details.commsPeriod = computeActualCommsPeriod(config.descriptor);
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
    std::lock_guard<std::recursive_mutex> lg(m_mainDataMutex);

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
        uint32_t index = nextRealTimeMeasurementIndex >= c.baselineMeasurementIndex ? nextRealTimeMeasurementIndex - c.baselineMeasurementIndex : 0;
        Clock::time_point nextMeasurementTP = c.baselineMeasurementTimePoint + c.descriptor.measurementPeriod * index;

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

    m_mainData.sensorsConfigs.push_back(config);
    while (m_mainData.sensorsConfigs.size() > 100)
    {
        m_mainData.sensorsConfigs.erase(m_mainData.sensorsConfigs.begin());
    }
    m_mainData.sensorsConfigs.back().computedCommsPeriod = computeActualCommsPeriod(m_mainData.sensorsConfigs.back().descriptor); //make sure the computed commd config is up-to-date

    emit sensorsConfigAdded();

    s_logger.logInfo("Added sensors config");

    triggerDelayedSave(std::chrono::seconds(1));

    return success;
}

//////////////////////////////////////////////////////////////////////////

Result<void> DB::setSensorsConfigs(std::vector<SensorsConfig> const& configs)
{
    std::lock_guard<std::recursive_mutex> lg(m_mainDataMutex);

    m_mainData.sensorsConfigs = configs;
    for (SensorsConfig& config: m_mainData.sensorsConfigs)
    {
        config.computedCommsPeriod = config.descriptor.commsPeriod;
    }
    if (!m_mainData.sensorsConfigs.empty())
    {
        SensorsConfig& config = m_mainData.sensorsConfigs.back();
        config.computedCommsPeriod = computeActualCommsPeriod(config.descriptor);
    }

    emit sensorsConfigChanged();

    s_logger.logInfo("Changed sensors configs");

    triggerDelayedSave(std::chrono::seconds(1));
    return success;
}

//////////////////////////////////////////////////////////////////////////

DB::SensorsConfig const& DB::getLastSensorsConfig() const
{
    std::lock_guard<std::recursive_mutex> lg(m_mainDataMutex);

    static SensorsConfig s_empty;
    return m_mainData.sensorsConfigs.empty() ? s_empty : m_mainData.sensorsConfigs.back();
}

//////////////////////////////////////////////////////////////////////////

size_t DB::getSensorsConfigCount() const
{
    std::lock_guard<std::recursive_mutex> lg(m_mainDataMutex);
    return m_mainData.sensorsConfigs.size();
}

//////////////////////////////////////////////////////////////////////////

DB::SensorsConfig const& DB::getSensorsConfig(size_t index) const
{
    std::lock_guard<std::recursive_mutex> lg(m_mainDataMutex);
    assert(index < m_mainData.sensorsConfigs.size());
    return m_mainData.sensorsConfigs[index];
}

//////////////////////////////////////////////////////////////////////////

DB::SensorsConfig const& DB::findSensorsConfigForMeasurementIndex(uint32_t index) const
{
    std::lock_guard<std::recursive_mutex> lg(m_mainDataMutex);

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
    std::lock_guard<std::recursive_mutex> lg(m_mainDataMutex);

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

        m_mainData.sensors.push_back(sensor);
        emit sensorAdded(sensor.id);

        s_logger.logInfo(QString("Added sensor '%1'").arg(descriptor.name.c_str()));
    }

    //refresh computed comms period as new sensors are added
    if (!m_mainData.sensorsConfigs.empty())
    {
        SensorsConfig& config = m_mainData.sensorsConfigs.back();
        config.computedCommsPeriod = computeActualCommsPeriod(config.descriptor);
        emit sensorsConfigChanged();
    }

    triggerDelayedSave(std::chrono::seconds(1));

    return success;
}

//////////////////////////////////////////////////////////////////////////

Result<void> DB::setSensor(SensorId id, SensorDescriptor const& descriptor)
{
    std::lock_guard<std::recursive_mutex> lg(m_mainDataMutex);

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
    std::lock_guard<std::recursive_mutex> lg(m_mainDataMutex);

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
    sensor.deviceInfo.sensorType = sensorType;
    sensor.deviceInfo.hardwareVersion = hardwareVersion;
    sensor.deviceInfo.softwareVersion = softwareVersion;
    sensor.calibration = calibration;
    sensor.state = Sensor::State::Active;
    sensor.serialNumber = serialNumber;

    emit sensorBound(sensor.id);
    emit sensorChanged(sensor.id);

    s_logger.logInfo(QString("Sensor '%1' bound to address %2").arg(sensor.descriptor.name.c_str()).arg(sensor.address));

    //refresh computed comms period as new sensors are added
    if (!m_mainData.sensorsConfigs.empty())
    {
        SensorsConfig& config = m_mainData.sensorsConfigs.back();
        config.computedCommsPeriod = computeActualCommsPeriod(config.descriptor);
        emit sensorsConfigChanged();
    }

    triggerDelayedSave(std::chrono::seconds(1));

    return sensor.id;
}

//////////////////////////////////////////////////////////////////////////

Result<void> DB::setSensorCalibration(SensorId id, Sensor::Calibration const& calibration)
{
    std::lock_guard<std::recursive_mutex> lg(m_mainDataMutex);

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
    std::lock_guard<std::recursive_mutex> lg(m_mainDataMutex);

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
    std::lock_guard<std::recursive_mutex> lg(m_mainDataMutex);

    return setSensorsInputDetails({details});
}

//////////////////////////////////////////////////////////////////////////

bool DB::setSensorsInputDetails(std::vector<SensorInputDetails> const& details)
{
    std::lock_guard<std::recursive_mutex> lg(m_mainDataMutex);

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

        if (d.hasDeviceInfo)
        {
            sensor.deviceInfo = d.deviceInfo;
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

        if (d.hasMeasurement)
        {
            sensor.isMeasurementValid = true;
            sensor.measurementTemperature = d.measurementTemperature;
            sensor.measurementHumidity = d.measurementHumidity;
            sensor.measurementVcc = d.measurementVcc;
        }

        emit sensorDataChanged(sensor.id);
    }

    triggerDelayedSave(std::chrono::seconds(1));

    return ok;
}

//////////////////////////////////////////////////////////////////////////

void DB::removeSensor(size_t index)
{
    std::lock_guard<std::recursive_mutex> lg(m_mainDataMutex);

    assert(index < m_mainData.sensors.size());

    SensorId sensorId = m_mainData.sensors[index].id;

    {
        auto it = m_mainData.measurements.find(sensorId);
        if (it != m_mainData.measurements.end())
        {
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

    for (Alarm& alarm: m_mainData.alarms)
    {
        alarm.descriptor.sensors.erase(sensorId);
        alarm.triggersPerSensor.erase(sensorId);
    }

    for (Report& report: m_mainData.reports)
    {
        report.descriptor.sensors.erase(sensorId);
    }

    s_logger.logInfo(QString("Removing sensor '%1'").arg(m_mainData.sensors[index].descriptor.name.c_str()));

    m_mainData.sensors.erase(m_mainData.sensors.begin() + index);
    emit sensorRemoved(sensorId);

    //refresh computed comms period as new sensors are removed
    if (!m_mainData.sensorsConfigs.empty())
    {
        SensorsConfig& config = m_mainData.sensorsConfigs.back();
        config.computedCommsPeriod = computeActualCommsPeriod(config.descriptor);
        emit sensorsConfigChanged();
    }

    triggerDelayedSave(std::chrono::seconds(1));
}

//////////////////////////////////////////////////////////////////////////

int32_t DB::findSensorIndexByName(std::string const& name) const
{
    std::lock_guard<std::recursive_mutex> lg(m_mainDataMutex);

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
    std::lock_guard<std::recursive_mutex> lg(m_mainDataMutex);

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
    std::lock_guard<std::recursive_mutex> lg(m_mainDataMutex);

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
    std::lock_guard<std::recursive_mutex> lg(m_mainDataMutex);

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
    std::lock_guard<std::recursive_mutex> lg(m_mainDataMutex);

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
    std::lock_guard<std::recursive_mutex> lg(m_mainDataMutex);

    return m_mainData.alarms.size();
}

//////////////////////////////////////////////////////////////////////////

DB::Alarm const& DB::getAlarm(size_t index) const
{
    std::lock_guard<std::recursive_mutex> lg(m_mainDataMutex);

    assert(index < m_mainData.alarms.size());
    return m_mainData.alarms[index];
}

//////////////////////////////////////////////////////////////////////////

Result<void> DB::addAlarm(AlarmDescriptor const& descriptor)
{
    std::lock_guard<std::recursive_mutex> lg(m_mainDataMutex);

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

    m_mainData.alarms.push_back(alarm);
    emit alarmAdded(alarm.id);

    s_logger.logInfo(QString("Added alarm '%1'").arg(descriptor.name.c_str()));

    triggerDelayedSave(std::chrono::seconds(1));

    return success;
}

//////////////////////////////////////////////////////////////////////////

Result<void> DB::setAlarm(AlarmId id, AlarmDescriptor const& descriptor)
{
    std::lock_guard<std::recursive_mutex> lg(m_mainDataMutex);

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
    std::lock_guard<std::recursive_mutex> lg(m_mainDataMutex);

    assert(index < m_mainData.alarms.size());
    AlarmId id = m_mainData.alarms[index].id;

    s_logger.logInfo(QString("Removed alarm '%1'").arg(m_mainData.alarms[index].descriptor.name.c_str()));

    m_mainData.alarms.erase(m_mainData.alarms.begin() + index);
    emit alarmRemoved(id);

    triggerDelayedSave(std::chrono::seconds(1));
}

//////////////////////////////////////////////////////////////////////////

int32_t DB::findAlarmIndexByName(std::string const& name) const
{
    std::lock_guard<std::recursive_mutex> lg(m_mainDataMutex);

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
    std::lock_guard<std::recursive_mutex> lg(m_mainDataMutex);

    auto it = std::find_if(m_mainData.alarms.begin(), m_mainData.alarms.end(), [id](Alarm const& alarm) { return alarm.id == id; });
    if (it == m_mainData.alarms.end())
    {
        return -1;
    }
    return std::distance(m_mainData.alarms.begin(), it);
}

//////////////////////////////////////////////////////////////////////////

uint8_t DB::computeAlarmTriggers(Measurement const& m)
{
    std::lock_guard<std::recursive_mutex> lg(m_mainDataMutex);

    uint8_t allTriggers = 0;

    for (Alarm& alarm: m_mainData.alarms)
    {
        uint8_t triggers = _computeAlarmTriggers(alarm, m);
        allTriggers |= triggers;
    }

    return allTriggers;
}

//////////////////////////////////////////////////////////////////////////

uint8_t DB::_computeAlarmTriggers(Alarm& alarm, Measurement const& m)
{
    std::lock_guard<std::recursive_mutex> lg(m_mainDataMutex);

    AlarmDescriptor const& ad = alarm.descriptor;
    if (ad.filterSensors)
    {
        if (ad.sensors.find(m.descriptor.sensorId) == ad.sensors.end())
        {
            return 0;
        }
    }

    uint8_t triggers = 0;
    if (ad.highTemperatureWatch && m.descriptor.temperature > ad.highTemperature)
    {
        triggers |= AlarmTrigger::Temperature;
    }
    if (ad.lowTemperatureWatch && m.descriptor.temperature < ad.lowTemperature)
    {
        triggers |= AlarmTrigger::Temperature;
    }

    if (ad.highHumidityWatch && m.descriptor.humidity > ad.highHumidity)
    {
        triggers |= AlarmTrigger::Humidity;
    }
    if (ad.lowHumidityWatch && m.descriptor.humidity < ad.lowHumidity)
    {
        triggers |= AlarmTrigger::Humidity;
    }

    float alertLevelVcc = (k_alertPercentageVcc / 100.f) * (k_maxBatteryLevel - k_minBatteryLevel) + k_minBatteryLevel;
    if (ad.lowVccWatch && m.descriptor.vcc <= alertLevelVcc)
    {
        triggers |= AlarmTrigger::LowVcc;
    }
//        if (ad.sensorErrorsWatch && m.descriptor.sensorErrors != 0)
//        {
//            triggers |= AlarmTrigger::SensorErrors;
//        }

    float alertLevelSignal = (k_alertPercentageSignal / 100.f) * (k_maxSignalLevel - k_minSignalLevel) + k_minSignalLevel;
    if (ad.lowSignalWatch && m.combinedSignalStrength <= alertLevelSignal)
    {
        triggers |= AlarmTrigger::LowSignal;
    }

    uint8_t oldTriggers = 0;
    auto it = alarm.triggersPerSensor.find(m.descriptor.sensorId);
    if (it != alarm.triggersPerSensor.end())
    {
        oldTriggers = it->second;
    }

    if (triggers != oldTriggers)
    {
        if (triggers == 0)
        {
            alarm.triggersPerSensor.erase(m.descriptor.sensorId);
        }
        else
        {
            alarm.triggersPerSensor[m.descriptor.sensorId] = triggers;
        }
    }

    uint8_t diff = oldTriggers ^ triggers;
    if (diff != 0)
    {
        uint8_t removed = diff & oldTriggers;
        uint8_t added = diff & triggers;

        s_logger.logInfo(QString("Alarm '%1' triggers for measurement index %2 have changed: old %3, new %4, added %5, removed %6")
                         .arg(ad.name.c_str())
                         .arg(m.descriptor.index)
                         .arg(oldTriggers)
                         .arg(triggers)
                         .arg(added)
                         .arg(removed));

        emit alarmTriggersChanged(alarm.id, m, oldTriggers, triggers, added, removed);
    }

    return triggers;
}

//////////////////////////////////////////////////////////////////////////

size_t DB::getReportCount() const
{
    std::lock_guard<std::recursive_mutex> lg(m_mainDataMutex);

    return m_mainData.reports.size();
}

//////////////////////////////////////////////////////////////////////////

DB::Report const& DB::getReport(size_t index) const
{
    std::lock_guard<std::recursive_mutex> lg(m_mainDataMutex);

    assert(index < m_mainData.reports.size());
    return m_mainData.reports[index];
}

//////////////////////////////////////////////////////////////////////////

Result<void> DB::addReport(ReportDescriptor const& descriptor)
{
    std::lock_guard<std::recursive_mutex> lg(m_mainDataMutex);

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

    m_mainData.reports.push_back(report);
    emit reportAdded(report.id);

    s_logger.logInfo(QString("Added report '%1'").arg(descriptor.name.c_str()));

    triggerDelayedSave(std::chrono::seconds(1));

    return success;
}

//////////////////////////////////////////////////////////////////////////

Result<void> DB::setReport(ReportId id, ReportDescriptor const& descriptor)
{
    std::lock_guard<std::recursive_mutex> lg(m_mainDataMutex);

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
    std::lock_guard<std::recursive_mutex> lg(m_mainDataMutex);

    assert(index < m_mainData.reports.size());
    ReportId id = m_mainData.reports[index].id;

    s_logger.logInfo(QString("Removed report '%1'").arg(m_mainData.reports[index].descriptor.name.c_str()));

    m_mainData.reports.erase(m_mainData.reports.begin() + index);
    emit reportRemoved(id);

    triggerDelayedSave(std::chrono::seconds(1));
}

//////////////////////////////////////////////////////////////////////////

int32_t DB::findReportIndexByName(std::string const& name) const
{
    std::lock_guard<std::recursive_mutex> lg(m_mainDataMutex);

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
    std::lock_guard<std::recursive_mutex> lg(m_mainDataMutex);

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
    std::lock_guard<std::recursive_mutex> lg(m_mainDataMutex);

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
    std::lock_guard<std::recursive_mutex> lg(m_mainDataMutex);

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
    std::lock_guard<std::recursive_mutex> lg(m_mainDataMutex);

    return _addMeasurements(md.sensorId, {md});
}

//////////////////////////////////////////////////////////////////////////

bool DB::addMeasurements(std::vector<MeasurementDescriptor> const& mds)
{
    std::lock_guard<std::recursive_mutex> lg(m_mainDataMutex);

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
    std::lock_guard<std::recursive_mutex> lg(m_mainDataMutex);

    int32_t _sensorIndex = findSensorIndexById(sensorId);
    if (_sensorIndex < 0)
    {
        return false;
    }
    size_t sensorIndex = static_cast<size_t>(_sensorIndex);
    Sensor& sensor = m_mainData.sensors[sensorIndex];

    uint32_t rtIndex = computeNextRealTimeMeasurementIndex();

    //first remove already confirmed measurements or future measurements
    mds.erase(std::remove_if(mds.begin(), mds.end(), [&sensor, rtIndex](MeasurementDescriptor const& d)
    {
        //leave some threshold for the RT check, to account for inaccuracies in time keeping
        return d.index < sensor.lastConfirmedMeasurementIndex || d.index > rtIndex + 5;
    }), mds.end());

    if (mds.empty())
    {
        return true;
    }

    bool added = false;
    uint32_t minIndex = std::numeric_limits<uint32_t>::max();
    uint32_t maxIndex = std::numeric_limits<uint32_t>::lowest();
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
        measurement.combinedSignalStrength = std::min(md.signalStrength.b2s, md.signalStrength.s2b);
        measurement.alarmTriggers = computeAlarmTriggers(measurement);

        //this is also set in the setSensorInputDetails
        if (!sensor.isMeasurementValid)
        {
            sensor.isMeasurementValid = true;
            sensor.measurementTemperature = measurement.descriptor.temperature;
            sensor.measurementHumidity = measurement.descriptor.humidity;
            sensor.measurementVcc = measurement.descriptor.vcc;
        }

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
        minIndex = std::min(minIndex, md.index);
        maxIndex = std::max(maxIndex, md.index);
        added = true;
    }

    if (!added)
    {
        return true;
    }
    s_logger.logVerbose(QString("Added measurement indices %1 to %2, sensor '%3'").arg(minIndex).arg(maxIndex).arg(sensor.descriptor.name.c_str()));

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
    std::lock_guard<std::recursive_mutex> lg(m_mainDataMutex);

    SensorsConfig const& config = findSensorsConfigForMeasurementIndex(md.index);
    uint32_t index = md.index >= config.baselineMeasurementIndex ? md.index - config.baselineMeasurementIndex : 0;
    Clock::time_point tp = config.baselineMeasurementTimePoint + config.descriptor.measurementPeriod * index;
    return tp;
}

//////////////////////////////////////////////////////////////////////////

std::vector<DB::Measurement> DB::getAllMeasurements() const
{
    std::lock_guard<std::recursive_mutex> lg(m_mainDataMutex);

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
    std::lock_guard<std::recursive_mutex> lg(m_mainDataMutex);

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
    std::lock_guard<std::recursive_mutex> lg(m_mainDataMutex);

    std::vector<DB::Measurement> result;
    result.reserve(8192);
    _getFilteredMeasurements(filter, &result);
    return result;
}

//////////////////////////////////////////////////////////////////////////

size_t DB::getFilteredMeasurementCount(Filter const& filter) const
{
    std::lock_guard<std::recursive_mutex> lg(m_mainDataMutex);

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
    std::lock_guard<std::recursive_mutex> lg(m_mainDataMutex);

    auto it = m_mainData.measurements.find(sensor_id);
    if (it == m_mainData.measurements.end())
    {
        return Error("Sensor not found");
    }

    const StoredMeasurements& storedMeasurements = it->second;
    if (storedMeasurements.empty())
    {
        return Error("No data");
    }
    return unpack(sensor_id, storedMeasurements.back());
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
    sm.alarmTriggers = m.alarmTriggers;
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
    m.alarmTriggers = sm.alarmTriggers;
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

    Data data;
    {
        std::lock_guard<std::recursive_mutex> lg(m_mainDataMutex);
        data = m_mainData;
    }

    {
        std::unique_lock<std::mutex> lg(m_storeMutex);
        m_storeData = std::move(data);
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
        std::stringstream jsonString;
        {
	        cereal::JSONOutputArchive archive(jsonString);
	        archive(cereal::make_nvp("db", data));
        }

        std::string tempFilename = (s_dataFolder + "/" + m_dataName + "_temp");
        {
            std::ofstream file(tempFilename, std::ios_base::binary);
            if (!file.is_open())
            {
                s_logger.logCritical(QString("Failed to open '%1': %2").arg(tempFilename.c_str()).arg(std::strerror(errno)));
            }
            else
            {
                std::string str = jsonString.str();
				Crypt crypt;
				crypt.setCompressionLevel(-1); //default
				crypt.setKey(k_fileEncryptionKey);
#ifdef USE_DATA_ENCRYPTION
				QByteArray dataToWrite = crypt.encryptToByteArray(QByteArray(str.data(), (int)str.size()));
#else
				QByteArray dataToWrite = QByteArray(str.data(), (int)str.size());
#endif
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
        QByteArray dataToWrite = crypt.encryptToByteArray(QByteArray(buffer.data(), (int)buffer.size()));
#else
        QByteArray dataToWrite = QByteArray(buffer.data(), (int)buffer.size());
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

    std::cout << QString("Done saving DB to '%1' & '%2'. Time: %3s\n").arg(m_dataName.c_str()).arg(m_dbName.c_str()).arg(std::chrono::duration<float>(Clock::now() - start).count()).toUtf8().data();
    std::cout.flush();

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

