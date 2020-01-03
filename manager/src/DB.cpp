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
#include <QByteArray>

#include "Utils.h"
#include "Crypt.h"
#include "Logger.h"
#include "sqlite3.h"

extern Logger s_logger;

const DB::Clock::duration MEASUREMENT_JITTER = std::chrono::seconds(10);
const DB::Clock::duration COMMS_DURATION = std::chrono::seconds(10);

extern std::string getMacStr(DB::BaseStationDescriptor::Mac const& mac);

//////////////////////////////////////////////////////////////////////////

DB::DB()
{
}

//////////////////////////////////////////////////////////////////////////

DB::~DB()
{
}

//////////////////////////////////////////////////////////////////////////

Result<void> DB::create(sqlite3& db)
{
	sqlite3_exec(&db, "BEGIN TRANSACTION;", NULL, NULL, NULL);
	utils::epilogue epi([&db] { sqlite3_exec(&db, "END TRANSACTION;", NULL, NULL, NULL); });

	{
		const char* sql = "CREATE TABLE BaseStations (id INTEGER PRIMARY KEY, name STRING, mac STRING);";
		if (sqlite3_exec(&db, sql, NULL, NULL, nullptr))
		{
			Error error(QString("Error executing SQLite3 statement: %1").arg(sqlite3_errmsg(&db)).toUtf8().data());
			return error;
		}
	}
	{
		const char* sql = "CREATE TABLE SensorsConfigs (baselineMeasurementTimePoint DATETIME, baselineMeasurementIndex INTEGER, sensorsPower INTEGER, measurementPeriod INTEGER, commsPeriod INTEGER);";
		if (sqlite3_exec(&db, sql, NULL, NULL, nullptr))
		{
			Error error(QString("Error executing SQLite3 statement: %1").arg(sqlite3_errmsg(&db)).toUtf8().data());
			return error;
		}
	}
	{
		const char* sql = "CREATE TABLE Sensors (id INTEGER PRIMARY KEY, name STRING, address INTEGER, sensorType INTEGER, hardwareVersion INTEGER, softwareVersion INTEGER, "
            "temperatureBias REAL, humidityBias REAL, serialNumber INTEGER, state INTEGER, shouldSleep BOOLEAN, sleepStateTimePoint DATETIME, commsErrorCounter INTEGER, "
            "unknownRebootsErrorCounter INTEGER, powerOnRebootsErrorCounter INTEGER, resetRebootsErrorCounter INTEGER, brownoutRebootsErrorCounter INTEGER, watchdogRebootsErrorCounter INTEGER, lastCommsTimePoint DATETIME, lastConfirmedMeasurementIndex INTEGER, "
            "firstStoredMeasurementIndex INTEGER, storedMeasurementCount INTEGER, estimatedStoredMeasurementCount INTEGER, lastSignalStrengthB2S INTEGER, averageSignalStrengthB2S INTEGER, averageSignalStrengthS2B INTEGER, "
            "isMeasurementValid BOOLEAN, measurementTemperature REAL, measurementHumidity REAL, measurementVcc REAL);";
		if (sqlite3_exec(&db, sql, NULL, NULL, nullptr))
		{
			Error error(QString("Error executing SQLite3 statement: %1").arg(sqlite3_errmsg(&db)).toUtf8().data());
			return error;
		}
	}
	{
		const char* sql = "CREATE TABLE Alarms (id INTEGER PRIMARY KEY, name STRING, filterSensors BOOLEAN, sensors STRING, lowTemperatureWatch BOOLEAN, lowTemperature REAL, highTemperatureWatch BOOLEAN, highTemperature REAL, "
            "lowHumidityWatch BOOLEAN, lowHumidity REAL, highHumidityWatch BOOLEAN, highHumidity REAL, "
            "lowVccWatch BOOLEAN, lowSignalWatch BOOLEAN, "
            "sendEmailAction BOOLEAN);";
		if (sqlite3_exec(&db, sql, NULL, NULL, nullptr))
		{
			Error error(QString("Error executing SQLite3 statement: %1").arg(sqlite3_errmsg(&db)).toUtf8().data());
			return error;
		}
	}
	{
		const char* sql = "CREATE TABLE Reports (id INTEGER PRIMARY KEY, name STRING, period INTEGER, customPeriod INTEGER, data INTEGER, filterSensors BOOLEAN, sensors STRING, lastTriggeredTimePoint DATETIME);";
		if (sqlite3_exec(&db, sql, NULL, NULL, nullptr))
		{
			Error error(QString("Error executing SQLite3 statement: %1").arg(sqlite3_errmsg(&db)).toUtf8().data());
			return error;
		}
	} 
    {
		const char* sql = "CREATE TABLE Measurements (id INTEGER PRIMARY KEY, timePoint DATETIME, idx INTEGER, sensorId INTEGER, temperature REAL, humidity REAL, vcc REAL, signalStrengthS2B INTEGER, signalStrengthB2S INTEGER, "
            "sensorErrors INTEGER, alarmTriggers INTEGER);";
		if (sqlite3_exec(&db, sql, NULL, NULL, nullptr))
		{
			Error error(QString("Error executing SQLite3 statement: %1").arg(sqlite3_errmsg(&db)).toUtf8().data());
			return error;
		}
	}

	return success;
}

//////////////////////////////////////////////////////////////////////////

Result<void> DB::load(sqlite3& db)
{
    Clock::time_point start = Clock::now();

    Data data;

    {
		sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(&db, "REPLACE INTO BaseStations (id, name, mac) "
                                    "VALUES (?1, ?2, ?3);", -1, &stmt, NULL) != SQLITE_OK)
		{
			return Error(QString("Cannot load base stations: %1").arg(sqlite3_errmsg(&db)).toUtf8().data());
		}
        m_saveBaseStationsStmt.reset(stmt, &sqlite3_finalize);
    }
	{
		sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(&db, "INSERT INTO SensorsConfigs (baselineMeasurementTimePoint, baselineMeasurementIndex, sensorsPower, measurementPeriod, commsPeriod) "
						            "VALUES (?1, ?2, ?3, ?4, ?5);", -1, &stmt, NULL) != SQLITE_OK)
		{
			return Error(QString("Cannot load base stations: %1").arg(sqlite3_errmsg(&db)).toUtf8().data());
		}
		m_saveSensorsConfigsStmt.reset(stmt, &sqlite3_finalize);
	}
	{
		sqlite3_stmt* stmt;
		if (sqlite3_prepare_v2(&db, "REPLACE INTO Sensors (id, name, address, sensorType, hardwareVersion, softwareVersion, "
						                       "temperatureBias, humidityBias, serialNumber, state, shouldSleep, sleepStateTimePoint, commsErrorCounter, "
						                       "unknownRebootsErrorCounter, powerOnRebootsErrorCounter, resetRebootsErrorCounter, brownoutRebootsErrorCounter, watchdogRebootsErrorCounter, lastCommsTimePoint, lastConfirmedMeasurementIndex, "
						                       "firstStoredMeasurementIndex, storedMeasurementCount, estimatedStoredMeasurementCount, lastSignalStrengthB2S, averageSignalStrengthB2S, averageSignalStrengthS2B, "
						                       "isMeasurementValid, measurementTemperature, measurementHumidity, measurementVcc) "
						            "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12, ?13, ?14, ?15, ?16, ?17, ?18, ?19, ?20, ?21, ?22, ?23, ?24, ?25, ?26, ?27, ?28, ?29, ?30);", -1, &stmt, NULL) != SQLITE_OK)
		{
			return Error(QString("Cannot load base stations: %1").arg(sqlite3_errmsg(&db)).toUtf8().data());
		}
		m_saveSensorsStmt.reset(stmt, &sqlite3_finalize);
	}
	{
		sqlite3_stmt* stmt;
		if (sqlite3_prepare_v2(&db, "REPLACE INTO Alarms (id, name, filterSensors, sensors, lowTemperatureWatch, lowTemperature, highTemperatureWatch, highTemperature, "
						                       "lowHumidityWatch, lowHumidity, highHumidityWatch, highHumidity, "
						                       "lowVccWatch, lowSignalWatch, "
						                       "sendEmailAction) "
							        "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12, ?13, ?14, ?15);", -1, &stmt, NULL) != SQLITE_OK)
		{
			return Error(QString("Cannot load base stations: %1").arg(sqlite3_errmsg(&db)).toUtf8().data());
		}
		m_saveAlarmsStmt.reset(stmt, &sqlite3_finalize);
	}
	{
		sqlite3_stmt* stmt;
		if (sqlite3_prepare_v2(&db, "REPLACE INTO Reports (id, name, period, customPeriod, data, filterSensors, sensors, lastTriggeredTimePoint) "
							        "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8);", -1, &stmt, NULL) != SQLITE_OK)
		{
			return Error(QString("Cannot load base stations: %1").arg(sqlite3_errmsg(&db)).toUtf8().data());
		}
		m_saveReportsStmt.reset(stmt, &sqlite3_finalize);
    }
    {
		sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(&db, "REPLACE INTO Measurements (id, timePoint, idx, sensorId, temperature, humidity, vcc, signalStrengthS2B, signalStrengthB2S, sensorErrors, alarmTriggers) "
							        "VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11);", -1, &stmt, NULL) != SQLITE_OK)
		{
			return Error(QString("Cannot load base stations: %1").arg(sqlite3_errmsg(&db)).toUtf8().data());
		}
		m_addMeasurementsStmt.reset(stmt, &sqlite3_finalize);
    }

	{
		//const char* sql = "CREATE TABLE BaseStations (id INTEGER PRIMARY KEY, name STRING, mac STRING);";
		const char* sql = "SELECT id, name, mac "
                          "FROM BaseStations;";
		sqlite3_stmt* stmt;
		if (sqlite3_prepare_v2(&db, sql, -1, &stmt, 0) != SQLITE_OK)
		{
			return Error(QString("Cannot load base stations: %1").arg(sqlite3_errmsg(&db)).toUtf8().data());
		}
		utils::epilogue epi([stmt] { sqlite3_finalize(stmt); });

        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            BaseStation bs;
            bs.id = sqlite3_column_int64(stmt, 0);
		    bs.descriptor.name = (char const*)sqlite3_column_text(stmt, 1);
		    std::string macStr = (char const*)sqlite3_column_text(stmt, 2);
			int m0, m1, m2, m3, m4, m5;
            if (sscanf(macStr.c_str(), "%X:%X:%X:%X:%X:%X", &m0, &m1, &m2, &m3, &m4, &m5) != 6)
            {
                return Error(QString("Bad base station mac: %1").arg(macStr.c_str()).toUtf8().data());
            }
            bs.descriptor.mac = { static_cast<uint8_t>(m0 & 0xFF), static_cast<uint8_t>(m1 & 0xFF), static_cast<uint8_t>(m2 & 0xFF), static_cast<uint8_t>(m3 & 0xFF), static_cast<uint8_t>(m4 & 0xFF), static_cast<uint8_t>(m5 & 0xFF) };
            data.baseStations.push_back(std::move(bs));
        }
	}

	{
		//const char* sql = "CREATE TABLE SensorsConfigs (baselineMeasurementTimePoint DATETIME, baselineMeasurementIndex INTEGER, sensorsPower INTEGER, measurementPeriod INTEGER, commsPeriod INTEGER);";
		const char* sql = "SELECT baselineMeasurementTimePoint, baselineMeasurementIndex, sensorsPower, measurementPeriod, commsPeriod "
                          "FROM SensorsConfigs;";
		sqlite3_stmt* stmt;
		if (sqlite3_prepare_v2(&db, sql, -1, &stmt, 0) != SQLITE_OK)
		{
			return Error(QString("Cannot load sensors configs: %1").arg(sqlite3_errmsg(&db)).toUtf8().data());
		}
		utils::epilogue epi([stmt] { sqlite3_finalize(stmt); });

		while (sqlite3_step(stmt) == SQLITE_ROW)
		{
            SensorsConfig sc;
			sc.baselineMeasurementTimePoint = Clock::from_time_t(sqlite3_column_int64(stmt, 0));
			sc.baselineMeasurementIndex = sqlite3_column_int64(stmt, 1);
			sc.descriptor.sensorsPower = (int8_t)sqlite3_column_int64(stmt, 2);
			sc.descriptor.measurementPeriod = std::chrono::seconds(sqlite3_column_int64(stmt, 3));
			sc.descriptor.commsPeriod = std::chrono::seconds(sqlite3_column_int64(stmt, 4));
			data.sensorsConfigs.push_back(std::move(sc));
		}
	}

	{
		const char* sql = "SELECT id, name, address, sensorType, hardwareVersion, softwareVersion, "
			 			        "temperatureBias, humidityBias, serialNumber, state, shouldSleep, sleepStateTimePoint, commsErrorCounter, "
			 			        "unknownRebootsErrorCounter, powerOnRebootsErrorCounter, resetRebootsErrorCounter, brownoutRebootsErrorCounter, watchdogRebootsErrorCounter, lastCommsTimePoint, lastConfirmedMeasurementIndex, "
			 			        "firstStoredMeasurementIndex, storedMeasurementCount, estimatedStoredMeasurementCount, lastSignalStrengthB2S, averageSignalStrengthB2S, averageSignalStrengthS2B, "
			 			        "isMeasurementValid, measurementTemperature, measurementHumidity, measurementVcc "
                          "FROM Sensors;";
		sqlite3_stmt* stmt;
		if (sqlite3_prepare_v2(&db, sql, -1, &stmt, 0) != SQLITE_OK)
		{
			return Error(QString("Cannot load sensors: %1").arg(sqlite3_errmsg(&db)).toUtf8().data());
		}
		utils::epilogue epi([stmt] { sqlite3_finalize(stmt); });

		while (sqlite3_step(stmt) == SQLITE_ROW)
		{
			Sensor s;
			s.id = sqlite3_column_int64(stmt, 0);
			s.descriptor.name = (char const*)sqlite3_column_text(stmt, 1);
			s.address = sqlite3_column_int64(stmt, 2);
			s.deviceInfo.sensorType = (uint8_t)sqlite3_column_int64(stmt, 3);
			s.deviceInfo.hardwareVersion = (uint8_t)sqlite3_column_int64(stmt, 4);
			s.deviceInfo.softwareVersion = (uint8_t)sqlite3_column_int64(stmt, 5);
			s.calibration.temperatureBias = (float)sqlite3_column_double(stmt, 6);
			s.calibration.humidityBias = (float)sqlite3_column_double(stmt, 7);
			s.serialNumber = (uint32_t)sqlite3_column_int64(stmt, 8);
			s.state = (Sensor::State)sqlite3_column_int64(stmt, 9);
			s.shouldSleep = sqlite3_column_int64(stmt, 10) ? true : false;
			s.sleepStateTimePoint = Clock::from_time_t(sqlite3_column_int64(stmt, 11));
			s.errorCounters.comms = (uint32_t)sqlite3_column_int64(stmt, 12);
			s.errorCounters.unknownReboots = (uint32_t)sqlite3_column_int64(stmt, 13);
			s.errorCounters.powerOnReboots = (uint32_t)sqlite3_column_int64(stmt, 14);
			s.errorCounters.resetReboots = (uint32_t)sqlite3_column_int64(stmt, 15);
			s.errorCounters.brownoutReboots = (uint32_t)sqlite3_column_int64(stmt, 16);
			s.errorCounters.watchdogReboots = (uint32_t)sqlite3_column_int64(stmt, 17);
			s.lastCommsTimePoint = Clock::from_time_t(sqlite3_column_int64(stmt, 18));
			s.lastConfirmedMeasurementIndex = (uint32_t)sqlite3_column_int64(stmt, 19);
			s.firstStoredMeasurementIndex = (uint32_t)sqlite3_column_int64(stmt, 20);
			s.storedMeasurementCount = (uint32_t)sqlite3_column_int64(stmt, 21);
			s.estimatedStoredMeasurementCount = (uint32_t)sqlite3_column_int64(stmt, 22);
			s.lastSignalStrengthB2S = (int8_t)sqlite3_column_int64(stmt, 23);
			s.averageSignalStrength.b2s = (int8_t)sqlite3_column_int64(stmt, 24);
			s.averageSignalStrength.s2b = (int8_t)sqlite3_column_int64(stmt, 25);
			s.isMeasurementValid = sqlite3_column_int64(stmt, 26) ? true : false;
			s.measurementTemperature = (float)sqlite3_column_double(stmt, 27);
			s.measurementHumidity = (float)sqlite3_column_double(stmt, 28);
			s.measurementVcc = (float)sqlite3_column_double(stmt, 29);
			data.sensors.push_back(std::move(s));
		}
	}

	{
// 		const char* sql = "CREATE TABLE Alarms (id INTEGER PRIMARY KEY, name, filterSensors BOOLEAN, sensors STRING, lowTemperatureWatch BOOLEAN, lowTemperature REAL, highTemperatureWatch BOOLEAN, highTemperature REAL, "
// 			"lowHumidityWatch BOOLEAN, lowHumidity REAL, highHumidityWatch BOOLEAN, highHumidity REAL, "
// 			"lowVccWatch BOOLEAN, lowSignalWatch BOOLEAN, "
// 			"sendEmailAction BOOLEAN);";
		const char* sql = "SELECT id, name, filterSensors, sensors, lowTemperatureWatch, lowTemperature, highTemperatureWatch, highTemperature, "
			 			            "lowHumidityWatch, lowHumidity, highHumidityWatch, highHumidity, "
			 			            "lowVccWatch, lowSignalWatch, sendEmailAction "
                          "FROM Alarms;";
		sqlite3_stmt* stmt;
		if (sqlite3_prepare_v2(&db, sql, -1, &stmt, 0) != SQLITE_OK)
		{
			return Error(QString("Cannot load alarms: %1").arg(sqlite3_errmsg(&db)).toUtf8().data());
		}
		utils::epilogue epi([stmt] { sqlite3_finalize(stmt); });

		while (sqlite3_step(stmt) == SQLITE_ROW)
		{
			Alarm a;
            a.id = sqlite3_column_int64(stmt, 0);
			a.descriptor.name = (char const*)sqlite3_column_text(stmt, 1);
            a.descriptor.filterSensors = sqlite3_column_int64(stmt, 2) ? true : false;
			QString sensors = (char const*)sqlite3_column_text(stmt, 3);
			QStringList l = sensors.split(QChar(';'), QString::SkipEmptyParts);
			for (QString str : l)
			{
				a.descriptor.sensors.insert(atoll(str.trimmed().toUtf8().data()));
			}
            a.descriptor.lowTemperatureWatch = sqlite3_column_int64(stmt, 4) ? true : false;
            a.descriptor.lowTemperature = (float)sqlite3_column_double(stmt, 5);
			a.descriptor.highTemperatureWatch = sqlite3_column_int64(stmt, 6) ? true : false;
			a.descriptor.highTemperature = (float)sqlite3_column_double(stmt, 7);
			a.descriptor.lowHumidityWatch = sqlite3_column_int64(stmt, 8) ? true : false;
			a.descriptor.lowHumidity = (float)sqlite3_column_double(stmt, 9);
			a.descriptor.highHumidityWatch = sqlite3_column_int64(stmt, 10) ? true : false;
			a.descriptor.highHumidity = (float)sqlite3_column_double(stmt, 11);
			a.descriptor.lowVccWatch = sqlite3_column_int64(stmt, 12);
			a.descriptor.lowSignalWatch = sqlite3_column_int64(stmt, 13);
			a.descriptor.sendEmailAction = sqlite3_column_int64(stmt, 14);
			data.alarms.push_back(std::move(a));
		}
	}
	{
		//const char* sql = "CREATE TABLE Reports (id INTEGER PRIMARY KEY, name STRING, period INTEGER, customPeriod INTEGER, data INTEGER, filterSensors BOOLEAN, sensors STRING, lastTriggeredTimePoint DATETIME);";
		const char* sql = "SELECT id, name, period, data, filterSensors, sensors, lastTriggeredTimePoint "
                          "FROM Reports;";
		sqlite3_stmt* stmt;
		if (sqlite3_prepare_v2(&db, sql, -1, &stmt, 0) != SQLITE_OK)
		{
			return Error(QString("Cannot load reports: %1").arg(sqlite3_errmsg(&db)).toUtf8().data());
		}
		utils::epilogue epi([stmt] { sqlite3_finalize(stmt); });

		while (sqlite3_step(stmt) == SQLITE_ROW)
		{
			Report r;
			r.id = sqlite3_column_int64(stmt, 0);
			r.descriptor.name = (char const*)sqlite3_column_text(stmt, 1);
			r.descriptor.period = (ReportDescriptor::Period)sqlite3_column_int64(stmt, 2);
			r.descriptor.customPeriod = std::chrono::seconds(sqlite3_column_int64(stmt, 3));
			r.descriptor.data = (ReportDescriptor::Data)sqlite3_column_int64(stmt, 4);
			r.descriptor.filterSensors = sqlite3_column_int64(stmt, 5) ? true : false;
			QString sensors = (char const*)sqlite3_column_text(stmt, 6);
			QStringList l = sensors.split(QChar(';'), QString::SkipEmptyParts);
			for (QString str : l)
			{
				r.descriptor.sensors.insert(atoll(str.trimmed().toUtf8().data()));
			}
			r.lastTriggeredTimePoint = Clock::from_time_t(sqlite3_column_int64(stmt, 7));
			data.reports.push_back(std::move(r));
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

    {
        std::lock_guard<std::recursive_mutex> lg(m_dataMutex);
        m_data = data;

		{
            //remove any unbound sensor
			int32_t index = findUnboundSensorIndex();
			if (index >= 0)
			{
				removeSensor((size_t)index);
			}
		}

        //refresh computed comms period
        if (!m_data.sensorsConfigs.empty())
        {
            SensorsConfig& config = m_data.sensorsConfigs.back();
            config.computedCommsPeriod = computeActualCommsPeriod(config.descriptor);
        }

        s_logger.logVerbose(QString("Done loading DB. Time: %3s").arg(std::chrono::duration<float>(Clock::now() - start).count()));
    }

	m_sqlite = &db;

    return success;
}

//void DB::test()
//{
//    for (PackedMeasurement const& pm: m_mainData.measurements.begin()->second)
//    {
//        Measurement measurement = unpack(m_mainData.measurements.begin()->first, pm);
//        computeTriggeredAlarm(measurement.descriptor);
//    }
//}

//////////////////////////////////////////////////////////////////////////

void DB::process()
{
    if (m_saveScheduled)
    {
        save();
    }
}

///////////////////////////////////////////////////////////////////////////////////////////

DB::Clock::duration DB::computeActualCommsPeriod(SensorsConfigDescriptor const& config) const
{
    std::lock_guard<std::recursive_mutex> lg(m_dataMutex);

    Clock::duration max_period = m_data.sensors.size() * COMMS_DURATION;
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
    std::lock_guard<std::recursive_mutex> lg(m_dataMutex);

    return m_data.baseStations.size();
}

//////////////////////////////////////////////////////////////////////////

DB::BaseStation const& DB::getBaseStation(size_t index) const
{
    std::lock_guard<std::recursive_mutex> lg(m_dataMutex);

    assert(index < m_data.baseStations.size());
    return m_data.baseStations[index];
}

//////////////////////////////////////////////////////////////////////////

bool DB::addBaseStation(BaseStationDescriptor const& descriptor)
{
    std::lock_guard<std::recursive_mutex> lg(m_dataMutex);

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

    std::unique_ptr<DB> db(new DB());
    if (db->load(*m_sqlite) != success)
    {
        s_logger.logCritical(QString("Cannot load db for Base Station '%1' / %2").arg(descriptor.name.c_str()).arg(macStr).toUtf8().data());
        return false;
    }

    s_logger.logInfo(QString("Adding base station '%1' / %2").arg(descriptor.name.c_str()).arg(getMacStr(descriptor.mac).c_str()));

    BaseStation baseStation;
    baseStation.descriptor = descriptor;
    baseStation.id = ++m_data.lastBaseStationId;

    m_data.baseStations.push_back(baseStation);
    emit baseStationAdded(baseStation.id);

    save();

    return true;
}

//////////////////////////////////////////////////////////////////////////

bool DB::setBaseStation(BaseStationId id, BaseStationDescriptor const& descriptor)
{
    std::lock_guard<std::recursive_mutex> lg(m_dataMutex);

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

    m_data.baseStations[index].descriptor = descriptor;
    emit baseStationChanged(id);

    save();

    return true;
}

//////////////////////////////////////////////////////////////////////////

void DB::removeBaseStation(size_t index)
{
    std::lock_guard<std::recursive_mutex> lg(m_dataMutex);

    assert(index < m_data.baseStations.size());
    BaseStationId id = m_data.baseStations[index].id;

    BaseStationDescriptor const& descriptor = m_data.baseStations[index].descriptor;
    s_logger.logInfo(QString("Removing base station '%1' / %2").arg(descriptor.name.c_str()).arg(getMacStr(descriptor.mac).c_str()));

    m_data.baseStations.erase(m_data.baseStations.begin() + index);
    emit baseStationRemoved(id);

    save();
}

//////////////////////////////////////////////////////////////////////////

int32_t DB::findBaseStationIndexByName(std::string const& name) const
{
    std::lock_guard<std::recursive_mutex> lg(m_dataMutex);

    auto it = std::find_if(m_data.baseStations.begin(), m_data.baseStations.end(), [&name](BaseStation const& baseStation) { return baseStation.descriptor.name == name; });
    if (it == m_data.baseStations.end())
    {
        return -1;
    }
    return std::distance(m_data.baseStations.begin(), it);
}

//////////////////////////////////////////////////////////////////////////

int32_t DB::findBaseStationIndexById(BaseStationId id) const
{
    std::lock_guard<std::recursive_mutex> lg(m_dataMutex);

    auto it = std::find_if(m_data.baseStations.begin(), m_data.baseStations.end(), [id](BaseStation const& baseStation) { return baseStation.id == id; });
    if (it == m_data.baseStations.end())
    {
        return -1;
    }
    return std::distance(m_data.baseStations.begin(), it);
}

//////////////////////////////////////////////////////////////////////////

int32_t DB::findBaseStationIndexByMac(BaseStationDescriptor::Mac const& mac) const
{
    std::lock_guard<std::recursive_mutex> lg(m_dataMutex);

    auto it = std::find_if(m_data.baseStations.begin(), m_data.baseStations.end(), [&mac](BaseStation const& baseStation) { return baseStation.descriptor.mac == mac; });
    if (it == m_data.baseStations.end())
    {
        return -1;
    }
    return std::distance(m_data.baseStations.begin(), it);
}

//////////////////////////////////////////////////////////////////////////

size_t DB::getSensorCount() const
{
    std::lock_guard<std::recursive_mutex> lg(m_dataMutex);
    return m_data.sensors.size();
}

//////////////////////////////////////////////////////////////////////////

DB::Sensor const& DB::getSensor(size_t index) const
{
    std::lock_guard<std::recursive_mutex> lg(m_dataMutex);
    assert(index < m_data.sensors.size());
    Sensor const& sensor = m_data.sensors[index];
    return sensor;
}

///////////////////////////////////////////////////////////////////////////////////////////

DB::SensorOutputDetails DB::computeSensorOutputDetails(SensorId id) const
{
    std::lock_guard<std::recursive_mutex> lg(m_dataMutex);

    int32_t _index = findSensorIndexById(id);
    if (_index < 0)
    {
        return {};
    }

    size_t index = static_cast<size_t>(_index);
    Sensor const& sensor = m_data.sensors[index];
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
    std::lock_guard<std::recursive_mutex> lg(m_dataMutex);

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

    if (!m_data.sensorsConfigs.empty())
    {
        uint32_t nextRealTimeMeasurementIndex = computeNextRealTimeMeasurementIndex();

        SensorsConfig const& c = findSensorsConfigForMeasurementIndex(nextRealTimeMeasurementIndex);
        uint32_t index = nextRealTimeMeasurementIndex >= c.baselineMeasurementIndex ? nextRealTimeMeasurementIndex - c.baselineMeasurementIndex : 0;
        Clock::time_point nextMeasurementTP = c.baselineMeasurementTimePoint + c.descriptor.measurementPeriod * index;

        m_data.sensorsConfigs.erase(std::remove_if(m_data.sensorsConfigs.begin(), m_data.sensorsConfigs.end(), [&nextRealTimeMeasurementIndex](SensorsConfig const& c)
        {
            return c.baselineMeasurementIndex == nextRealTimeMeasurementIndex;
        }), m_data.sensorsConfigs.end());
        config.baselineMeasurementIndex = nextRealTimeMeasurementIndex;
        config.baselineMeasurementTimePoint = nextMeasurementTP;
    }
    else
    {
        config.baselineMeasurementIndex = 0;
        config.baselineMeasurementTimePoint = Clock::now();
    }

    m_data.sensorsConfigs.push_back(config);
    while (m_data.sensorsConfigs.size() > 100)
    {
        m_data.sensorsConfigs.erase(m_data.sensorsConfigs.begin());
    }
    m_data.sensorsConfigs.back().computedCommsPeriod = computeActualCommsPeriod(m_data.sensorsConfigs.back().descriptor); //make sure the computed commd config is up-to-date

    emit sensorsConfigAdded();

    s_logger.logInfo("Added sensors config");

    save();

    return success;
}

//////////////////////////////////////////////////////////////////////////

Result<void> DB::setSensorsConfigs(std::vector<SensorsConfig> const& configs)
{
    std::lock_guard<std::recursive_mutex> lg(m_dataMutex);

    m_data.sensorsConfigs = configs;
    for (SensorsConfig& config: m_data.sensorsConfigs)
    {
        config.computedCommsPeriod = config.descriptor.commsPeriod;
    }
    if (!m_data.sensorsConfigs.empty())
    {
        SensorsConfig& config = m_data.sensorsConfigs.back();
        config.computedCommsPeriod = computeActualCommsPeriod(config.descriptor);
    }

    emit sensorsConfigChanged();

    s_logger.logInfo("Changed sensors configs");

    save();

    return success;
}

//////////////////////////////////////////////////////////////////////////

DB::SensorsConfig const& DB::getLastSensorsConfig() const
{
    std::lock_guard<std::recursive_mutex> lg(m_dataMutex);

    static SensorsConfig s_empty;
    return m_data.sensorsConfigs.empty() ? s_empty : m_data.sensorsConfigs.back();
}

//////////////////////////////////////////////////////////////////////////

size_t DB::getSensorsConfigCount() const
{
    std::lock_guard<std::recursive_mutex> lg(m_dataMutex);
    return m_data.sensorsConfigs.size();
}

//////////////////////////////////////////////////////////////////////////

DB::SensorsConfig const& DB::getSensorsConfig(size_t index) const
{
    std::lock_guard<std::recursive_mutex> lg(m_dataMutex);
    assert(index < m_data.sensorsConfigs.size());
    return m_data.sensorsConfigs[index];
}

//////////////////////////////////////////////////////////////////////////

DB::SensorsConfig const& DB::findSensorsConfigForMeasurementIndex(uint32_t index) const
{
    std::lock_guard<std::recursive_mutex> lg(m_dataMutex);

    for (auto it = m_data.sensorsConfigs.rbegin(); it != m_data.sensorsConfigs.rend(); ++it)
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
    std::lock_guard<std::recursive_mutex> lg(m_dataMutex);

    if (findSensorIndexByName(descriptor.name) >= 0)
    {
        return Error("Name '" + descriptor.name + "' already in use");
    }
    auto it = std::find_if(m_data.sensors.begin(), m_data.sensors.end(), [](Sensor const& sensor) { return sensor.state == Sensor::State::Unbound; });
    if (it != m_data.sensors.end())
    {
        return Error("There is already an unbound sensor");
    }

    {
        Sensor sensor;
        sensor.descriptor.name = descriptor.name;
        sensor.id = ++m_data.lastSensorId;
        sensor.state = Sensor::State::Unbound;

        m_data.sensors.push_back(sensor);
        emit sensorAdded(sensor.id);

        s_logger.logInfo(QString("Added sensor '%1'").arg(descriptor.name.c_str()));
    }

    //refresh computed comms period as new sensors are added
    if (!m_data.sensorsConfigs.empty())
    {
        SensorsConfig& config = m_data.sensorsConfigs.back();
        config.computedCommsPeriod = computeActualCommsPeriod(config.descriptor);
        emit sensorsConfigChanged();
    }

    save();

    return success;
}

//////////////////////////////////////////////////////////////////////////

Result<void> DB::setSensor(SensorId id, SensorDescriptor const& descriptor)
{
    std::lock_guard<std::recursive_mutex> lg(m_dataMutex);

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
    m_data.sensors[index].descriptor = descriptor;
    emit sensorChanged(id);

    s_logger.logInfo(QString("Changed sensor '%1'").arg(descriptor.name.c_str()));

    save();

    return success;
}

//////////////////////////////////////////////////////////////////////////

Result<DB::SensorId> DB::bindSensor(uint32_t serialNumber, uint8_t sensorType, uint8_t hardwareVersion, uint8_t softwareVersion, Sensor::Calibration const& calibration)
{
    std::lock_guard<std::recursive_mutex> lg(m_dataMutex);

    int32_t _index = findUnboundSensorIndex();
    if (_index < 0)
    {
        return Error("No unbound sensor");
    }

    size_t index = static_cast<size_t>(_index);
    Sensor& sensor = m_data.sensors[index];
    if (sensor.state != Sensor::State::Unbound)
    {
        return Error("No unbound sensor (inconsistent internal state)");
    }

    sensor.address = ++m_data.lastSensorAddress;
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
    if (!m_data.sensorsConfigs.empty())
    {
        SensorsConfig& config = m_data.sensorsConfigs.back();
        config.computedCommsPeriod = computeActualCommsPeriod(config.descriptor);
        emit sensorsConfigChanged();
    }

    save();

    return sensor.id;
}

//////////////////////////////////////////////////////////////////////////

Result<void> DB::setSensorCalibration(SensorId id, Sensor::Calibration const& calibration)
{
    std::lock_guard<std::recursive_mutex> lg(m_dataMutex);

    int32_t _index = findSensorIndexById(id);
    if (_index < 0)
    {
        return Error("Invalid sensor id");
    }
    size_t index = static_cast<size_t>(_index);
    Sensor& sensor = m_data.sensors[index];
    if (sensor.state == Sensor::State::Unbound)
    {
        return Error("Cannot change calibration for unbound sensor");
    }

    sensor.calibration = calibration;

    emit sensorChanged(sensor.id);

    s_logger.logInfo(QString("Sensor '%1' calibration changed").arg(sensor.descriptor.name.c_str()));

    save();

    return success;
}

//////////////////////////////////////////////////////////////////////////

Result<void> DB::setSensorSleep(SensorId id, bool sleep)
{
    std::lock_guard<std::recursive_mutex> lg(m_dataMutex);

    int32_t _index = findSensorIndexById(id);
    if (_index < 0)
    {
        return Error("Invalid sensor id");
    }
    size_t index = static_cast<size_t>(_index);
    Sensor& sensor = m_data.sensors[index];
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

    save();

    return success;
}

//////////////////////////////////////////////////////////////////////////

Result<void> DB::clearErrorCounters(SensorId id)
{
	std::lock_guard<std::recursive_mutex> lg(m_dataMutex);

	int32_t _index = findSensorIndexById(id);
	if (_index < 0)
	{
		return Error("Invalid sensor id");
	}
	size_t index = static_cast<size_t>(_index);
	Sensor& sensor = m_data.sensors[index];
    sensor.errorCounters = ErrorCounters();

	emit sensorChanged(sensor.id);

	s_logger.logInfo(QString("Sensor '%1' error counters cleared").arg(sensor.descriptor.name.c_str()));

    save();

	return success;
}

//////////////////////////////////////////////////////////////////////////

bool DB::setSensorInputDetails(SensorInputDetails const& details)
{
    std::lock_guard<std::recursive_mutex> lg(m_dataMutex);

    return setSensorsInputDetails({details});
}

//////////////////////////////////////////////////////////////////////////

bool DB::setSensorsInputDetails(std::vector<SensorInputDetails> const& details)
{
    std::lock_guard<std::recursive_mutex> lg(m_dataMutex);

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
        Sensor& sensor = m_data.sensors[index];
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

            //we do this here just in case the sensor doesn't have any measurements. In that case the average signal strength will be calculated from the lastSignalStrengthB2S
            //this happens the first time a sensor is added
            if (sensor.averageSignalStrength.b2s == 0 || sensor.averageSignalStrength.s2b == 0)
			{
                sensor.averageSignalStrength = { d.signalStrengthB2S, d.signalStrengthB2S };
			}
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

    scheduleSave();

    return ok;
}

//////////////////////////////////////////////////////////////////////////

void DB::removeSensor(size_t index)
{
    std::lock_guard<std::recursive_mutex> lg(m_dataMutex);

    assert(index < m_data.sensors.size());

    SensorId sensorId = m_data.sensors[index].id;

    {
		QString sql = QString("DELETE FROM Measurements "
                              "WHERE sensorId = %1;").arg(sensorId);
		if (sqlite3_exec(m_sqlite, sql.toUtf8().data(), NULL, NULL, nullptr))
		{
			s_logger.logCritical(QString("Failed to remove measurements for sensor %1: %2").arg(sensorId).arg(sqlite3_errmsg(m_sqlite)));
			return;
		}
        emit measurementsRemoved(sensorId);
    }

    for (Alarm& alarm: m_data.alarms)
    {
        alarm.descriptor.sensors.erase(sensorId);
        alarm.triggersPerSensor.erase(sensorId);
    }

    for (Report& report: m_data.reports)
    {
        report.descriptor.sensors.erase(sensorId);
    }

    s_logger.logInfo(QString("Removing sensor '%1'").arg(m_data.sensors[index].descriptor.name.c_str()));

    m_data.sensors.erase(m_data.sensors.begin() + index);
    emit sensorRemoved(sensorId);

    //refresh computed comms period as new sensors are removed
    if (!m_data.sensorsConfigs.empty())
    {
        SensorsConfig& config = m_data.sensorsConfigs.back();
        config.computedCommsPeriod = computeActualCommsPeriod(config.descriptor);
        emit sensorsConfigChanged();
    }

    save();
}

//////////////////////////////////////////////////////////////////////////

int32_t DB::findSensorIndexByName(std::string const& name) const
{
    std::lock_guard<std::recursive_mutex> lg(m_dataMutex);

    auto it = std::find_if(m_data.sensors.begin(), m_data.sensors.end(), [&name](Sensor const& sensor) { return sensor.descriptor.name == name; });
    if (it == m_data.sensors.end())
    {
        return -1;
    }
    return std::distance(m_data.sensors.begin(), it);
}

//////////////////////////////////////////////////////////////////////////

int32_t DB::findSensorIndexById(SensorId id) const
{
    std::lock_guard<std::recursive_mutex> lg(m_dataMutex);

    auto it = std::find_if(m_data.sensors.begin(), m_data.sensors.end(), [&id](Sensor const& sensor) { return sensor.id == id; });
    if (it == m_data.sensors.end())
    {
        return -1;
    }
    return std::distance(m_data.sensors.begin(), it);
}

//////////////////////////////////////////////////////////////////////////

int32_t DB::findSensorIndexByAddress(SensorAddress address) const
{
    std::lock_guard<std::recursive_mutex> lg(m_dataMutex);

    auto it = std::find_if(m_data.sensors.begin(), m_data.sensors.end(), [&address](Sensor const& sensor) { return sensor.address == address; });
    if (it == m_data.sensors.end())
    {
        return -1;
    }
    return std::distance(m_data.sensors.begin(), it);
}

//////////////////////////////////////////////////////////////////////////

int32_t DB::findSensorIndexBySerialNumber(SensorSerialNumber serialNumber) const
{
    std::lock_guard<std::recursive_mutex> lg(m_dataMutex);

    auto it = std::find_if(m_data.sensors.begin(), m_data.sensors.end(), [&serialNumber](Sensor const& sensor) { return sensor.serialNumber == serialNumber; });
    if (it == m_data.sensors.end())
    {
        return -1;
    }
    return std::distance(m_data.sensors.begin(), it);
}

//////////////////////////////////////////////////////////////////////////

int32_t DB::findUnboundSensorIndex() const
{
    std::lock_guard<std::recursive_mutex> lg(m_dataMutex);

    auto it = std::find_if(m_data.sensors.begin(), m_data.sensors.end(), [](Sensor const& sensor) { return sensor.state == Sensor::State::Unbound; });
    if (it == m_data.sensors.end())
    {
        return -1;
    }
    return std::distance(m_data.sensors.begin(), it);
}

//////////////////////////////////////////////////////////////////////////

size_t DB::getAlarmCount() const
{
    std::lock_guard<std::recursive_mutex> lg(m_dataMutex);

    return m_data.alarms.size();
}

//////////////////////////////////////////////////////////////////////////

DB::Alarm const& DB::getAlarm(size_t index) const
{
    std::lock_guard<std::recursive_mutex> lg(m_dataMutex);

    assert(index < m_data.alarms.size());
    return m_data.alarms[index];
}

//////////////////////////////////////////////////////////////////////////

Result<void> DB::addAlarm(AlarmDescriptor const& descriptor)
{
    std::lock_guard<std::recursive_mutex> lg(m_dataMutex);

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

    alarm.id = ++m_data.lastAlarmId;

    m_data.alarms.push_back(alarm);
    emit alarmAdded(alarm.id);

    s_logger.logInfo(QString("Added alarm '%1'").arg(descriptor.name.c_str()));

    save();

    return success;
}

//////////////////////////////////////////////////////////////////////////

Result<void> DB::setAlarm(AlarmId id, AlarmDescriptor const& descriptor)
{
    std::lock_guard<std::recursive_mutex> lg(m_dataMutex);

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
    m_data.alarms[index].descriptor = descriptor;
    emit alarmChanged(id);

    s_logger.logInfo(QString("Changed alarm '%1'").arg(descriptor.name.c_str()));

    save();

    return success;
}

//////////////////////////////////////////////////////////////////////////

void DB::removeAlarm(size_t index)
{
    std::lock_guard<std::recursive_mutex> lg(m_dataMutex);

    assert(index < m_data.alarms.size());
    AlarmId id = m_data.alarms[index].id;

    s_logger.logInfo(QString("Removed alarm '%1'").arg(m_data.alarms[index].descriptor.name.c_str()));

    m_data.alarms.erase(m_data.alarms.begin() + index);
    emit alarmRemoved(id);

    save();
}

//////////////////////////////////////////////////////////////////////////

int32_t DB::findAlarmIndexByName(std::string const& name) const
{
    std::lock_guard<std::recursive_mutex> lg(m_dataMutex);

    auto it = std::find_if(m_data.alarms.begin(), m_data.alarms.end(), [&name](Alarm const& alarm) { return alarm.descriptor.name == name; });
    if (it == m_data.alarms.end())
    {
        return -1;
    }
    return std::distance(m_data.alarms.begin(), it);
}

//////////////////////////////////////////////////////////////////////////

int32_t DB::findAlarmIndexById(AlarmId id) const
{
    std::lock_guard<std::recursive_mutex> lg(m_dataMutex);

    auto it = std::find_if(m_data.alarms.begin(), m_data.alarms.end(), [id](Alarm const& alarm) { return alarm.id == id; });
    if (it == m_data.alarms.end())
    {
        return -1;
    }
    return std::distance(m_data.alarms.begin(), it);
}

//////////////////////////////////////////////////////////////////////////

uint8_t DB::computeAlarmTriggers(Measurement const& m)
{
    std::lock_guard<std::recursive_mutex> lg(m_dataMutex);

    uint8_t allTriggers = 0;

    for (Alarm& alarm: m_data.alarms)
    {
        uint8_t triggers = _computeAlarmTriggers(alarm, m);
        allTriggers |= triggers;
    }

    return allTriggers;
}

//////////////////////////////////////////////////////////////////////////

uint8_t DB::_computeAlarmTriggers(Alarm& alarm, Measurement const& m)
{
    std::lock_guard<std::recursive_mutex> lg(m_dataMutex);

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

    float alertLevelVcc = utils::k_alertBatteryLevel * (utils::k_maxBatteryLevel - utils::k_minBatteryLevel) + utils::k_minBatteryLevel;
    if (ad.lowVccWatch && m.descriptor.vcc <= alertLevelVcc)
    {
        triggers |= AlarmTrigger::LowVcc;
    }
//        if (ad.sensorErrorsWatch && m.descriptor.sensorErrors != 0)
//        {
//            triggers |= AlarmTrigger::SensorErrors;
//        }

    float alertLevelSignal = utils::k_alertSignalLevel * (utils::k_maxSignalLevel - utils::k_minSignalLevel) + utils::k_minSignalLevel;
    if (ad.lowSignalWatch && std::min(m.descriptor.signalStrength.b2s, m.descriptor.signalStrength.s2b) <= alertLevelSignal)
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
    std::lock_guard<std::recursive_mutex> lg(m_dataMutex);

    return m_data.reports.size();
}

//////////////////////////////////////////////////////////////////////////

DB::Report const& DB::getReport(size_t index) const
{
    std::lock_guard<std::recursive_mutex> lg(m_dataMutex);

    assert(index < m_data.reports.size());
    return m_data.reports[index];
}

//////////////////////////////////////////////////////////////////////////

Result<void> DB::addReport(ReportDescriptor const& descriptor)
{
    std::lock_guard<std::recursive_mutex> lg(m_dataMutex);

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

    report.id = ++m_data.lastReportId;

    m_data.reports.push_back(report);
    emit reportAdded(report.id);

    s_logger.logInfo(QString("Added report '%1'").arg(descriptor.name.c_str()));

    save();

    return success;
}

//////////////////////////////////////////////////////////////////////////

Result<void> DB::setReport(ReportId id, ReportDescriptor const& descriptor)
{
    std::lock_guard<std::recursive_mutex> lg(m_dataMutex);

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
    m_data.reports[index].descriptor = descriptor;
    emit reportChanged(id);

    s_logger.logInfo(QString("Changed report '%1'").arg(descriptor.name.c_str()));

    save();

    return success;
}

//////////////////////////////////////////////////////////////////////////

void DB::removeReport(size_t index)
{
    std::lock_guard<std::recursive_mutex> lg(m_dataMutex);

    assert(index < m_data.reports.size());
    ReportId id = m_data.reports[index].id;

    s_logger.logInfo(QString("Removed report '%1'").arg(m_data.reports[index].descriptor.name.c_str()));

    m_data.reports.erase(m_data.reports.begin() + index);
    emit reportRemoved(id);

    save();
}

//////////////////////////////////////////////////////////////////////////

int32_t DB::findReportIndexByName(std::string const& name) const
{
    std::lock_guard<std::recursive_mutex> lg(m_dataMutex);

    auto it = std::find_if(m_data.reports.begin(), m_data.reports.end(), [&name](Report const& report) { return report.descriptor.name == name; });
    if (it == m_data.reports.end())
    {
        return -1;
    }
    return std::distance(m_data.reports.begin(), it);
}

//////////////////////////////////////////////////////////////////////////

int32_t DB::findReportIndexById(ReportId id) const
{
    std::lock_guard<std::recursive_mutex> lg(m_dataMutex);

    auto it = std::find_if(m_data.reports.begin(), m_data.reports.end(), [id](Report const& report) { return report.id == id; });
    if (it == m_data.reports.end())
    {
        return -1;
    }
    return std::distance(m_data.reports.begin(), it);
}

//////////////////////////////////////////////////////////////////////////

bool DB::isReportTriggered(ReportId id) const
{
    std::lock_guard<std::recursive_mutex> lg(m_dataMutex);

    int32_t _index = findReportIndexById(id);
    if (_index < 0)
    {
        return false;
    }

    size_t index = static_cast<size_t>(_index);
    Report const& report = m_data.reports[index];

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
    std::lock_guard<std::recursive_mutex> lg(m_dataMutex);

    int32_t _index = findReportIndexById(id);
    if (_index < 0)
    {
        return;
    }

    size_t index = static_cast<size_t>(_index);
    Report& report = m_data.reports[index];
    report.lastTriggeredTimePoint = Clock::now();

    save();
}

//////////////////////////////////////////////////////////////////////////

bool DB::addMeasurement(MeasurementDescriptor const& md)
{
    std::lock_guard<std::recursive_mutex> lg(m_dataMutex);

    return _addMeasurements(md.sensorId, { md });
}

//////////////////////////////////////////////////////////////////////////

bool DB::addMeasurements(std::vector<MeasurementDescriptor> mds)
{
    std::lock_guard<std::recursive_mutex> lg(m_dataMutex);

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
    std::vector<Measurement> asyncMeasurements;

	{
		std::lock_guard<std::recursive_mutex> lg(m_dataMutex);

		int32_t _sensorIndex = findSensorIndexById(sensorId);
		if (_sensorIndex < 0)
		{
			return false;
		}
		size_t sensorIndex = static_cast<size_t>(_sensorIndex);
		Sensor& sensor = m_data.sensors[sensorIndex];

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

		{
			for (MeasurementDescriptor const& md : mds)
			{
				MeasurementId id = computeMeasurementId(md);

				//advance the last confirmed index
				if (md.index == sensor.lastConfirmedMeasurementIndex + 1)
				{
					sensor.lastConfirmedMeasurementIndex = md.index;
				}

				Measurement measurement;
				measurement.id = id;
				measurement.descriptor = md;
				measurement.timePoint = computeMeasurementTimepoint(md);
				measurement.alarmTriggers = computeAlarmTriggers(measurement);
                asyncMeasurements.emplace_back(measurement);

				//this is also set in the setSensorInputDetails
				if (!sensor.isMeasurementValid)
				{
					sensor.isMeasurementValid = true;
					sensor.measurementTemperature = md.temperature;
					sensor.measurementHumidity = md.humidity;
					sensor.measurementVcc = md.vcc;
				}
			}
		}
		if (asyncMeasurements.empty())
		{
			return true;
		}

		emit sensorDataChanged(sensor.id);
	}

	{
		std::lock_guard<std::recursive_mutex> lg(m_asyncMeasurementsMutex);
		m_asyncMeasurements = std::move(asyncMeasurements);
	}

    scheduleSave();

    return true;
}

//////////////////////////////////////////////////////////////////////////

void DB::addAsyncMeasurements()
{
    struct SensorData
    {
		uint32_t minIndex = std::numeric_limits<uint32_t>::max();
		uint32_t maxIndex = std::numeric_limits<uint32_t>::lowest();
    };
    std::map<SensorId, SensorData> sensorDatas;

	{
		std::lock_guard<std::recursive_mutex> lg(m_asyncMeasurementsMutex);
		if (m_asyncMeasurements.empty())
		{
			return;
		}

		{
			sqlite3_exec(m_sqlite, "BEGIN TRANSACTION;", NULL, NULL, NULL);
			utils::epilogue epi1([this] { sqlite3_exec(m_sqlite, "END TRANSACTION;", NULL, NULL, NULL); });

			sqlite3_stmt* stmt = m_addMeasurementsStmt.get();
			for (Measurement const& m : m_asyncMeasurements)
			{
				MeasurementDescriptor const& md = m.descriptor;

				sqlite3_bind_int64(stmt, 1, m.id);
				sqlite3_bind_int64(stmt, 2, Clock::to_time_t(m.timePoint));
				sqlite3_bind_int64(stmt, 3, md.index);
				sqlite3_bind_int64(stmt, 4, md.sensorId);
				sqlite3_bind_double(stmt, 5, md.temperature);
				sqlite3_bind_double(stmt, 6, md.humidity);
				sqlite3_bind_double(stmt, 7, md.vcc);
				sqlite3_bind_int64(stmt, 8, md.signalStrength.s2b);
				sqlite3_bind_int64(stmt, 9, md.signalStrength.b2s);
				sqlite3_bind_int64(stmt, 10, md.sensorErrors);
				sqlite3_bind_int64(stmt, 11, m.alarmTriggers);
				if (sqlite3_step(stmt) != SQLITE_DONE)
				{
					s_logger.logCritical(QString("Failed to save measurement: %1").arg(sqlite3_errmsg(m_sqlite)));
				}
				sqlite3_reset(stmt);

                SensorData& sd = sensorDatas[md.sensorId];
				sd.minIndex = std::min(sd.minIndex, md.index);
				sd.maxIndex = std::max(sd.maxIndex, md.index);
			}
		}
		m_asyncMeasurements.clear();
	}

	{
		std::lock_guard<std::recursive_mutex> lg(m_dataMutex);
		for (auto const& p : sensorDatas)
		{
			int32_t _sensorIndex = findSensorIndexById(p.first);
			if (_sensorIndex >= 0)
			{
				size_t sensorIndex = static_cast<size_t>(_sensorIndex);
				Sensor& sensor = m_data.sensors[sensorIndex];
				sensor.averageSignalStrength = computeAverageSignalStrength(sensor.id, m_data);
				emit sensorDataChanged(sensor.id);
				s_logger.logVerbose(QString("Added measurement indices %1 to %2, sensor '%3'").arg(p.second.minIndex).arg(p.second.maxIndex).arg(sensor.descriptor.name.c_str()));
			}
			emit measurementsAdded(p.first);
		}
	}
}

//////////////////////////////////////////////////////////////////////////

DB::Clock::time_point DB::computeMeasurementTimepoint(MeasurementDescriptor const& md) const
{
    std::lock_guard<std::recursive_mutex> lg(m_dataMutex);

    SensorsConfig const& config = findSensorsConfigForMeasurementIndex(md.index);
    uint32_t index = md.index >= config.baselineMeasurementIndex ? md.index - config.baselineMeasurementIndex : 0;
    Clock::time_point tp = config.baselineMeasurementTimePoint + config.descriptor.measurementPeriod * index;
    return tp;
}

//////////////////////////////////////////////////////////////////////////

size_t DB::getAllMeasurementCount() const
{
    std::lock_guard<std::recursive_mutex> lg(m_dataMutex);

	const char* sql = "SELECT COUNT(*) FROM Measurements;";
	sqlite3_stmt* stmt;
	if (sqlite3_prepare_v2(m_sqlite, sql, -1, &stmt, 0) != SQLITE_OK)
	{
		const char* msg = sqlite3_errmsg(m_sqlite);
		Q_ASSERT(false);
		return 0;
	}
	utils::epilogue epi([stmt] { sqlite3_finalize(stmt); });

    if (sqlite3_step(stmt) != SQLITE_ROW)
    {
        const char* msg = sqlite3_errmsg(m_sqlite);
        Q_ASSERT(false);
        return 0;
    }

    return sqlite3_column_int64(stmt, 0);
}

//////////////////////////////////////////////////////////////////////////

std::vector<DB::Measurement> DB::getFilteredMeasurements(Filter const& filter) const
{
    std::lock_guard<std::recursive_mutex> lg(m_dataMutex);

    std::vector<DB::Measurement> result;
    result.reserve(8192);
    _getFilteredMeasurements(filter, &result);
    return result;
}

//////////////////////////////////////////////////////////////////////////

size_t DB::getFilteredMeasurementCount(Filter const& filter) const
{
    std::lock_guard<std::recursive_mutex> lg(m_dataMutex);

    return _getFilteredMeasurements(filter, nullptr);
}

//////////////////////////////////////////////////////////////////////////

static std::string getQueryWherePart(DB::Filter const& filter)
{
    std::string sql;

    std::vector<std::string> conditions;

	if (filter.useTimePointFilter)
	{
        std::string str = " timePoint >= ";
        str += std::to_string(DB::Clock::to_time_t(filter.timePointFilter.min));
        str += " AND timePoint <= ";
        str += std::to_string(DB::Clock::to_time_t(filter.timePointFilter.max));
        conditions.push_back(str);
	}
	if (filter.useSensorFilter)
	{
        std::string str;
        for (DB::SensorId sensorId : filter.sensorIds)
		{
			if (!str.empty()) str += " OR";
            str += " sensorId = " + std::to_string(sensorId);
		}
		conditions.push_back(str);
	}
	if (filter.useTemperatureFilter)
	{
		std::string str = " temperature >= ";
		str += std::to_string(filter.temperatureFilter.min);
		str += " AND temperature <= ";
		str += std::to_string(filter.temperatureFilter.max);
		conditions.push_back(str);
	}
	if (filter.useHumidityFilter)
	{
		std::string str = " humidity >= ";
		str += std::to_string(filter.humidityFilter.min);
		str += " AND humidity <= ";
		str += std::to_string(filter.humidityFilter.max);
		conditions.push_back(str);
	}
	if (filter.useVccFilter)
	{
		std::string str = " vcc >= ";
		str += std::to_string(filter.vccFilter.min);
		str += " AND vcc <= ";
		str += std::to_string(filter.vccFilter.max);
		conditions.push_back(str);
	}
	if (filter.useSignalStrengthFilter)
	{
		std::string str = " MIN(signalStrengthS2B, signalStrengthB2S) >= ";
		str += std::to_string(filter.signalStrengthFilter.min);
		str += " AND MIN(signalStrengthS2B, signalStrengthB2S) <= ";
		str += std::to_string(filter.signalStrengthFilter.max);
		conditions.push_back(str);
	}

	if (!conditions.empty())
	{
		sql += " WHERE";

        bool first = true;
        for (std::string const& str: conditions)
		{
            if (!first)
            {
				sql += " AND ";
            }
            first = false;
			sql += " (";
			sql += str;
			sql += ")";
		}
	}
    return sql;
}

size_t DB::_getFilteredMeasurements(Filter const& filter, std::vector<DB::Measurement>* out) const
{
    Clock::time_point start = Clock::now();

    size_t count = 0;

	{
		std::string sql = "SELECT id, timePoint, idx, sensorId, temperature, humidity, vcc, signalStrengthS2B, signalStrengthB2S, sensorErrors, alarmTriggers "
                          "FROM Measurements " + getQueryWherePart(filter);
		sql += ";";

		sqlite3_stmt* stmt;
		if (sqlite3_prepare_v2(m_sqlite, sql.c_str(), -1, &stmt, 0) != SQLITE_OK)
		{
            const char* msg = sqlite3_errmsg(m_sqlite);
			Q_ASSERT(false);
			return {};
		}
		utils::epilogue epi([stmt] { sqlite3_finalize(stmt); });

		while (sqlite3_step(stmt) == SQLITE_ROW)
		{
			if (out)
			{
                Measurement m = unpack(stmt);
				out->push_back(std::move(m));
			}
			count++;
		}
	}
    return count;
}

//////////////////////////////////////////////////////////////////////////

Result<DB::Measurement> DB::getLastMeasurementForSensor(SensorId sensorId) const
{
    std::lock_guard<std::recursive_mutex> lg(m_dataMutex);

    QString sql = QString("SELECT id, timePoint, idx, sensorId, temperature, humidity, vcc, signalStrengthS2B, signalStrengthB2S, sensorErrors, alarmTriggers "
                          "FROM Measurements "
                          "WHERE sensorId = %1 "
                          "ORDER BY idx DESC LIMIT 1;").arg(sensorId);
	sqlite3_stmt* stmt;
	if (sqlite3_prepare_v2(m_sqlite, sql.toUtf8().data(), -1, &stmt, 0) != SQLITE_OK)
	{
		const char* msg = sqlite3_errmsg(m_sqlite);
		Q_ASSERT(false);
		return Error(QString("Failed to get last measurement from the db: %1").arg(sqlite3_errmsg(m_sqlite)).toUtf8().data());
	}
	utils::epilogue epi([stmt] { sqlite3_finalize(stmt); });

	while (sqlite3_step(stmt) == SQLITE_ROW)
	{
		return unpack(stmt);
	}

    return Error("No data");
}

//////////////////////////////////////////////////////////////////////////

Result<DB::Measurement> DB::findMeasurementById(MeasurementId id) const
{
	QString sql = QString("SELECT id, timePoint, idx, sensorId, temperature, humidity, vcc, signalStrengthS2B, signalStrengthB2S, sensorErrors, alarmTriggers "
                          "FROM Measurements "
                          "WHERE id = %1;").arg(id);
	sqlite3_stmt* stmt;
	if (sqlite3_prepare_v2(m_sqlite, sql.toUtf8().data(), -1, &stmt, 0) != SQLITE_OK)
	{
		const char* msg = sqlite3_errmsg(m_sqlite);
		Q_ASSERT(false);
		return Error(QString("Failed to get measurement from the db: %1").arg(sqlite3_errmsg(m_sqlite)).toUtf8().data());
	}
	utils::epilogue epi([stmt] { sqlite3_finalize(stmt); });

	while (sqlite3_step(stmt) == SQLITE_ROW)
	{
		return unpack(stmt);
	}

	return Error("Cannot find measurement");
}

//////////////////////////////////////////////////////////////////////////

Result<void> DB::setMeasurement(MeasurementId id, MeasurementDescriptor const& measurement)
{
	QString sql = QString("UPDATE Measurements "
                          "SET temperature = ?1, humidity = ?2, vcc = ?3 "
                          "WHERE id = ?4;").arg(id);
	sqlite3_stmt* stmt;
	if (sqlite3_prepare_v2(m_sqlite, sql.toUtf8().data(), -1, &stmt, 0) != SQLITE_OK)
	{
		const char* msg = sqlite3_errmsg(m_sqlite);
		Q_ASSERT(false);
		return Error(QString("Failed to get measurement from the db: %1").arg(sqlite3_errmsg(m_sqlite)).toUtf8().data());
	}
	utils::epilogue epi([stmt] { sqlite3_finalize(stmt); });

	sqlite3_bind_double(stmt, 1, measurement.temperature);
	sqlite3_bind_double(stmt, 2, measurement.humidity);
	sqlite3_bind_double(stmt, 3, measurement.vcc);
	sqlite3_bind_int64(stmt, 4, id);
	if (sqlite3_step(stmt) != SQLITE_DONE)
	{
		return Error(QString("Failed to set measurement: %1").arg(sqlite3_errmsg(m_sqlite)).toUtf8().data());
	}

    emit measurementsChanged(getSensorIdFromMeasurementId(id));
    return success;
}

//////////////////////////////////////////////////////////////////////////

inline DB::SensorId DB::getSensorIdFromMeasurementId(MeasurementId id)
{
	return SensorId(id >> 32);
}

//////////////////////////////////////////////////////////////////////////

inline DB::MeasurementId DB::computeMeasurementId(MeasurementDescriptor const& md)
{
    return (MeasurementId(md.sensorId) << 32) | MeasurementId(md.index);
}

//////////////////////////////////////////////////////////////////////////

DB::Measurement DB::unpack(sqlite3_stmt* stmt)
{
	Measurement m;
	m.id = sqlite3_column_int64(stmt, 0);
	m.timePoint = Clock::from_time_t(sqlite3_column_int64(stmt, 1));
	m.descriptor.index = sqlite3_column_int64(stmt, 2);
	m.descriptor.sensorId = (uint32_t)sqlite3_column_int64(stmt, 3);
	m.descriptor.temperature = (float)sqlite3_column_double(stmt, 4);
	m.descriptor.humidity = (float)sqlite3_column_double(stmt, 5);
	m.descriptor.vcc = (float)sqlite3_column_double(stmt, 6);
	m.descriptor.signalStrength.s2b = (int8_t)sqlite3_column_int(stmt, 7);
	m.descriptor.signalStrength.b2s = (int8_t)sqlite3_column_int(stmt, 8);
	m.descriptor.sensorErrors = (uint8_t)sqlite3_column_int(stmt, 9);
	m.alarmTriggers = (uint8_t)sqlite3_column_int(stmt, 10);
    return m;
}

//////////////////////////////////////////////////////////////////////////

DB::SignalStrength DB::computeAverageSignalStrength(SensorId sensorId, Data const& data) const
{
	QString sql = QString("SELECT id, timePoint, idx, sensorId, temperature, humidity, vcc, signalStrengthS2B, signalStrengthB2S, sensorErrors, alarmTriggers "
                          "FROM Measurements "
                          "WHERE sensorId = %1 AND signalStrengthS2B != 0 AND signalStrengthB2S != 0 "
                          "ORDER BY idx DESC LIMIT 10;").arg(sensorId);
	sqlite3_stmt* stmt;
	if (sqlite3_prepare_v2(m_sqlite, sql.toUtf8().data(), -1, &stmt, 0) != SQLITE_OK)
	{
		const char* msg = sqlite3_errmsg(m_sqlite);
		Q_ASSERT(false);
        return {};
	}
	utils::epilogue epi([stmt] { sqlite3_finalize(stmt); });

    int64_t avgs2b = 0;
    int64_t avgb2s = 0;
    int64_t count = 0;

	while (sqlite3_step(stmt) == SQLITE_ROW)
	{
		Measurement m = unpack(stmt);
        avgb2s += m.descriptor.signalStrength.b2s;
        avgs2b += m.descriptor.signalStrength.s2b;
        count++;
	}

    if (count > 0)
    {
        avgb2s /= count;
        avgs2b /= count;
    }
    else
    {
		int32_t sensorIndex = findSensorIndexById(sensorId);
		if (sensorIndex >= 0)
		{
			Sensor const& sensor = getSensor((size_t)sensorIndex);
			return { sensor.lastSignalStrengthB2S, sensor.lastSignalStrengthB2S };
		}
    }
	SignalStrength avg;
    avg.b2s = static_cast<int8_t>(avgb2s);
    avg.s2b = static_cast<int8_t>(avgs2b);
	return avg;
}

//////////////////////////////////////////////////////////////////////////

void DB::scheduleSave()
{
    m_saveScheduled = true;
}

//////////////////////////////////////////////////////////////////////////

void DB::save()
{
    addAsyncMeasurements();
    save(m_data);
    m_saveScheduled = false;
}

//////////////////////////////////////////////////////////////////////////

void DB::save(Data const& data) const
{
    Clock::time_point start = Clock::now();

	sqlite3_exec(m_sqlite, "BEGIN TRANSACTION;", NULL, NULL, NULL);
	utils::epilogue epi([this] { sqlite3_exec(m_sqlite, "END TRANSACTION;", NULL, NULL, NULL); });

 	{
	    {
		    const char* sql = "DELETE FROM BaseStations;";
		    if (sqlite3_exec(m_sqlite, sql, NULL, NULL, nullptr))
		    {
			    s_logger.logCritical(QString("Failed to clear base stations: %1").arg(sqlite3_errmsg(m_sqlite)));
			    return;
		    }
	    }
	    sqlite3_stmt* stmt = m_saveBaseStationsStmt.get();
	    for (BaseStation const& bs : data.baseStations)
	    {
		    sqlite3_bind_int64(stmt, 1, bs.id);
		    sqlite3_bind_text(stmt, 2, bs.descriptor.name.c_str(), -1, SQLITE_STATIC);
		    sqlite3_bind_text(stmt, 3, getMacStr(bs.descriptor.mac).c_str(), -1, SQLITE_TRANSIENT);
		    if (sqlite3_step(stmt) != SQLITE_DONE)
		    {
			    s_logger.logCritical(QString("Failed to save base station: %1").arg(sqlite3_errmsg(m_sqlite)));
			    return;
		    }
		    sqlite3_reset(stmt);
	    }
 	}
 	{
		{
			const char* sql = "DELETE FROM SensorsConfigs;";
			if (sqlite3_exec(m_sqlite, sql, NULL, NULL, nullptr))
			{
				s_logger.logCritical(QString("Failed to clear sensor configs: %1").arg(sqlite3_errmsg(m_sqlite)));
				return;
			}
		}
		sqlite3_stmt* stmt = m_saveSensorsConfigsStmt.get();
		for (SensorsConfig const& sc: data.sensorsConfigs)
		{
			sqlite3_bind_int64(stmt, 1, Clock::to_time_t(sc.baselineMeasurementTimePoint));
			sqlite3_bind_int64(stmt, 2, sc.baselineMeasurementIndex);
            sqlite3_bind_int64(stmt, 3, sc.descriptor.sensorsPower);
            sqlite3_bind_int64(stmt, 4, std::chrono::duration_cast<std::chrono::seconds>(sc.descriptor.measurementPeriod).count());
            sqlite3_bind_int64(stmt, 5, std::chrono::duration_cast<std::chrono::seconds>(sc.descriptor.commsPeriod).count());
			if (sqlite3_step(stmt) != SQLITE_DONE)
			{
				s_logger.logCritical(QString("Failed to save sensor config: %1").arg(sqlite3_errmsg(m_sqlite)));
				return;
			}
			sqlite3_reset(stmt);
		}
	}
 	{
		{
			const char* sql = "DELETE FROM Sensors;";
			if (sqlite3_exec(m_sqlite, sql, NULL, NULL, nullptr))
			{
				s_logger.logCritical(QString("Failed to clear sensor configs: %1").arg(sqlite3_errmsg(m_sqlite)));
				return;
			}
		}
		sqlite3_stmt* stmt = m_saveSensorsStmt.get();
		for (Sensor const& s : data.sensors)
		{
			sqlite3_bind_int64(stmt, 1, s.id);
            sqlite3_bind_text(stmt, 2, s.descriptor.name.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_int64(stmt, 3, s.address);
            sqlite3_bind_int64(stmt, 4, s.deviceInfo.sensorType);
            sqlite3_bind_int64(stmt, 5, s.deviceInfo.hardwareVersion);
            sqlite3_bind_int64(stmt, 6, s.deviceInfo.softwareVersion);

            sqlite3_bind_double(stmt, 7, s.calibration.temperatureBias);
            sqlite3_bind_double(stmt, 8, s.calibration.humidityBias);

			sqlite3_bind_int64(stmt, 9, s.serialNumber);
			sqlite3_bind_int64(stmt, 10, (int)s.state);
            sqlite3_bind_int(stmt, 11, s.shouldSleep ? 1 : 0);
            sqlite3_bind_int64(stmt, 12, Clock::to_time_t(s.sleepStateTimePoint));

			sqlite3_bind_int64(stmt, 13, s.errorCounters.comms);
			sqlite3_bind_int64(stmt, 14, s.errorCounters.unknownReboots);
			sqlite3_bind_int64(stmt, 15, s.errorCounters.powerOnReboots);
			sqlite3_bind_int64(stmt, 16, s.errorCounters.resetReboots);
			sqlite3_bind_int64(stmt, 17, s.errorCounters.brownoutReboots);
			sqlite3_bind_int64(stmt, 18, s.errorCounters.watchdogReboots);

            sqlite3_bind_int64(stmt, 19, Clock::to_time_t(s.lastCommsTimePoint));

			sqlite3_bind_int64(stmt, 20, s.lastConfirmedMeasurementIndex);
			sqlite3_bind_int64(stmt, 21, s.firstStoredMeasurementIndex);
			sqlite3_bind_int64(stmt, 22, s.storedMeasurementCount);
			sqlite3_bind_int64(stmt, 23, s.estimatedStoredMeasurementCount);
			
            sqlite3_bind_int64(stmt, 24, s.lastSignalStrengthB2S);
			sqlite3_bind_int64(stmt, 25, s.averageSignalStrength.b2s);
			sqlite3_bind_int64(stmt, 26, s.averageSignalStrength.s2b);

            sqlite3_bind_int(stmt, 27, s.isMeasurementValid ? 1 : 0);
			sqlite3_bind_double(stmt, 28, s.measurementTemperature);
			sqlite3_bind_double(stmt, 29, s.measurementHumidity);
			sqlite3_bind_double(stmt, 30, s.measurementVcc);
			if (sqlite3_step(stmt) != SQLITE_DONE)
			{
				s_logger.logCritical(QString("Failed to save sensor: %1").arg(sqlite3_errmsg(m_sqlite)));
				return;
			}
			sqlite3_reset(stmt);
		}
	}
 	{
		{
			const char* sql = "DELETE FROM Alarms;";
			if (sqlite3_exec(m_sqlite, sql, NULL, NULL, nullptr))
			{
				s_logger.logCritical(QString("Failed to clear alarms: %1").arg(sqlite3_errmsg(m_sqlite)));
				return;
			}
		}
		sqlite3_stmt* stmt = m_saveAlarmsStmt.get();
		for (Alarm const& a : data.alarms)
		{
			sqlite3_bind_int64(stmt, 1, a.id);
			sqlite3_bind_text(stmt, 2, a.descriptor.name.c_str(), -1, SQLITE_STATIC);
			sqlite3_bind_int(stmt, 3, a.descriptor.filterSensors ? 1 : 0);
            std::string sensors;
            for (SensorId id : a.descriptor.sensors)
            {
                sensors += std::to_string(id) + ";";
            }
			sqlite3_bind_text(stmt, 4, sensors.c_str(), -1, SQLITE_STATIC);
			sqlite3_bind_int(stmt, 5, a.descriptor.lowTemperatureWatch ? 1 : 0);
			sqlite3_bind_double(stmt, 6, a.descriptor.lowTemperature);
			sqlite3_bind_int(stmt, 7, a.descriptor.highTemperatureWatch ? 1 : 0);
			sqlite3_bind_double(stmt, 8, a.descriptor.highTemperature);
			sqlite3_bind_int(stmt, 9, a.descriptor.lowHumidityWatch ? 1 : 0);
			sqlite3_bind_double(stmt, 10, a.descriptor.lowHumidity);
			sqlite3_bind_int(stmt, 11, a.descriptor.highHumidityWatch ? 1 : 0);
			sqlite3_bind_double(stmt, 12, a.descriptor.highHumidity);
			sqlite3_bind_int(stmt, 13, a.descriptor.lowVccWatch ? 1 : 0);
			sqlite3_bind_int(stmt, 14, a.descriptor.lowSignalWatch ? 1 : 0);
			sqlite3_bind_int(stmt, 15, a.descriptor.sendEmailAction ? 1 : 0);
			if (sqlite3_step(stmt) != SQLITE_DONE)
			{
				s_logger.logCritical(QString("Failed to save alarms: %1").arg(sqlite3_errmsg(m_sqlite)));
				return;
			}
			sqlite3_reset(stmt);
		}
	}
 	{
		{
			const char* sql = "DELETE FROM Reports;";
			if (sqlite3_exec(m_sqlite, sql, NULL, NULL, nullptr))
			{
				s_logger.logCritical(QString("Failed to clear reports: %1").arg(sqlite3_errmsg(m_sqlite)));
				return;
			}
		}
		sqlite3_stmt* stmt = m_saveReportsStmt.get();
		for (Report const& r : data.reports)
		{
			sqlite3_bind_int64(stmt, 1, r.id);
			sqlite3_bind_text(stmt, 2, r.descriptor.name.c_str(), -1, SQLITE_STATIC);
			sqlite3_bind_int(stmt, 3, (int)r.descriptor.period);
			sqlite3_bind_int64(stmt, 4, std::chrono::duration_cast<std::chrono::seconds>(r.descriptor.customPeriod).count());
			sqlite3_bind_int(stmt, 5, (int)r.descriptor.data);
			sqlite3_bind_int64(stmt, 6, r.descriptor.filterSensors ? 1 : 0);
			std::string sensors;
			for (SensorId id : r.descriptor.sensors)
			{
				sensors += std::to_string(id) + ";";
			}
			sqlite3_bind_text(stmt, 7, sensors.c_str(), -1, SQLITE_STATIC);
			sqlite3_bind_int64(stmt, 8, Clock::to_time_t(r.lastTriggeredTimePoint));
			if (sqlite3_step(stmt) != SQLITE_DONE)
			{
				s_logger.logCritical(QString("Failed to save report: %1").arg(sqlite3_errmsg(m_sqlite)));
				return;
			}
			sqlite3_reset(stmt);
		}
	}

    std::cout << QString("Done saving DB. Time: %3s\n").arg(std::chrono::duration<float>(Clock::now() - start).count()).toUtf8().data();
    std::cout.flush();
}

//////////////////////////////////////////////////////////////////////////

