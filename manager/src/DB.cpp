#include "DB.h"
#include <algorithm>
#include <functional>
#include <fstream>
#include <iostream>
#include <sstream>
#include <QDateTime>
#include <QDir>
#include <QByteArray>

#include "Utils.h"
#include "Crypt.h"
#include "Logger.h"
#include "sqlite3.h"
#include "Emailer.h"

#ifdef _MSC_VER
//not #if defined(_WIN32) || defined(_WIN64) because we have strncasecmp in mingw
#define strncasecmp _strnicmp
#define strcasecmp _stricmp
#endif


extern Logger s_logger;

const IClock::duration MEASUREMENT_JITTER = std::chrono::seconds(10);
const IClock::duration COMMS_DURATION = std::chrono::seconds(10);

Q_DECLARE_METATYPE(DB::Measurement);

//////////////////////////////////////////////////////////////////////////

DB::DB()
	: DB(std::make_shared<SystemClock>())
{
}

//////////////////////////////////////////////////////////////////////////

DB::DB(std::shared_ptr<IClock> clock)
	: m_clock(std::move(clock))
{
	qRegisterMetaType<UserId>("UserId");
    qRegisterMetaType<BaseStationId>("BaseStationId");
	qRegisterMetaType<SensorId>("SensorId");
	qRegisterMetaType<SensorAddress>("SensorAddress");
	qRegisterMetaType<SensorSerialNumber>("SensorSerialNumber");
	qRegisterMetaType<MeasurementId>("MeasurementId");
	qRegisterMetaType<AlarmId>("AlarmId");
	qRegisterMetaType<ReportId>("ReportId");
	qRegisterMetaType<Measurement>("Measurement");
	qRegisterMetaType<AlarmTriggers>("AlarmTriggers");

	m_emailer.reset(new Emailer(*this));
}

//////////////////////////////////////////////////////////////////////////

DB::~DB()
{
	close();
}

//////////////////////////////////////////////////////////////////////////

Result<void> DB::create(sqlite3& db)
{
	sqlite3_exec(&db, "BEGIN TRANSACTION;", NULL, NULL, NULL);
	utils::epilogue epi([&db] { sqlite3_exec(&db, "END TRANSACTION;", NULL, NULL, NULL); });

	{
		const char* sql = "CREATE TABLE GeneralSettings (id INTEGER PRIMARY KEY, dateTimeFormat INTEGER);";
		if (sqlite3_exec(&db, sql, NULL, NULL, nullptr))
		{
			Error error(QString("Error executing SQLite3 statement: %1").arg(sqlite3_errmsg(&db)).toUtf8().data());
			return error;
		}
		const char* sqlInsert = "INSERT INTO GeneralSettings VALUES(0, 0);";
		if (sqlite3_exec(&db, sqlInsert, NULL, NULL, nullptr))
		{
			Error error(QString("Error executing SQLite3 statement: %1").arg(sqlite3_errmsg(&db)).toUtf8().data());
			return error;
		}
	}
	{
		const char* sql = "CREATE TABLE CsvSettings (id INTEGER PRIMARY KEY, dateTimeFormatOverride INTEGER, unitsFormat INTEGER, "
			"exportId BOOLEAN, exportIndex BOOLEAN, exportSensorName BOOLEAN, exportSensorSN BOOLEAN, exportTimePoint BOOLEAN, exportReceivedTimePoint BOOLEAN, "
			"exportTemperature BOOLEAN, exportHumidity BOOLEAN, exportBattery BOOLEAN, exportSignal BOOLEAN, "
            "decimalPlaces INTEGER, separator STRING);";
		if (sqlite3_exec(&db, sql, NULL, NULL, nullptr))
		{
			Error error(QString("Error executing SQLite3 statement: %1").arg(sqlite3_errmsg(&db)).toUtf8().data());
			return error;
		}
        const char* sqlInsert = "INSERT INTO CsvSettings VALUES(0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, ',');";
		if (sqlite3_exec(&db, sqlInsert, NULL, NULL, nullptr))
		{
			Error error(QString("Error executing SQLite3 statement: %1").arg(sqlite3_errmsg(&db)).toUtf8().data());
			return error;
		}
	}
	{
		const char* sql = "CREATE TABLE EmailSettings (id INTEGER PRIMARY KEY, host STRING, port INTEGER, connection INTEGER, username STRING, password STRING, sender STRING, recipients STRING);";
		if (sqlite3_exec(&db, sql, NULL, NULL, nullptr))
		{
			Error error(QString("Error executing SQLite3 statement: %1").arg(sqlite3_errmsg(&db)).toUtf8().data());
			return error;
		}
		const char* sqlInsert = "INSERT INTO EmailSettings VALUES(0, '', 465, 0, '', '', '', '');";
		if (sqlite3_exec(&db, sqlInsert, NULL, NULL, nullptr))
		{
			Error error(QString("Error executing SQLite3 statement: %1").arg(sqlite3_errmsg(&db)).toUtf8().data());
			return error;
		}
	}
	{
		const char* sql = "CREATE TABLE FtpSettings (id INTEGER PRIMARY KEY, host STRING, port INTEGER, username STRING, password STRING, folder STRING, uploadBackups BOOLEAN, uploadPeriod INTEGER);";
		if (sqlite3_exec(&db, sql, NULL, NULL, nullptr))
		{
			Error error(QString("Error executing SQLite3 statement: %1").arg(sqlite3_errmsg(&db)).toUtf8().data());
			return error;
		}
		const char* sqlInsert = "INSERT INTO FtpSettings VALUES(0, '', 21, '', '', '', 0, 14400);";
		if (sqlite3_exec(&db, sqlInsert, NULL, NULL, nullptr))
		{
			Error error(QString("Error executing SQLite3 statement: %1").arg(sqlite3_errmsg(&db)).toUtf8().data());
			return error;
		}
	}
	{
		const char* sql = "CREATE TABLE Users (id INTEGER PRIMARY KEY, name STRING, passwordHash STRING, permissions INTEGER, type INTEGER, lastLogin DATETIME);";
		if (sqlite3_exec(&db, sql, NULL, NULL, nullptr))
		{
			Error error(QString("Error executing SQLite3 statement: %1").arg(sqlite3_errmsg(&db)).toUtf8().data());
			return error;
		}
	}
	{
		const char* sql = "CREATE TABLE BaseStations (id INTEGER PRIMARY KEY, name STRING, mac STRING);";
		if (sqlite3_exec(&db, sql, NULL, NULL, nullptr))
		{
			Error error(QString("Error executing SQLite3 statement: %1").arg(sqlite3_errmsg(&db)).toUtf8().data());
			return error;
		}
	}
	{
		const char* sql = "CREATE TABLE SensorTimeConfigs (baselineMeasurementTimePoint DATETIME, baselineMeasurementIndex INTEGER, measurementPeriod INTEGER, commsPeriod INTEGER);";
		if (sqlite3_exec(&db, sql, NULL, NULL, nullptr))
		{
			Error error(QString("Error executing SQLite3 statement: %1").arg(sqlite3_errmsg(&db)).toUtf8().data());
			return error;
		}
	}
	{
		const char* sql = "CREATE TABLE SensorSettings (id INTEGER PRIMARY KEY, radioPower INTEGER, retries INTEGER, alertBatteryLevel REAL, alertSignalStrengthLevel REAL);";
		if (sqlite3_exec(&db, sql, NULL, NULL, nullptr))
		{
			Error error(QString("Error executing SQLite3 statement: %1").arg(sqlite3_errmsg(&db)).toUtf8().data());
			return error;
		}
		const char* sqlInsert = "INSERT INTO SensorSettings VALUES(0, 0, 2, 0.1, 0.1);";
		if (sqlite3_exec(&db, sqlInsert, NULL, NULL, nullptr))
		{
			Error error(QString("Error executing SQLite3 statement: %1").arg(sqlite3_errmsg(&db)).toUtf8().data());
			return error;
		}
	}
	{
		const char* sql = "CREATE TABLE Sensors (id INTEGER PRIMARY KEY, name STRING, address INTEGER, sensorType INTEGER, hardwareVersion INTEGER, softwareVersion INTEGER, "
            "temperatureBias REAL, humidityBias REAL, serialNumber INTEGER, state INTEGER, shouldSleep BOOLEAN, sleepStateTimePoint DATETIME, statsCommsBlackouts INTEGER, statsCommsFailures INTEGER, "
            "statsUnknownReboots INTEGER, statsPowerOnReboots INTEGER, statsResetReboots INTEGER, statsBrownoutReboots INTEGER, statsWatchdogReboots INTEGER, statsCommsRetries INTEGER, statsAsleep INTEGER, statsAwake INTEGER, statsCommsRounds INTEGER, statsMeasurementRounds INTEGER, "
			"lastCommsTimePoint DATETIME, lastConfirmedMeasurementIndex INTEGER, lastAlarmProcessesMeasurementIndex INTEGER, "
            "firstStoredMeasurementIndex INTEGER, storedMeasurementCount INTEGER, estimatedStoredMeasurementCount INTEGER, lastSignalStrengthB2S INTEGER, averageSignalStrengthB2S INTEGER, averageSignalStrengthS2B INTEGER, "
            "isRTMeasurementValid BOOLEAN, rtMeasurementTemperature REAL, rtMeasurementHumidity REAL, rtMeasurementVcc REAL);";
		if (sqlite3_exec(&db, sql, NULL, NULL, nullptr))
		{
			Error error(QString("Error executing SQLite3 statement: %1").arg(sqlite3_errmsg(&db)).toUtf8().data());
			return error;
		}
	}
	{
		const char* sql = "CREATE TABLE Alarms (id INTEGER PRIMARY KEY, name STRING, filterSensors BOOLEAN, sensors STRING, lowTemperatureWatch BOOLEAN, lowTemperatureSoft REAL, lowTemperatureHard REAL, highTemperatureWatch BOOLEAN, highTemperatureSoft REAL, highTemperatureHard REAL, "
            "lowHumidityWatch BOOLEAN, lowHumiditySoft REAL, lowHumidityHard REAL, highHumidityWatch BOOLEAN, highHumiditySoft REAL, highHumidityHard REAL, "
            "lowVccWatch BOOLEAN, lowSignalWatch BOOLEAN, sensorBlackoutWatch BOOLEAN, baseStationDisconnectedWatch BOOLEAN, sendEmailAction BOOLEAN, resendPeriod INTEGER, triggersPerSensor STRING, triggersPerBaseStation STRING, lastTriggeredTimePoint DATETIME);";
		if (sqlite3_exec(&db, sql, NULL, NULL, nullptr))
		{
			Error error(QString("Error executing SQLite3 statement: %1").arg(sqlite3_errmsg(&db)).toUtf8().data());
			return error;
		}
	}
	{
		const char* sql = "CREATE TABLE Reports (id INTEGER PRIMARY KEY, name STRING, period INTEGER, customPeriod INTEGER, filterSensors BOOLEAN, sensors STRING, lastTriggeredTimePoint DATETIME);";
		if (sqlite3_exec(&db, sql, NULL, NULL, nullptr))
		{
			Error error(QString("Error executing SQLite3 statement: %1").arg(sqlite3_errmsg(&db)).toUtf8().data());
			return error;
		}
	} 
    {
		const char* sql = "CREATE TABLE Measurements (id INTEGER PRIMARY KEY AUTOINCREMENT, timePoint DATETIME, receivedTimePoint DATETIME, idx INTEGER, sensorId INTEGER, temperature REAL, humidity REAL, vcc REAL, signalStrengthS2B INTEGER, signalStrengthB2S INTEGER, "
                                                     "sensorErrors INTEGER, alarmTriggersCurrent INTEGER, alarmTriggersAdded INTEGER, alarmTriggersRemoved INTEGER, UNIQUE(idx, sensorId));";
		if (sqlite3_exec(&db, sql, NULL, NULL, nullptr))
		{
			Error error(QString("Error executing SQLite3 statement: %1").arg(sqlite3_errmsg(&db)).toUtf8().data());
			return error;
		}
	}
	{
		const char* sql = "CREATE INDEX idx_sensorId ON Measurements (idx, sensorId);";
		if (sqlite3_exec(&db, sql, NULL, NULL, nullptr))
		{
			Error error(QString("Error executing SQLite3 statement: %1").arg(sqlite3_errmsg(&db)).toUtf8().data());
			return error;
		}
	}
	{
		const char* sql = "CREATE INDEX measurementsIdx ON Measurements(idx);";
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
	IClock::time_point start = m_clock->now();

    Data data;

    {
		sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(&db, "REPLACE INTO BaseStations (id, name, mac) "
                                    "VALUES (?1, ?2, ?3);", -1, &stmt, NULL) != SQLITE_OK)
		{
			return Error(QString("Cannot prepare query: %1").arg(sqlite3_errmsg(&db)).toUtf8().data());
		}
        m_saveBaseStationsStmt.reset(stmt, &sqlite3_finalize);
    }
	{
		sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(&db, "INSERT INTO SensorTimeConfigs (baselineMeasurementTimePoint, baselineMeasurementIndex, measurementPeriod, commsPeriod) "
						            "VALUES (?1, ?2, ?3, ?4);", -1, &stmt, NULL) != SQLITE_OK)
		{
			return Error(QString("Cannot prepare query: %1").arg(sqlite3_errmsg(&db)).toUtf8().data());
		}
		m_saveSensorTimeConfigsStmt.reset(stmt, &sqlite3_finalize);
	}
	{
		sqlite3_stmt* stmt;
		if (sqlite3_prepare_v2(&db, "REPLACE INTO SensorSettings (id, radioPower, retries, alertBatteryLevel, alertSignalStrengthLevel) "
							   "VALUES (0, ?1, ?2, ?3, ?4);", -1, &stmt, NULL) != SQLITE_OK)
		{
			return Error(QString("Cannot prepare query: %1").arg(sqlite3_errmsg(&db)).toUtf8().data());
		}
		m_saveSensorSettingsStmt.reset(stmt, &sqlite3_finalize);
	}
	{
		sqlite3_stmt* stmt;
		if (sqlite3_prepare_v2(&db, "REPLACE INTO Sensors (id, name, address, sensorType, hardwareVersion, softwareVersion, "
						                       "temperatureBias, humidityBias, serialNumber, state, shouldSleep, sleepStateTimePoint, statsCommsBlackouts, statsCommsFailures, "
						                       "statsUnknownReboots, statsPowerOnReboots, statsResetReboots, statsBrownoutReboots, statsWatchdogReboots, statsCommsRetries, statsAsleep, statsAwake, statsCommsRounds, statsMeasurementRounds, "
											   "lastCommsTimePoint, lastConfirmedMeasurementIndex, lastAlarmProcessesMeasurementIndex, "
						                       "firstStoredMeasurementIndex, storedMeasurementCount, estimatedStoredMeasurementCount, lastSignalStrengthB2S, averageSignalStrengthB2S, averageSignalStrengthS2B, "
						                       "isRTMeasurementValid, rtMeasurementTemperature, rtMeasurementHumidity, rtMeasurementVcc) "
						            "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12, ?13, ?14, ?15, ?16, ?17, ?18, ?19, ?20, ?21, ?22, ?23, ?24, ?25, ?26, ?27, ?28, ?29, ?30, ?31, ?32, ?33, ?34, ?35, ?36, ?37);", -1, &stmt, NULL) != SQLITE_OK)
		{
			return Error(QString("Cannot prepare query: %1").arg(sqlite3_errmsg(&db)).toUtf8().data());
		}
		m_saveSensorsStmt.reset(stmt, &sqlite3_finalize);
	}
	{
		sqlite3_stmt* stmt;
		if (sqlite3_prepare_v2(&db, "REPLACE INTO Alarms (id, name, filterSensors, sensors, lowTemperatureWatch, lowTemperatureSoft, lowTemperatureHard, highTemperatureWatch, highTemperatureSoft, highTemperatureHard, "
						                       "lowHumidityWatch, lowHumiditySoft, lowHumidityHard, highHumidityWatch, highHumiditySoft, highHumidityHard, "
						                       "lowVccWatch, lowSignalWatch, sensorBlackoutWatch, baseStationDisconnectedWatch, sendEmailAction, resendPeriod, triggersPerSensor, triggersPerBaseStation, "
											   "lastTriggeredTimePoint) "
							        "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12, ?13, ?14, ?15, ?16, ?17, ?18, ?19, ?20, ?21, ?22, ?23, ?24, ?25);", -1, &stmt, NULL) != SQLITE_OK)
		{
			return Error(QString("Cannot prepare query: %1").arg(sqlite3_errmsg(&db)).toUtf8().data());
		}
		m_saveAlarmsStmt.reset(stmt, &sqlite3_finalize);
	}
	{
		sqlite3_stmt* stmt;
		if (sqlite3_prepare_v2(&db, "REPLACE INTO Reports (id, name, period, customPeriod, filterSensors, sensors, lastTriggeredTimePoint) "
							        "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7);", -1, &stmt, NULL) != SQLITE_OK)
		{
			return Error(QString("Cannot prepare query: %1").arg(sqlite3_errmsg(&db)).toUtf8().data());
		}
		m_saveReportsStmt.reset(stmt, &sqlite3_finalize);
    }
    {
		sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(&db, "INSERT OR IGNORE INTO Measurements (timePoint, receivedTimePoint, idx, sensorId, temperature, humidity, vcc, signalStrengthS2B, signalStrengthB2S, sensorErrors, alarmTriggersCurrent, alarmTriggersAdded, alarmTriggersRemoved) "
							        "VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12, ?13);", -1, &stmt, NULL) != SQLITE_OK)
		{
			return Error(QString("Cannot prepare query: %1").arg(sqlite3_errmsg(&db)).toUtf8().data());
		}
		m_addMeasurementsStmt.reset(stmt, &sqlite3_finalize);
    }


	{
		const char* sql = "SELECT dateTimeFormat FROM GeneralSettings;";
		sqlite3_stmt* stmt;
		if (sqlite3_prepare_v2(&db, sql, -1, &stmt, 0) != SQLITE_OK)
		{
			return Error(QString("Cannot load general settings: %1").arg(sqlite3_errmsg(&db)).toUtf8().data());
		}
		utils::epilogue epi([stmt] { sqlite3_finalize(stmt); });

		if (sqlite3_step(stmt) != SQLITE_ROW)
		{
			return Error(QString("Cannot load general settings row: %1").arg(sqlite3_errmsg(&db)).toUtf8().data());
		}
		data.generalSettings.dateTimeFormat = (DateTimeFormat)sqlite3_column_int64(stmt, 0);
	}
	{
		//id INTEGER PRIMARY KEY, dateTimeFormatOverride INTEGER, unitsFormat INTEGER, "
		//	"exportId BOOLEAN, exportIndex BOOLEAN, exportSensorName BOOLEAN, exportSensorSN BOOLEAN, exportTimePoint BOOLEAN, exportReceivedTimePoint BOOLEAN, "
		//	"exportTemperature BOOLEAN, exportHumidity BOOLEAN, exportBattery BOOLEAN, exportSignal BOOLEAN, "
		//	"decimalPlaces INTEGER
		const char* sql = "SELECT dateTimeFormatOverride, unitsFormat, exportId, exportIndex, exportSensorName, exportSensorSN, exportTimePoint, exportReceivedTimePoint, "
			"exportTemperature, exportHumidity, exportBattery, exportSignal, "
            "decimalPlaces, separator FROM CsvSettings;";
		sqlite3_stmt* stmt;
		if (sqlite3_prepare_v2(&db, sql, -1, &stmt, 0) != SQLITE_OK)
		{
			return Error(QString("Cannot load csv settings: %1").arg(sqlite3_errmsg(&db)).toUtf8().data());
		}
		utils::epilogue epi([stmt] { sqlite3_finalize(stmt); });

		if (sqlite3_step(stmt) != SQLITE_ROW)
		{
			return Error(QString("Cannot load csv settings row: %1").arg(sqlite3_errmsg(&db)).toUtf8().data());
		}
		int v = sqlite3_column_int64(stmt, 0);
		if (v >= 0)
		{
			data.csvSettings.dateTimeFormatOverride = (DateTimeFormat)v;
		}
		data.csvSettings.unitsFormat = (CsvSettings::UnitsFormat)sqlite3_column_int(stmt, 1);
		data.csvSettings.exportId = sqlite3_column_int(stmt, 2) ? true : false;
		data.csvSettings.exportIndex = sqlite3_column_int(stmt, 3) ? true : false;
		data.csvSettings.exportSensorName = sqlite3_column_int(stmt, 4) ? true : false;
		data.csvSettings.exportSensorSN = sqlite3_column_int(stmt, 5) ? true : false;
		data.csvSettings.exportTimePoint = sqlite3_column_int(stmt, 6) ? true : false;
		data.csvSettings.exportReceivedTimePoint = sqlite3_column_int(stmt, 7) ? true : false;
		data.csvSettings.exportTemperature = sqlite3_column_int(stmt, 8) ? true : false;
		data.csvSettings.exportHumidity = sqlite3_column_int(stmt, 9) ? true : false;
		data.csvSettings.exportBattery = sqlite3_column_int(stmt, 10) ? true : false;
		data.csvSettings.exportSignal = sqlite3_column_int(stmt, 11) ? true : false;
		data.csvSettings.decimalPlaces = (uint32_t)sqlite3_column_int(stmt, 12);
        data.csvSettings.separator = (const char*)sqlite3_column_text(stmt, 13);
    }
	{
		//id INTEGER PRIMARY KEY, host STRING, port INTEGER, connection INTEGER, username STRING, password STRING, sender STRING, recipients STRING
		const char* sql = "SELECT host, port, connection, username, password, sender, recipients FROM EmailSettings;";
		sqlite3_stmt* stmt;
		if (sqlite3_prepare_v2(&db, sql, -1, &stmt, 0) != SQLITE_OK)
		{
			return Error(QString("Cannot load email settings: %1").arg(sqlite3_errmsg(&db)).toUtf8().data());
		}
		utils::epilogue epi([stmt] { sqlite3_finalize(stmt); });

		if (sqlite3_step(stmt) != SQLITE_ROW)
		{
			return Error(QString("Cannot load email settings row: %1").arg(sqlite3_errmsg(&db)).toUtf8().data());
		}
		data.emailSettings.host = (char const*)sqlite3_column_text(stmt, 0);
		data.emailSettings.port = sqlite3_column_int(stmt, 1);
		data.emailSettings.connection = (EmailSettings::Connection)sqlite3_column_int(stmt, 2);
		data.emailSettings.username = (char const*)sqlite3_column_text(stmt, 3);
		data.emailSettings.password = (char const*)sqlite3_column_text(stmt, 4);
		data.emailSettings.sender = (char const*)sqlite3_column_text(stmt, 5);
		QString recipients = (char const*)sqlite3_column_text(stmt, 6);

		QStringList l = recipients.split(QChar(';'), QString::SkipEmptyParts);
		for (QString str : l)
		{
			data.emailSettings.recipients.push_back(str.trimmed().toUtf8().data());
		}
	}
	{
		//FtpSettings (id INTEGER PRIMARY KEY, host STRING, port INTEGER, username STRING, password STRING, folder STRING, uploadBackups BOOLEAN, uploadPeriod INTEGER);";
		const char* sql = "SELECT host, port, username, password, folder, uploadBackups, uploadPeriod FROM FtpSettings;";
		sqlite3_stmt* stmt;
		if (sqlite3_prepare_v2(&db, sql, -1, &stmt, 0) != SQLITE_OK)
		{
			return Error(QString("Cannot load ftp settings: %1").arg(sqlite3_errmsg(&db)).toUtf8().data());
		}
		utils::epilogue epi([stmt] { sqlite3_finalize(stmt); });

		if (sqlite3_step(stmt) != SQLITE_ROW)
		{
			return Error(QString("Cannot load ftp settings row: %1").arg(sqlite3_errmsg(&db)).toUtf8().data());
		}

		data.ftpSettings.host = (char const*)sqlite3_column_text(stmt, 0);
		data.ftpSettings.port = sqlite3_column_int(stmt, 1);
		data.ftpSettings.username = (char const*)sqlite3_column_text(stmt, 2);
		data.ftpSettings.password = (char const*)sqlite3_column_text(stmt, 3);
		data.ftpSettings.folder = (char const*)sqlite3_column_text(stmt, 4);
		data.ftpSettings.uploadBackups = sqlite3_column_int(stmt, 5) ? true : false;
		data.ftpSettings.uploadBackupsPeriod = std::chrono::seconds(sqlite3_column_int64(stmt, 6));
	}
	{
		//Users (id INTEGER PRIMARY KEY, name STRING, passwordHash STRING, permissions INTEGER, type INTEGER, lastLogin DATETIME);";
		const char* sql = "SELECT id, name, passwordHash, permissions, type, lastLogin FROM Users;";
		sqlite3_stmt* stmt;
		if (sqlite3_prepare_v2(&db, sql, -1, &stmt, 0) != SQLITE_OK)
		{
			return Error(QString("Cannot load user settings: %1").arg(sqlite3_errmsg(&db)).toUtf8().data());
		}
		utils::epilogue epi([stmt] { sqlite3_finalize(stmt); });

		while (sqlite3_step(stmt) == SQLITE_ROW)
		{
			User user;
			user.id = sqlite3_column_int(stmt, 0);
			user.descriptor.name = (char const*)sqlite3_column_text(stmt, 1);
			user.descriptor.passwordHash = (char const*)sqlite3_column_text(stmt, 2);
			user.descriptor.permissions = sqlite3_column_int(stmt, 3);
			user.descriptor.type = (UserDescriptor::Type)sqlite3_column_int(stmt, 4);
			user.lastLogin = IClock::from_time_t(sqlite3_column_int64(stmt, 5));
			data.users.push_back(std::move(user));
		}
	}
	{
		//const char* sql = "CREATE TABLE BaseStations (id INTEGER PRIMARY KEY, name STRING, mac STRING);";
		const char* sql = "SELECT id, name, mac "
                          "FROM BaseStations;";
		sqlite3_stmt* stmt;
		if (sqlite3_prepare_v2(&db, sql, -1, &stmt, 0) != SQLITE_OK)
		{
			return Error(QString("Cannot prepare query: %1").arg(sqlite3_errmsg(&db)).toUtf8().data());
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
		const char* sql = "SELECT baselineMeasurementTimePoint, baselineMeasurementIndex, measurementPeriod, commsPeriod "
                          "FROM SensorTimeConfigs;";
		sqlite3_stmt* stmt;
		if (sqlite3_prepare_v2(&db, sql, -1, &stmt, 0) != SQLITE_OK)
		{
			return Error(QString("Cannot load sensors configs: %1").arg(sqlite3_errmsg(&db)).toUtf8().data());
		}
		utils::epilogue epi([stmt] { sqlite3_finalize(stmt); });

		while (sqlite3_step(stmt) == SQLITE_ROW)
		{
            SensorTimeConfig sc;
			sc.baselineMeasurementTimePoint = IClock::from_time_t(sqlite3_column_int64(stmt, 0));
			sc.baselineMeasurementIndex = sqlite3_column_int64(stmt, 1);
			sc.descriptor.measurementPeriod = std::chrono::seconds(sqlite3_column_int64(stmt, 2));
			sc.descriptor.commsPeriod = std::chrono::seconds(sqlite3_column_int64(stmt, 3));
			data.sensorTimeConfigs.push_back(std::move(sc));
		}
	}

	{
		const char* sql = "SELECT radioPower, retries, alertBatteryLevel, alertSignalStrengthLevel "
			              "FROM SensorSettings;";
		sqlite3_stmt* stmt;
		if (sqlite3_prepare_v2(&db, sql, -1, &stmt, 0) != SQLITE_OK)
		{
			return Error(QString("Cannot load sensors configs: %1").arg(sqlite3_errmsg(&db)).toUtf8().data());
		}
		utils::epilogue epi([stmt] { sqlite3_finalize(stmt); });

		if (sqlite3_step(stmt) != SQLITE_ROW)
		{
            return Error(QString("Cannot load sensor config: %1").arg(sqlite3_errmsg(&db)).toUtf8().data());
		}

        SensorSettings ss;
		ss.radioPower = (int8_t)sqlite3_column_int64(stmt, 0);
		ss.retries = (uint8_t)std::clamp<int64_t>(sqlite3_column_int64(stmt, 1), 1, 10);
		ss.alertBatteryLevel = std::clamp((float)sqlite3_column_double(stmt, 2), 0.f, 1.f);
		ss.alertSignalStrengthLevel = std::clamp((float)sqlite3_column_double(stmt, 3), 0.f, 1.f);
		data.sensorSettings = ss;
	}

	{
		const char* sql = "SELECT id, name, address, sensorType, hardwareVersion, softwareVersion, "
			 			        "temperatureBias, humidityBias, serialNumber, state, shouldSleep, sleepStateTimePoint, statsCommsBlackouts, statsCommsFailures, "
			 			        "statsUnknownReboots, statsPowerOnReboots, statsResetReboots, statsBrownoutReboots, statsWatchdogReboots, statsCommsRetries, statsAsleep, statsAwake, statsCommsRounds, statsMeasurementRounds, "
							    "lastCommsTimePoint, lastConfirmedMeasurementIndex, lastAlarmProcessesMeasurementIndex, "
			 			        "firstStoredMeasurementIndex, storedMeasurementCount, estimatedStoredMeasurementCount, lastSignalStrengthB2S, averageSignalStrengthB2S, averageSignalStrengthS2B, "
			 			        "isRTMeasurementValid, rtMeasurementTemperature, rtMeasurementHumidity, rtMeasurementVcc "
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
			int index = 0;
			s.id = sqlite3_column_int64(stmt, index++);
			s.descriptor.name = (char const*)sqlite3_column_text(stmt, index++);
			s.address = sqlite3_column_int64(stmt, index++);
			s.deviceInfo.sensorType = (uint8_t)sqlite3_column_int64(stmt, index++);
			s.deviceInfo.hardwareVersion = (uint8_t)sqlite3_column_int64(stmt, index++);
			s.deviceInfo.softwareVersion = (uint8_t)sqlite3_column_int64(stmt, index++);
			s.calibration.temperatureBias = (float)sqlite3_column_double(stmt, index++);
			s.calibration.humidityBias = (float)sqlite3_column_double(stmt, index++);
			s.serialNumber = (uint32_t)sqlite3_column_int64(stmt, index++);
			s.state = (Sensor::State)sqlite3_column_int64(stmt, index++);
			s.shouldSleep = sqlite3_column_int64(stmt, index++) ? true : false;
			s.sleepStateTimePoint = IClock::from_time_t(sqlite3_column_int64(stmt, index++));
			s.stats.commsBlackouts = (uint32_t)sqlite3_column_int64(stmt, index++);
			s.stats.commsFailures = (uint32_t)sqlite3_column_int64(stmt, index++);
			s.stats.unknownReboots = (uint32_t)sqlite3_column_int64(stmt, index++);
			s.stats.powerOnReboots = (uint32_t)sqlite3_column_int64(stmt, index++);
			s.stats.resetReboots = (uint32_t)sqlite3_column_int64(stmt, index++);
			s.stats.brownoutReboots = (uint32_t)sqlite3_column_int64(stmt, index++);
			s.stats.watchdogReboots = (uint32_t)sqlite3_column_int64(stmt, index++);
			s.stats.commsRetries = (uint32_t)sqlite3_column_int64(stmt, index++);
			s.stats.asleep = std::chrono::milliseconds(sqlite3_column_int64(stmt, index++));
			s.stats.awake = std::chrono::milliseconds(sqlite3_column_int64(stmt, index++));
			s.stats.commsRounds = (uint32_t)sqlite3_column_int64(stmt, index++);
			s.stats.measurementRounds = (uint32_t)sqlite3_column_int64(stmt, index++);
			s.lastCommsTimePoint = IClock::from_time_t(sqlite3_column_int64(stmt, index++));
			s.lastConfirmedMeasurementIndex = (uint32_t)sqlite3_column_int64(stmt, index++);
			s.lastAlarmProcessesMeasurementIndex = (uint32_t)sqlite3_column_int64(stmt, index++);
			s.firstStoredMeasurementIndex = (uint32_t)sqlite3_column_int64(stmt, index++);
			s.storedMeasurementCount = (uint32_t)sqlite3_column_int64(stmt, index++);
			s.estimatedStoredMeasurementCount = (uint32_t)sqlite3_column_int64(stmt, index++);
			s.lastSignalStrengthB2S = (int16_t)sqlite3_column_int64(stmt, index++);
			s.averageSignalStrength.b2s = (int16_t)sqlite3_column_int64(stmt, index++);
			s.averageSignalStrength.s2b = (int16_t)sqlite3_column_int64(stmt, index++);
			s.isRTMeasurementValid = sqlite3_column_int64(stmt, index++) ? true : false;
			s.rtMeasurementTemperature = (float)sqlite3_column_double(stmt, index++);
			s.rtMeasurementHumidity = (float)sqlite3_column_double(stmt, index++);
			s.rtMeasurementVcc = (float)sqlite3_column_double(stmt, index++);
			Q_ASSERT(index == 37);
			data.sensors.push_back(std::move(s));
		}
	}

	{
		const char* sql = "SELECT id, name, filterSensors, sensors, lowTemperatureWatch, lowTemperatureSoft, lowTemperatureHard, highTemperatureWatch, highTemperatureSoft, highTemperatureHard, "
			 			            "lowHumidityWatch, lowHumiditySoft, lowHumidityHard, highHumidityWatch, highHumiditySoft, highHumidityHard, "
							        "lowVccWatch, lowSignalWatch, sensorBlackoutWatch, baseStationDisconnectedWatch, sendEmailAction, resendPeriod, triggersPerSensor, triggersPerBaseStation, "
				                    "lastTriggeredTimePoint "
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
			int index = 0;
            a.id = sqlite3_column_int64(stmt, index++);
			a.descriptor.name = (char const*)sqlite3_column_text(stmt, index++);
            a.descriptor.filterSensors = sqlite3_column_int64(stmt, index++) ? true : false;
			{
				QString sensors = (char const*)sqlite3_column_text(stmt, index++);
				QStringList l = sensors.split(QChar(';'), QString::SkipEmptyParts);
				for (QString str : l)
				{
					a.descriptor.sensors.insert(atoll(str.trimmed().toUtf8().data()));
				}
			}
            a.descriptor.lowTemperatureWatch = sqlite3_column_int64(stmt, index++) ? true : false;
            a.descriptor.lowTemperatureSoft = (float)sqlite3_column_double(stmt, index++);
            a.descriptor.lowTemperatureHard = (float)sqlite3_column_double(stmt, index++);
            std::tie(a.descriptor.lowTemperatureHard, a.descriptor.lowTemperatureSoft) = std::minmax(a.descriptor.lowTemperatureSoft, a.descriptor.lowTemperatureHard);
			a.descriptor.highTemperatureWatch = sqlite3_column_int64(stmt, index++) ? true : false;
			a.descriptor.highTemperatureSoft = (float)sqlite3_column_double(stmt, index++);
			a.descriptor.highTemperatureHard = (float)sqlite3_column_double(stmt, index++);
			std::tie(a.descriptor.highTemperatureSoft, a.descriptor.highTemperatureHard) = std::minmax(a.descriptor.highTemperatureSoft, a.descriptor.highTemperatureHard);
			a.descriptor.lowHumidityWatch = sqlite3_column_int64(stmt, index++) ? true : false;
			a.descriptor.lowHumiditySoft = (float)sqlite3_column_double(stmt, index++);
			a.descriptor.lowHumidityHard = (float)sqlite3_column_double(stmt, index++);
			std::tie(a.descriptor.lowHumidityHard, a.descriptor.lowHumiditySoft) = std::minmax(a.descriptor.lowHumiditySoft, a.descriptor.lowHumidityHard);
			a.descriptor.highHumidityWatch = sqlite3_column_int64(stmt, index++) ? true : false;
			a.descriptor.highHumiditySoft = (float)sqlite3_column_double(stmt, index++);
			a.descriptor.highHumidityHard = (float)sqlite3_column_double(stmt, index++);
			std::tie(a.descriptor.highHumiditySoft, a.descriptor.highHumidityHard) = std::minmax(a.descriptor.highHumiditySoft, a.descriptor.highHumidityHard);
			a.descriptor.lowVccWatch = sqlite3_column_int64(stmt, index++);
			a.descriptor.lowSignalWatch = sqlite3_column_int64(stmt, index++);
			a.descriptor.sensorBlackoutWatch = sqlite3_column_int64(stmt, index++);
			a.descriptor.baseStationDisconnectedWatch = sqlite3_column_int64(stmt, index++);
			a.descriptor.sendEmailAction = sqlite3_column_int64(stmt, index++);
            a.descriptor.resendPeriod = std::chrono::seconds(sqlite3_column_int64(stmt, index++));
			{
				QString triggers = (char const*)sqlite3_column_text(stmt, index++);
				QStringList l = triggers.split(QChar(';'), QString::SkipEmptyParts);
				for (QString str : l)
				{
                    QStringList l2 = str.trimmed().split(QChar('/'), QString::SkipEmptyParts);
                    if (l2.size() == 2)
                    {
					    a.triggersPerSensor.emplace(atoll(l2[0].trimmed().toUtf8().data()), atoi(l2[1].trimmed().toUtf8().data()));
                    }
                    else
                    {
                        Q_ASSERT(false);
                    }
				}
			}
			{
				QString triggers = (char const*)sqlite3_column_text(stmt, index++);
				QStringList l = triggers.split(QChar(';'), QString::SkipEmptyParts);
				for (QString str : l)
				{
					QStringList l2 = str.trimmed().split(QChar('/'), QString::SkipEmptyParts);
					if (l2.size() == 2)
					{
						a.triggersPerBaseStation.emplace(atoll(l2[0].trimmed().toUtf8().data()), atoi(l2[1].trimmed().toUtf8().data()));
					}
					else
					{
						Q_ASSERT(false);
					}
				}
			}
			a.lastTriggeredTimePoint = IClock::from_time_t(sqlite3_column_int64(stmt, index++));
			Q_ASSERT(index == 25);
			data.alarms.push_back(std::move(a));
		}
	}
	{
		const char* sql = "SELECT id, name, period, customPeriod, filterSensors, sensors, lastTriggeredTimePoint "
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
			r.descriptor.filterSensors = sqlite3_column_int64(stmt, 4) ? true : false;
			QString sensors = (char const*)sqlite3_column_text(stmt, 5);
			QStringList l = sensors.split(QChar(';'), QString::SkipEmptyParts);
			for (QString str : l)
			{
				r.descriptor.sensors.insert(atoll(str.trimmed().toUtf8().data()));
			}
			r.lastTriggeredTimePoint = IClock::from_time_t(sqlite3_column_int64(stmt, 6));
			data.reports.push_back(std::move(r));
		}
    }

	for (const User& user : data.users)
	{
		data.lastUserId = std::max(data.lastUserId, user.id);
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

	bool needsSave = false;
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
        if (m_data.sensorTimeConfigs.empty())
        {
            _addSensorTimeConfig(SensorTimeConfigDescriptor()).ignore();
			needsSave = true;
        }
        else
        {
            SensorTimeConfig& config = m_data.sensorTimeConfigs.back();
            config.computedCommsPeriod = computeActualCommsPeriod(config.descriptor);
			data.sensorTimeConfigsChanged = true;
        }

        s_logger.logVerbose(QString("Done loading DB. Time: %3s").arg(std::chrono::duration<float>(m_clock->now() - start).count()));
    }

	m_sqlite = &db;

	if (needsSave)
	{
		save(true);
	}

    return success;
}

//////////////////////////////////////////////////////////////////////////

void DB::scheduleSave()
{
	m_saveScheduled = true;
}

//////////////////////////////////////////////////////////////////////////

void DB::save(bool newTransaction)
{
	save(m_data, newTransaction);
	m_saveScheduled = false;
}

//////////////////////////////////////////////////////////////////////////

void DB::save(Data& data, bool newTransaction) const
{
	IClock::time_point start = m_clock->now();

	if (newTransaction)
	{
		sqlite3_exec(m_sqlite, "BEGIN TRANSACTION;", NULL, NULL, NULL);
	}
	utils::epilogue epi([this, newTransaction]
	{
		if (newTransaction)
		{
			sqlite3_exec(m_sqlite, "END TRANSACTION;", NULL, NULL, NULL);
		}
	});

	if (data.generalSettingsChanged)
	{
		data.generalSettingsChanged = false;
		sqlite3_stmt* stmt;
		sqlite3_prepare_v2(m_sqlite, "REPLACE INTO GeneralSettings (id, dateTimeFormat) VALUES (0, ?1);", -1, &stmt, NULL);
		utils::epilogue epi([stmt] { sqlite3_finalize(stmt); });

		sqlite3_bind_int64(stmt, 1, (int)data.generalSettings.dateTimeFormat);
		if (sqlite3_step(stmt) != SQLITE_DONE)
		{
			s_logger.logCritical(QString("Failed to save general settings: %1").arg(sqlite3_errmsg(m_sqlite)));
			return;
		}
	}

	if (data.csvSettingsChanged)
	{
		data.csvSettingsChanged = false;
		sqlite3_stmt* stmt;
		sqlite3_prepare_v2(m_sqlite, "REPLACE INTO CsvSettings (id, dateTimeFormatOverride, unitsFormat, "
						   "exportId, exportIndex, exportSensorName, exportSensorSN, exportTimePoint, exportReceivedTimePoint, "
						   "exportTemperature, exportHumidity, exportBattery, exportSignal, "
						   "decimalPlaces, separator) VALUES (0, ?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12, ?13, ?14);", -1, &stmt, NULL);
		utils::epilogue epi([stmt] { sqlite3_finalize(stmt); });

		sqlite3_bind_int64(stmt, 1, data.csvSettings.dateTimeFormatOverride.has_value() ? (int)data.csvSettings.dateTimeFormatOverride.value() : -1);
		sqlite3_bind_int64(stmt, 2, (int)data.csvSettings.unitsFormat);
		sqlite3_bind_int(stmt, 3, data.csvSettings.exportId ? 1 : 0);
		sqlite3_bind_int(stmt, 4, data.csvSettings.exportIndex ? 1 : 0);
		sqlite3_bind_int(stmt, 5, data.csvSettings.exportSensorName ? 1 : 0);
		sqlite3_bind_int(stmt, 6, data.csvSettings.exportSensorSN ? 1 : 0);
		sqlite3_bind_int(stmt, 7, data.csvSettings.exportTimePoint ? 1 : 0);
		sqlite3_bind_int(stmt, 8, data.csvSettings.exportReceivedTimePoint ? 1 : 0);
		sqlite3_bind_int(stmt, 9, data.csvSettings.exportTemperature ? 1 : 0);
		sqlite3_bind_int(stmt, 10, data.csvSettings.exportHumidity ? 1 : 0);
		sqlite3_bind_int(stmt, 11, data.csvSettings.exportBattery ? 1 : 0);
		sqlite3_bind_int(stmt, 12, data.csvSettings.exportSignal ? 1 : 0);
		sqlite3_bind_int64(stmt, 13, data.csvSettings.decimalPlaces);
		sqlite3_bind_text(stmt, 14, data.csvSettings.separator.c_str(), -1, SQLITE_STATIC);
		if (sqlite3_step(stmt) != SQLITE_DONE)
		{
			s_logger.logCritical(QString("Failed to save csv settings: %1").arg(sqlite3_errmsg(m_sqlite)));
			return;
		}
	}

	if (data.emailSettingsChanged)
	{
		data.emailSettingsChanged = false;
		sqlite3_stmt* stmt;
		sqlite3_prepare_v2(m_sqlite, "REPLACE INTO EmailSettings (id, host, port, connection, username, password, sender, recipients) VALUES (0, ?1, ?2, ?3, ?4, ?5, ?6, ?7);", -1, &stmt, NULL);
		utils::epilogue epi([stmt] { sqlite3_finalize(stmt); });

		sqlite3_bind_text(stmt, 1, data.emailSettings.host.c_str(), -1, SQLITE_STATIC);
		sqlite3_bind_int64(stmt, 2, data.emailSettings.port);
		sqlite3_bind_int(stmt, 3, (int)data.emailSettings.connection);
		sqlite3_bind_text(stmt, 4, data.emailSettings.username.c_str(), -1, SQLITE_STATIC);
		sqlite3_bind_text(stmt, 5, data.emailSettings.password.c_str(), -1, SQLITE_STATIC);
		sqlite3_bind_text(stmt, 6, data.emailSettings.sender.c_str(), -1, SQLITE_STATIC);
		std::string recipients = std::accumulate(data.emailSettings.recipients.begin(), data.emailSettings.recipients.end(), std::string(";"));
		sqlite3_bind_text(stmt, 7, recipients.c_str(), -1, SQLITE_TRANSIENT);
		if (sqlite3_step(stmt) != SQLITE_DONE)
		{
			s_logger.logCritical(QString("Failed to save email settings: %1").arg(sqlite3_errmsg(m_sqlite)));
			return;
		}
	}

	if (data.ftpSettingsChanged)
	{
		data.ftpSettingsChanged = false;
		sqlite3_stmt* stmt;
		sqlite3_prepare_v2(m_sqlite, "REPLACE INTO FtpSettings (id, host, port, username, password, folder, uploadBackups, uploadPeriod) VALUES (0, ?1, ?2, ?3, ?4, ?5, ?6, ?7);", -1, &stmt, NULL);
		utils::epilogue epi([stmt] { sqlite3_finalize(stmt); });

		sqlite3_bind_text(stmt, 1, data.ftpSettings.host.c_str(), -1, SQLITE_STATIC);
		sqlite3_bind_int64(stmt, 2, data.ftpSettings.port);
		sqlite3_bind_text(stmt, 3, data.ftpSettings.username.c_str(), -1, SQLITE_STATIC);
		sqlite3_bind_text(stmt, 4, data.ftpSettings.password.c_str(), -1, SQLITE_STATIC);
		sqlite3_bind_text(stmt, 5, data.ftpSettings.folder.c_str(), -1, SQLITE_STATIC);
		sqlite3_bind_int(stmt, 6, data.ftpSettings.uploadBackups ? 1 : 0);
		sqlite3_bind_int64(stmt, 7, std::chrono::duration_cast<std::chrono::seconds>(data.ftpSettings.uploadBackupsPeriod).count());
		if (sqlite3_step(stmt) != SQLITE_DONE)
		{
			s_logger.logCritical(QString("Failed to save ftp settings: %1").arg(sqlite3_errmsg(m_sqlite)));
			return;
		}
	}

	if (data.usersChanged || data.usersAddedOrRemoved)
	{
		if (data.usersAddedOrRemoved)
		{
			const char* sql = "DELETE FROM Users;";
			if (sqlite3_exec(m_sqlite, sql, NULL, NULL, nullptr))
			{
				s_logger.logCritical(QString("Failed to clear users: %1").arg(sqlite3_errmsg(m_sqlite)));
				return;
			}
		}
		sqlite3_stmt* stmt;
		sqlite3_prepare_v2(m_sqlite, "REPLACE INTO Users (id, name, passwordHash, permissions, type, lastLogin) VALUES(?1, ?2, ?3, ?4, ?5, ?6);", -1, &stmt, NULL);
		utils::epilogue epi([stmt] { sqlite3_finalize(stmt); });

		for (User const& user : data.users)
		{
			sqlite3_bind_int64(stmt, 1, user.id);
			sqlite3_bind_text(stmt, 2, user.descriptor.name.c_str(), -1, SQLITE_STATIC);
			sqlite3_bind_text(stmt, 3, user.descriptor.passwordHash.c_str(), -1, SQLITE_STATIC);
			sqlite3_bind_int64(stmt, 4, user.descriptor.permissions);
			sqlite3_bind_int(stmt, 5, (int)user.descriptor.type);
			sqlite3_bind_int64(stmt, 6, IClock::to_time_t(user.lastLogin));
			if (sqlite3_step(stmt) != SQLITE_DONE)
			{
				s_logger.logCritical(QString("Failed to save user: %1").arg(sqlite3_errmsg(m_sqlite)));
				return;
			}
			sqlite3_reset(stmt);
		}
		data.usersAddedOrRemoved = false;
		data.usersChanged = false;
	}
	if (data.baseStationsChanged || data.baseStationsAddedOrRemoved)
	{
		if (data.baseStationsAddedOrRemoved)
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
			sqlite3_bind_text(stmt, 3, utils::getMacStr(bs.descriptor.mac).c_str(), -1, SQLITE_TRANSIENT);
			if (sqlite3_step(stmt) != SQLITE_DONE)
			{
				s_logger.logCritical(QString("Failed to save base station: %1").arg(sqlite3_errmsg(m_sqlite)));
				return;
			}
			sqlite3_reset(stmt);
		}
		data.baseStationsAddedOrRemoved = false;
		data.baseStationsChanged = false;
	}
	if (data.sensorTimeConfigsChanged)
	{
		{
			const char* sql = "DELETE FROM SensorTimeConfigs;";
			if (sqlite3_exec(m_sqlite, sql, NULL, NULL, nullptr))
			{
				s_logger.logCritical(QString("Failed to clear sensor configs: %1").arg(sqlite3_errmsg(m_sqlite)));
				return;
			}
		}
		sqlite3_stmt* stmt = m_saveSensorTimeConfigsStmt.get();
		for (SensorTimeConfig const& sc : data.sensorTimeConfigs)
		{
			sqlite3_bind_int64(stmt, 1, IClock::to_time_t(sc.baselineMeasurementTimePoint));
			sqlite3_bind_int64(stmt, 2, sc.baselineMeasurementIndex);
			sqlite3_bind_int64(stmt, 3, std::chrono::duration_cast<std::chrono::seconds>(sc.descriptor.measurementPeriod).count());
			sqlite3_bind_int64(stmt, 4, std::chrono::duration_cast<std::chrono::seconds>(sc.descriptor.commsPeriod).count());
			if (sqlite3_step(stmt) != SQLITE_DONE)
			{
				s_logger.logCritical(QString("Failed to save sensor config: %1").arg(sqlite3_errmsg(m_sqlite)));
				return;
			}
			sqlite3_reset(stmt);
		}
		data.sensorTimeConfigsChanged = false;
	}
	if (data.sensorSettingsChanged)
	{
		data.sensorSettingsChanged = false;
		sqlite3_stmt* stmt = m_saveSensorSettingsStmt.get();

		sqlite3_bind_int64(stmt, 1, data.sensorSettings.radioPower);
		sqlite3_bind_int64(stmt, 2, data.sensorSettings.retries);
		sqlite3_bind_double(stmt, 3, data.sensorSettings.alertBatteryLevel);
		sqlite3_bind_double(stmt, 4, data.sensorSettings.alertSignalStrengthLevel);

		const char* xx = sqlite3_expanded_sql(stmt);

		if (sqlite3_step(stmt) != SQLITE_DONE)
		{
			s_logger.logCritical(QString("Failed to save sensor settings: %1").arg(sqlite3_errmsg(m_sqlite)));
			return;
		}
		sqlite3_reset(stmt);
	}
	if (data.sensorsChanged || data.sensorsAddedOrRemoved)
	{
		if (data.sensorsAddedOrRemoved)
		{
			const char* sql = "DELETE FROM Sensors;";
			if (sqlite3_exec(m_sqlite, sql, NULL, NULL, nullptr))
			{
				s_logger.logCritical(QString("Failed to clear sensors: %1").arg(sqlite3_errmsg(m_sqlite)));
				return;
			}
		}
		sqlite3_stmt* stmt = m_saveSensorsStmt.get();
		for (Sensor const& s : data.sensors)
		{
			int index = 1;
			sqlite3_bind_int64(stmt, index++, s.id);
			sqlite3_bind_text(stmt, index++, s.descriptor.name.c_str(), -1, SQLITE_STATIC);
			sqlite3_bind_int64(stmt, index++, s.address);
			sqlite3_bind_int64(stmt, index++, s.deviceInfo.sensorType);
			sqlite3_bind_int64(stmt, index++, s.deviceInfo.hardwareVersion);
			sqlite3_bind_int64(stmt, index++, s.deviceInfo.softwareVersion);

			sqlite3_bind_double(stmt, index++, s.calibration.temperatureBias);
			sqlite3_bind_double(stmt, index++, s.calibration.humidityBias);

			sqlite3_bind_int64(stmt, index++, s.serialNumber);
			sqlite3_bind_int64(stmt, index++, (int)s.state);
			sqlite3_bind_int(stmt, index++, s.shouldSleep ? 1 : 0);
			sqlite3_bind_int64(stmt, index++, IClock::to_time_t(s.sleepStateTimePoint));

			sqlite3_bind_int64(stmt, index++, s.stats.commsBlackouts);
			sqlite3_bind_int64(stmt, index++, s.stats.commsFailures);
			sqlite3_bind_int64(stmt, index++, s.stats.unknownReboots);
			sqlite3_bind_int64(stmt, index++, s.stats.powerOnReboots);
			sqlite3_bind_int64(stmt, index++, s.stats.resetReboots);
			sqlite3_bind_int64(stmt, index++, s.stats.brownoutReboots);
			sqlite3_bind_int64(stmt, index++, s.stats.watchdogReboots);
			sqlite3_bind_int64(stmt, index++, s.stats.commsRetries);
			sqlite3_bind_int64(stmt, index++, std::chrono::duration_cast<std::chrono::milliseconds>(s.stats.asleep).count());
			sqlite3_bind_int64(stmt, index++, std::chrono::duration_cast<std::chrono::milliseconds>(s.stats.awake).count());
			sqlite3_bind_int64(stmt, index++, s.stats.commsRounds);
			sqlite3_bind_int64(stmt, index++, s.stats.measurementRounds);

			sqlite3_bind_int64(stmt, index++, IClock::to_time_t(s.lastCommsTimePoint));

			sqlite3_bind_int64(stmt, index++, s.lastConfirmedMeasurementIndex);
			sqlite3_bind_int64(stmt, index++, s.lastAlarmProcessesMeasurementIndex);
			sqlite3_bind_int64(stmt, index++, s.firstStoredMeasurementIndex);
			sqlite3_bind_int64(stmt, index++, s.storedMeasurementCount);
			sqlite3_bind_int64(stmt, index++, s.estimatedStoredMeasurementCount);

			sqlite3_bind_int64(stmt, index++, s.lastSignalStrengthB2S);
			sqlite3_bind_int64(stmt, index++, s.averageSignalStrength.b2s);
			sqlite3_bind_int64(stmt, index++, s.averageSignalStrength.s2b);

			sqlite3_bind_int(stmt, index++, s.isRTMeasurementValid ? 1 : 0);
			sqlite3_bind_double(stmt, index++, s.rtMeasurementTemperature);
			sqlite3_bind_double(stmt, index++, s.rtMeasurementHumidity);
			sqlite3_bind_double(stmt, index++, s.rtMeasurementVcc);
			Q_ASSERT(index == 38);
			if (sqlite3_step(stmt) != SQLITE_DONE)
			{
				s_logger.logCritical(QString("Failed to save sensor: %1").arg(sqlite3_errmsg(m_sqlite)));
				return;
			}
			sqlite3_reset(stmt);
		}
		data.sensorsAddedOrRemoved = false;
		data.sensorsChanged = false;
	}
	if (data.alarmsChanged || data.alarmsAddedOrRemoved)
	{
		if (data.alarmsAddedOrRemoved)
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
			int index = 1;
			sqlite3_bind_int64(stmt, index++, a.id);
			sqlite3_bind_text(stmt, index++, a.descriptor.name.c_str(), -1, SQLITE_STATIC);
			sqlite3_bind_int(stmt, index++, a.descriptor.filterSensors ? 1 : 0);
			std::string sensors;
			for (SensorId id : a.descriptor.sensors)
			{
				sensors += std::to_string(id) + ";";
			}
			sqlite3_bind_text(stmt, index++, sensors.c_str(), -1, SQLITE_TRANSIENT);
			sqlite3_bind_int(stmt, index++, a.descriptor.lowTemperatureWatch ? 1 : 0);
			sqlite3_bind_double(stmt, index++, a.descriptor.lowTemperatureSoft);
			sqlite3_bind_double(stmt, index++, a.descriptor.lowTemperatureHard);
			sqlite3_bind_int(stmt, index++, a.descriptor.highTemperatureWatch ? 1 : 0);
			sqlite3_bind_double(stmt, index++, a.descriptor.highTemperatureSoft);
			sqlite3_bind_double(stmt, index++, a.descriptor.highTemperatureHard);
			sqlite3_bind_int(stmt, index++, a.descriptor.lowHumidityWatch ? 1 : 0);
			sqlite3_bind_double(stmt, index++, a.descriptor.lowHumiditySoft);
			sqlite3_bind_double(stmt, index++, a.descriptor.lowHumidityHard);
			sqlite3_bind_int(stmt, index++, a.descriptor.highHumidityWatch ? 1 : 0);
			sqlite3_bind_double(stmt, index++, a.descriptor.highHumiditySoft);
			sqlite3_bind_double(stmt, index++, a.descriptor.highHumidityHard);
			sqlite3_bind_int(stmt, index++, a.descriptor.lowVccWatch ? 1 : 0);
			sqlite3_bind_int(stmt, index++, a.descriptor.lowSignalWatch ? 1 : 0);
			sqlite3_bind_int(stmt, index++, a.descriptor.sensorBlackoutWatch ? 1 : 0);
			sqlite3_bind_int(stmt, index++, a.descriptor.baseStationDisconnectedWatch ? 1 : 0);
			sqlite3_bind_int(stmt, index++, a.descriptor.sendEmailAction ? 1 : 0);
			sqlite3_bind_int64(stmt, index++, std::chrono::duration_cast<std::chrono::seconds>(a.descriptor.resendPeriod).count());
			{
				std::string triggers;
				for (auto p : a.triggersPerSensor)
				{
					triggers += std::to_string(p.first) + "/" + std::to_string(p.second) + ";";
				}
				sqlite3_bind_text(stmt, index++, triggers.c_str(), -1, SQLITE_TRANSIENT);
			}
			{
				std::string triggers;
				for (auto p : a.triggersPerBaseStation)
				{
					triggers += std::to_string(p.first) + "/" + std::to_string(p.second) + ";";
				}
				sqlite3_bind_text(stmt, index++, triggers.c_str(), -1, SQLITE_TRANSIENT);
			}
			sqlite3_bind_int64(stmt, index++, IClock::to_time_t(a.lastTriggeredTimePoint));
			if (sqlite3_step(stmt) != SQLITE_DONE)
			{
				s_logger.logCritical(QString("Failed to save alarms: %1").arg(sqlite3_errmsg(m_sqlite)));
				return;
			}
			Q_ASSERT(index == 26);
			sqlite3_reset(stmt);
		}
		data.alarmsAddedOrRemoved = false;
		data.alarmsChanged = false;
	}
	if (data.reportsChanged || data.reportsAddedOrRemoved)
	{
		if (data.reportsAddedOrRemoved)
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
			sqlite3_bind_int64(stmt, 5, r.descriptor.filterSensors ? 1 : 0);
			std::string sensors;
			for (SensorId id : r.descriptor.sensors)
			{
				sensors += std::to_string(id) + ";";
			}
			sqlite3_bind_text(stmt, 6, sensors.c_str(), -1, SQLITE_TRANSIENT);
			sqlite3_bind_int64(stmt, 7, IClock::to_time_t(r.lastTriggeredTimePoint));
			if (sqlite3_step(stmt) != SQLITE_DONE)
			{
				s_logger.logCritical(QString("Failed to save report: %1").arg(sqlite3_errmsg(m_sqlite)));
				return;
			}
			sqlite3_reset(stmt);
		}
		data.reportsAddedOrRemoved = false;
		data.reportsChanged = false;
	}

	//    std::cout << QString("Done saving DB. Time: %3s\n").arg(std::chrono::duration<float>(m_clock->now() - start).count()).toUtf8().data();
	//    std::cout.flush();
}

//////////////////////////////////////////////////////////////////////////

void DB::close()
{
	std::lock_guard<std::recursive_mutex> lg(m_dataMutex);
	if (!m_sqlite)
	{
		return;
	}

	addAsyncMeasurements();
	if (m_saveScheduled)
	{
		save(true);
	}

	m_saveBaseStationsStmt = nullptr;
	m_saveSensorTimeConfigsStmt = nullptr;
	m_saveSensorSettingsStmt = nullptr;
	m_saveSensorsStmt = nullptr;
	m_saveAlarmsStmt = nullptr;
	m_saveReportsStmt = nullptr;
	m_addMeasurementsStmt = nullptr;
	m_sqlite = nullptr;

	m_data = Data();
}

//////////////////////////////////////////////////////////////////////////

sqlite3* DB::getSqliteDB()
{
	return m_sqlite;
}

//////////////////////////////////////////////////////////////////////////

void DB::checkRepetitiveAlarms()
{
	std::lock_guard<std::recursive_mutex> lg(m_dataMutex);

	bool triggerSave = false;
	for (Alarm& alarm : m_data.alarms)
	{
        if ((!alarm.triggersPerSensor.empty() || !alarm.triggersPerBaseStation.empty()) && 
			(m_clock->now() - alarm.lastTriggeredTimePoint >= alarm.descriptor.resendPeriod))
        {
            emit alarmStillTriggered(alarm.id);
            alarm.lastTriggeredTimePoint = m_clock->now();
			m_data.alarmsChanged = true;
			triggerSave = true;
        }
	}
	if (triggerSave)
	{
		save(true);
	}
}

//////////////////////////////////////////////////////////////////////////

void DB::checkReports()
{
	std::lock_guard<std::recursive_mutex> lg(m_dataMutex);

	bool triggerSave = false;

	for (Report& report: m_data.reports)
	{
		if (isReportTriggered(report))
		{
			emit reportTriggered(report.id, report.lastTriggeredTimePoint, m_clock->now());
			report.lastTriggeredTimePoint = m_clock->now();
			m_data.reportsChanged = true;
			triggerSave = true;
		}
	}

	if (triggerSave)
	{
		save(true);
	}
}

//////////////////////////////////////////////////////////////////////////

void DB::checkForDisconnectedBaseStations()
{
	std::lock_guard<std::recursive_mutex> lg(m_dataMutex);

	for (BaseStation& bs: m_data.baseStations)
	{
		if (!bs.isConnected && m_clock->now() - bs.lastConnectedTimePoint > std::chrono::minutes(1))
		{
			computeBaseStationAlarmTriggers(bs);
		}
	}
}

//////////////////////////////////////////////////////////////////////////

void DB::checkForBlackoutSensors()
{
	std::lock_guard<std::recursive_mutex> lg(m_dataMutex);

	bool saveSensors = false;
	SensorTimeConfig timeConfig = getLastSensorTimeConfig();
	for (Sensor& s : m_data.sensors)
	{
		bool wasBlackout = s.blackout;
		if (s.state == Sensor::State::Active && m_clock->now() - s.lastCommsTimePoint > timeConfig.descriptor.commsPeriod * 1.5f)
		{
			s.blackout = true;
		}
		else
		{
			s.blackout = false;
		}

		//only send emails 2 rounds after being added to avoid slamming every time the program is started
		if (m_clock->now() - s.addedTimePoint > timeConfig.descriptor.commsPeriod * 2)
		{
			if (!wasBlackout && s.blackout)
			{
				s.stats.commsBlackouts++;
				saveSensors = true;
			}
			computeSensorAlarmTriggers(s, std::nullopt);
		}
	}

	if (saveSensors)
	{
		scheduleSave();
	}
}

//////////////////////////////////////////////////////////////////////////

void DB::checkMeasurementTriggers()
{
	std::lock_guard<std::recursive_mutex> lg(m_dataMutex);

	if (m_data.sensors.empty() || m_data.alarms.empty())
	{
		Q_ASSERT(false);
		return;
	}

	bool dataChanged = false;
	std::vector<Measurement> measurements;
	//gathered changed triggers
	for (Sensor& sensor : m_data.sensors)
	{
		if (sensor.lastConfirmedMeasurementIndex <= sensor.lastAlarmProcessesMeasurementIndex)
		{
			//nothing new happened
			continue;
		}

		QString sql = QString("SELECT id, timePoint, receivedTimePoint, idx, sensorId, temperature, humidity, vcc, signalStrengthS2B, signalStrengthB2S, sensorErrors, alarmTriggersCurrent, alarmTriggersAdded, alarmTriggersRemoved "
								"FROM Measurements "
								"WHERE idx > %1 AND idx <= %2 AND sensorId = %3;").arg(sensor.lastAlarmProcessesMeasurementIndex).arg(sensor.lastConfirmedMeasurementIndex).arg(sensor.id);
		sqlite3_stmt* stmt;
		if (sqlite3_prepare_v2(m_sqlite, sql.toUtf8().data(), -1, &stmt, 0) != SQLITE_OK)
		{
			const char* msg = sqlite3_errmsg(m_sqlite);
			Q_ASSERT(false);
			continue;
		}
		utils::epilogue epi([stmt] { sqlite3_finalize(stmt); });

		while (sqlite3_step(stmt) == SQLITE_ROW)
		{
			Measurement m = unpackMeasurement(stmt);
			m.alarmTriggers = computeSensorAlarmTriggers(sensor, m);
			measurements.push_back(m);
		}
		sensor.lastAlarmProcessesMeasurementIndex = sensor.lastConfirmedMeasurementIndex;
		dataChanged = true;
	}

	if (!measurements.empty() || dataChanged)
	{
		sqlite3_exec(m_sqlite, "BEGIN TRANSACTION;", NULL, NULL, NULL);
		utils::epilogue epi([this] { sqlite3_exec(m_sqlite, "END TRANSACTION;", NULL, NULL, NULL); });

		for (Measurement const& m : measurements)
		{
			QString sql = QString("UPDATE Measurements "
								  "SET alarmTriggersCurrent = ?1, alarmTriggersAdded = ?2, alarmTriggersRemoved = ?3 "
								  "WHERE id = ?4;");
			sqlite3_stmt* stmt;
			if (sqlite3_prepare_v2(m_sqlite, sql.toUtf8().data(), -1, &stmt, 0) != SQLITE_OK)
			{
				const char* msg = sqlite3_errmsg(m_sqlite);
				Q_ASSERT(false);
				continue;
			}
			utils::epilogue epi([stmt] { sqlite3_finalize(stmt); });

			sqlite3_bind_double(stmt, 1, m.alarmTriggers.current);
			sqlite3_bind_double(stmt, 2, m.alarmTriggers.added);
			sqlite3_bind_double(stmt, 3, m.alarmTriggers.removed);
			sqlite3_bind_int64(stmt, 4, m.id);
			if (sqlite3_step(stmt) != SQLITE_DONE)
			{
				const char* msg = sqlite3_errmsg(m_sqlite);
				Q_ASSERT(false);
				continue;
			}
		}

		m_data.sensorsChanged = true;
		save(false);
	}

	if (!measurements.empty())
	{
		emit measurementsChanged();
	}
}

//////////////////////////////////////////////////////////////////////////

void DB::process()
{
	addAsyncMeasurements();
	if (m_saveScheduled)
    {
		save(true);
    }

    checkRepetitiveAlarms();
	checkReports();
	checkForDisconnectedBaseStations();
	checkForBlackoutSensors();
	checkMeasurementTriggers();
}

//////////////////////////////////////////////////////////////////////////

bool DB::setGeneralSettings(GeneralSettings const& settings)
{
	std::lock_guard<std::recursive_mutex> lg(m_dataMutex);
	m_data.generalSettings = settings;
	m_data.generalSettingsChanged = true;
	emit generalSettingsChanged();

	s_logger.logInfo("Changed general settings");

	save(true);

	return true;
}

//////////////////////////////////////////////////////////////////////////

DB::GeneralSettings const& DB::getGeneralSettings() const
{
	std::lock_guard<std::recursive_mutex> lg(m_dataMutex);
	return m_data.generalSettings;
}

//////////////////////////////////////////////////////////////////////////

bool DB::setCsvSettings(CsvSettings const& settings)
{
	std::lock_guard<std::recursive_mutex> lg(m_dataMutex);
	m_data.csvSettings = settings;
	m_data.csvSettingsChanged = true;
	emit csvSettingsChanged();

	s_logger.logInfo("Changed csv settings");

	save(true);

	return true;
}

//////////////////////////////////////////////////////////////////////////

DB::CsvSettings const& DB::getCsvSettings() const
{
	std::lock_guard<std::recursive_mutex> lg(m_dataMutex);
	return m_data.csvSettings;
}

//////////////////////////////////////////////////////////////////////////

bool DB::setEmailSettings(EmailSettings const& settings)
{
	if (settings.sender.empty())
	{
		return false;
	}
	if (settings.username.empty())
	{
		return false;
	}
	if (settings.password.empty())
	{
		return false;
	}
	if (settings.host.empty())
	{
		return false;
	}
	if (settings.port == 0)
	{
		return false;
	}

	std::lock_guard<std::recursive_mutex> lg(m_dataMutex);
	m_data.emailSettings = settings;
	m_data.emailSettingsChanged = true;
	emit emailSettingsChanged();

	s_logger.logInfo("Changed email settings");

	save(true);

	return true;
}

//////////////////////////////////////////////////////////////////////////

DB::EmailSettings const& DB::getEmailSettings() const
{
	std::lock_guard<std::recursive_mutex> lg(m_dataMutex);
	return m_data.emailSettings;
}

//////////////////////////////////////////////////////////////////////////

bool DB::setFtpSettings(FtpSettings const& settings)
{
	if (settings.username.empty())
	{
		return false;
	}
	if (settings.password.empty())
	{
		return false;
	}
	if (settings.host.empty())
	{
		return false;
	}
	if (settings.port == 0)
	{
		return false;
	}

	std::lock_guard<std::recursive_mutex> lg(m_dataMutex);
	m_data.ftpSettings = settings;
	m_data.ftpSettingsChanged = true;
	emit ftpSettingsChanged();

	s_logger.logInfo("Changed ftp settings");

	save(true);

	return true;
}

//////////////////////////////////////////////////////////////////////////

DB::FtpSettings const& DB::getFtpSettings() const
{
	std::lock_guard<std::recursive_mutex> lg(m_dataMutex);
	return m_data.ftpSettings;
}

//////////////////////////////////////////////////////////////////////////

size_t DB::getUserCount() const
{
	std::lock_guard<std::recursive_mutex> lg(m_dataMutex);
	return m_data.users.size();
}

//////////////////////////////////////////////////////////////////////////

DB::User DB::getUser(size_t index) const
{
	std::lock_guard<std::recursive_mutex> lg(m_dataMutex);
	assert(index < m_data.users.size());
	return m_data.users[index];
}

//////////////////////////////////////////////////////////////////////////

bool DB::addUser(UserDescriptor const& descriptor)
{
	std::lock_guard<std::recursive_mutex> lg(m_dataMutex);
	if (findUserIndexByName(descriptor.name) >= 0)
	{
		return false;
	}

	User user;
	user.descriptor = descriptor;
	user.id = ++m_data.lastUserId;

	m_data.users.push_back(user);
	m_data.usersAddedOrRemoved = true;
	emit userAdded(user.id);

	s_logger.logInfo(QString("Added user '%1'").arg(descriptor.name.c_str()));

	save(true);

	return true;
}

//////////////////////////////////////////////////////////////////////////

Result<void> DB::setUser(UserId id, UserDescriptor const& descriptor)
{
	std::lock_guard<std::recursive_mutex> lg(m_dataMutex);
	int32_t index = findUserIndexByName(descriptor.name);
	if (index >= 0 && getUser(static_cast<size_t>(index)).id != id)
	{
		return Error(QString("Cannot find user '%1'").arg(descriptor.name.c_str()).toUtf8().data());
	}

	index = findUserIndexById(id);
	if (index < 0)
	{
		return Error(QString("Cannot find user '%1' (internal inconsistency)").arg(descriptor.name.c_str()).toUtf8().data());
	}

	User& user = m_data.users[static_cast<size_t>(index)];

	int32_t loggedInUserIndex = findUserIndexById(m_data.loggedInUserId);
	if (loggedInUserIndex >= 0)
	{
		const User& loggedInUser = m_data.users[static_cast<size_t>(loggedInUserIndex)];
		if (loggedInUser.descriptor.type != UserDescriptor::Type::Admin)
		{
			if (user.descriptor.permissions != descriptor.permissions &&
				(loggedInUser.descriptor.permissions & UserDescriptor::PermissionChangeUsers) == 0)
			{
				return Error(QString("No permission to change permissions").toUtf8().data());
			}

			//check that the user is not adding too many permissions
			uint32_t diff = loggedInUser.descriptor.permissions ^ descriptor.permissions;
			if ((diff & loggedInUser.descriptor.permissions) != diff)
			{
				return Error(QString("Permission escalation between users is not allowed").toUtf8().data());
			}

			if (loggedInUser.descriptor.type != UserDescriptor::Type::Admin && user.descriptor.type == UserDescriptor::Type::Admin)
			{
				return Error(QString("Cannot change admin user").toUtf8().data());
			}
		}
	}

	user.descriptor = descriptor;
	m_data.usersChanged = true;
	emit userChanged(id);

	s_logger.logInfo(QString("Changed user '%1'").arg(descriptor.name.c_str()));

	save(true);

	return success;
}

//////////////////////////////////////////////////////////////////////////

void DB::removeUser(size_t index)
{
	std::lock_guard<std::recursive_mutex> lg(m_dataMutex);
	assert(index < m_data.users.size());
	UserId id = m_data.users[index].id;

	s_logger.logInfo(QString("Removed user '%1'").arg(m_data.users[index].descriptor.name.c_str()));

	m_data.users.erase(m_data.users.begin() + index);
	m_data.usersAddedOrRemoved = true;
	emit userRemoved(id);

	save(true);
}

//////////////////////////////////////////////////////////////////////////

int32_t DB::findUserIndexByName(std::string const& name) const
{
	std::lock_guard<std::recursive_mutex> lg(m_dataMutex);
	auto it = std::find_if(m_data.users.begin(), m_data.users.end(), [&name](User const& user)
	{
		return strcasecmp(user.descriptor.name.c_str(), name.c_str()) == 0;
	});

	if (it == m_data.users.end())
	{
		return -1;
	}
	return int32_t(std::distance(m_data.users.begin(), it));
}

//////////////////////////////////////////////////////////////////////////

int32_t DB::findUserIndexById(UserId id) const
{
	std::lock_guard<std::recursive_mutex> lg(m_dataMutex);
	auto it = std::find_if(m_data.users.begin(), m_data.users.end(), [id](User const& user) { return user.id == id; });
	if (it == m_data.users.end())
	{
		return -1;
	}
	return int32_t(std::distance(m_data.users.begin(), it));
}

//////////////////////////////////////////////////////////////////////////

int32_t DB::findUserIndexByPasswordHash(std::string const& passwordHash) const
{
	std::lock_guard<std::recursive_mutex> lg(m_dataMutex);
	auto it = std::find_if(m_data.users.begin(), m_data.users.end(), [&passwordHash](User const& user) { return user.descriptor.passwordHash == passwordHash; });
	if (it == m_data.users.end())
	{
		return -1;
	}
	return int32_t(std::distance(m_data.users.begin(), it));
}

//////////////////////////////////////////////////////////////////////////

bool DB::needsAdmin() const
{
	std::lock_guard<std::recursive_mutex> lg(m_dataMutex);
	auto it = std::find_if(m_data.users.begin(), m_data.users.end(), [](User const& user) { return user.descriptor.type == UserDescriptor::Type::Admin; });
	if (it == m_data.users.end())
	{
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////

void DB::setLoggedInUserId(UserId id)
{
	std::lock_guard<std::recursive_mutex> lg(m_dataMutex);
	int32_t _index = findUserIndexById(id);
	if (_index >= 0)
	{
		size_t index = static_cast<size_t>(_index);
		s_logger.logInfo(QString("User '%1' logged in").arg(m_data.users[index].descriptor.name.c_str()));

		//         for (size_t i = 0; i < 10000; i++)
		// 		{
		// 			s_logger.logInfo(QString("User '%1' logged in").arg(m_mainData.users[index].descriptor.name.c_str()));
		// 		}

		m_data.loggedInUserId = id;
		emit userLoggedIn(id);
	}
}

//////////////////////////////////////////////////////////////////////////

DB::UserId DB::getLoggedInUserId() const
{
	std::lock_guard<std::recursive_mutex> lg(m_dataMutex);
	return m_data.loggedInUserId;
}

//////////////////////////////////////////////////////////////////////////

std::optional<DB::User> DB::getLoggedInUser() const
{
	std::lock_guard<std::recursive_mutex> lg(m_dataMutex);
	int32_t _index = findUserIndexById(m_data.loggedInUserId);
	if (_index >= 0)
	{
		size_t index = static_cast<size_t>(_index);
		return m_data.users[index];
	}
	return std::nullopt;
}

//////////////////////////////////////////////////////////////////////////

bool DB::isLoggedInAsAdmin() const
{
	std::lock_guard<std::recursive_mutex> lg(m_dataMutex);
	int32_t _index = findUserIndexById(m_data.loggedInUserId);
	if (_index >= 0)
	{
		size_t index = static_cast<size_t>(_index);
		return m_data.users[index].descriptor.type == UserDescriptor::Type::Admin;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////

Result<void> DB::setSensorSettings(SensorSettings const& settings)
{
	std::lock_guard<std::recursive_mutex> lg(m_dataMutex);
	if (settings.radioPower < -3 || settings.radioPower > 20)
	{
		return Error("Invalid radio power value");
	}
	if (settings.retries < 1 || settings.retries > 10)
	{
		return Error("Invalid number of retries");
	}
	if (settings.alertBatteryLevel < 0.f || settings.alertBatteryLevel >= 1.f)
	{
		return Error("Invalid alert battery level");
	}
	if (settings.alertSignalStrengthLevel < 0.f || settings.alertSignalStrengthLevel >= 1.f)
	{
		return Error("Invalid alert signal strength level");
	}

	m_data.sensorSettings = settings;
	m_data.sensorSettingsChanged = true;
	emit sensorSettingsChanged();

	s_logger.logInfo("Changed sensor settings");

	save(true);

	return success;
}

//////////////////////////////////////////////////////////////////////////

DB::SensorSettings DB::getSensorSettings() const
{
	std::lock_guard<std::recursive_mutex> lg(m_dataMutex);
	return m_data.sensorSettings;
}

///////////////////////////////////////////////////////////////////////////////////////////

IClock::duration DB::computeActualCommsPeriod(SensorTimeConfigDescriptor const& config) const
{
    std::lock_guard<std::recursive_mutex> lg(m_dataMutex);

	IClock::duration max_period = m_data.sensors.size() * COMMS_DURATION;
	IClock::duration period = std::max(config.commsPeriod, max_period);
    return std::max(period, config.measurementPeriod + MEASUREMENT_JITTER);
}

///////////////////////////////////////////////////////////////////////////////////////////

IClock::time_point DB::computeNextMeasurementTimePoint(Sensor const& sensor) const
{
    uint32_t nextMeasurementIndex = computeNextMeasurementIndex(sensor);
    SensorTimeConfig config = findSensorTimeConfigForMeasurementIndex(nextMeasurementIndex);

    uint32_t index = nextMeasurementIndex >= config.baselineMeasurementIndex ? nextMeasurementIndex - config.baselineMeasurementIndex : 0;
	IClock::time_point tp = config.baselineMeasurementTimePoint + config.descriptor.measurementPeriod * index;
    return tp;
}

///////////////////////////////////////////////////////////////////////////////////////////

IClock::time_point DB::computeNextCommsTimePoint(Sensor const& /*sensor*/, size_t sensorIndex) const
{
    SensorTimeConfig config = getLastSensorTimeConfig();
	IClock::duration period = computeActualCommsPeriod(config.descriptor);

	IClock::time_point now = m_clock->now();
    uint32_t index = static_cast<uint32_t>(std::ceil(((now - config.baselineMeasurementTimePoint) / period))) + 1;

	IClock::time_point start = config.baselineMeasurementTimePoint + period * index;
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

DB::BaseStation DB::getBaseStation(size_t index) const
{
    std::lock_guard<std::recursive_mutex> lg(m_dataMutex);

    Q_ASSERT(index < m_data.baseStations.size());
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

    s_logger.logInfo(QString("Adding base station '%1' / %2").arg(descriptor.name.c_str()).arg(utils::getMacStr(descriptor.mac).c_str()));

    BaseStation baseStation;
	baseStation.lastConnectedTimePoint = m_clock->now();
    baseStation.descriptor = descriptor;
    baseStation.id = ++m_data.lastBaseStationId;

    m_data.baseStations.push_back(baseStation);
	m_data.baseStationsAddedOrRemoved = true;
    emit baseStationAdded(baseStation.id);

	save(true);

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

    s_logger.logInfo(QString("Changing base station '%1' / %2").arg(descriptor.name.c_str()).arg(utils::getMacStr(descriptor.mac).c_str()));

    m_data.baseStations[index].descriptor = descriptor;
	m_data.baseStationsChanged = true;
    emit baseStationChanged(id);

	save(true);

    return true;
}

//////////////////////////////////////////////////////////////////////////

void DB::setBaseStationConnected(BaseStationId id, bool connected)
{
	std::lock_guard<std::recursive_mutex> lg(m_dataMutex);

	int32_t _index = findBaseStationIndexById(id);
	if (_index < 0)
	{
		Q_ASSERT(false);
		return;
	}

	size_t index = static_cast<size_t>(_index);

	BaseStation& bs = m_data.baseStations[index];
	if (bs.isConnected == connected)
	{
		return;
	}

	bs.isConnected = connected;
	bs.lastConnectedTimePoint = m_clock->now();
	if (connected)
	{
		computeBaseStationAlarmTriggers(bs);
	}

	m_data.baseStationsChanged = true;
	emit baseStationChanged(id);

	save(true);
}

//////////////////////////////////////////////////////////////////////////

void DB::removeBaseStation(size_t index)
{
    std::lock_guard<std::recursive_mutex> lg(m_dataMutex);

    Q_ASSERT(index < m_data.baseStations.size());
    BaseStationId id = m_data.baseStations[index].id;

    BaseStationDescriptor const& descriptor = m_data.baseStations[index].descriptor;
    s_logger.logInfo(QString("Removing base station '%1' / %2").arg(descriptor.name.c_str()).arg(utils::getMacStr(descriptor.mac).c_str()));

    m_data.baseStations.erase(m_data.baseStations.begin() + index);
	m_data.baseStationsAddedOrRemoved = true;
    emit baseStationRemoved(id);

	save(true);
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

DB::Sensor DB::getSensor(size_t index) const
{
    std::lock_guard<std::recursive_mutex> lg(m_dataMutex);
    Q_ASSERT(index < m_data.sensors.size());
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
    SensorTimeConfig config = getLastSensorTimeConfig();

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
    SensorTimeConfig config = getLastSensorTimeConfig();
	IClock::time_point now = m_clock->now();
    uint32_t index = static_cast<uint32_t>(std::ceil((now - config.baselineMeasurementTimePoint) / config.descriptor.measurementPeriod)) + config.baselineMeasurementIndex;
    return index;
}

//////////////////////////////////////////////////////////////////////////

Result<void> DB::_addSensorTimeConfig(SensorTimeConfigDescriptor const& descriptor)
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

	SensorTimeConfig config;
	config.descriptor = descriptor;

	if (!m_data.sensorTimeConfigs.empty())
	{
		uint32_t nextRealTimeMeasurementIndex = computeNextRealTimeMeasurementIndex();

		SensorTimeConfig c = findSensorTimeConfigForMeasurementIndex(nextRealTimeMeasurementIndex);
		uint32_t index = nextRealTimeMeasurementIndex >= c.baselineMeasurementIndex ? nextRealTimeMeasurementIndex - c.baselineMeasurementIndex : 0;
		IClock::time_point nextMeasurementTP = c.baselineMeasurementTimePoint + c.descriptor.measurementPeriod * index;

		m_data.sensorTimeConfigs.erase(std::remove_if(m_data.sensorTimeConfigs.begin(), m_data.sensorTimeConfigs.end(), [&nextRealTimeMeasurementIndex](SensorTimeConfig const& c)
		{
			return c.baselineMeasurementIndex == nextRealTimeMeasurementIndex;
		}), m_data.sensorTimeConfigs.end());
		config.baselineMeasurementIndex = nextRealTimeMeasurementIndex;
		config.baselineMeasurementTimePoint = nextMeasurementTP;
	}
	else
	{
		config.baselineMeasurementIndex = 0;
		config.baselineMeasurementTimePoint = m_clock->now();
	}

	m_data.sensorTimeConfigs.push_back(config);
	while (m_data.sensorTimeConfigs.size() > 100)
	{
		m_data.sensorTimeConfigs.erase(m_data.sensorTimeConfigs.begin());
	}
	m_data.sensorTimeConfigs.back().computedCommsPeriod = computeActualCommsPeriod(m_data.sensorTimeConfigs.back().descriptor); //make sure the computed commd config is up-to-date
	m_data.sensorTimeConfigsChanged = true;

	emit sensorTimeConfigAdded();

	s_logger.logInfo("Added sensors config");

	return success;
}

//////////////////////////////////////////////////////////////////////////

Result<void> DB::addSensorTimeConfig(SensorTimeConfigDescriptor const& descriptor)
{
	std::lock_guard<std::recursive_mutex> lg(m_dataMutex);

	Result<void> result = _addSensorTimeConfig(descriptor);
	if (result != success)
	{
		return result;
	}
	save(true);
    return success;
}

//////////////////////////////////////////////////////////////////////////

Result<void> DB::setSensorTimeConfigs(std::vector<SensorTimeConfig> const& configs)
{
    std::lock_guard<std::recursive_mutex> lg(m_dataMutex);

	m_data.sensorTimeConfigs = configs;
	for (SensorTimeConfig& config : m_data.sensorTimeConfigs)
    {
        config.computedCommsPeriod = config.descriptor.commsPeriod;
    }
    if (!m_data.sensorTimeConfigs.empty())
    {
		SensorTimeConfig& config = m_data.sensorTimeConfigs.back();
        config.computedCommsPeriod = computeActualCommsPeriod(config.descriptor);
    }

	m_data.sensorTimeConfigsChanged = true;
	emit sensorTimeConfigChanged();

    s_logger.logInfo("Changed sensors configs");

	save(true);

    return success;
}

//////////////////////////////////////////////////////////////////////////

DB::SensorTimeConfig DB::getLastSensorTimeConfig() const
{
    std::lock_guard<std::recursive_mutex> lg(m_dataMutex);

    static SensorTimeConfig s_empty;
	return m_data.sensorTimeConfigs.empty() ? s_empty : m_data.sensorTimeConfigs.back();
}

//////////////////////////////////////////////////////////////////////////

size_t DB::getSensorTimeConfigCount() const
{
    std::lock_guard<std::recursive_mutex> lg(m_dataMutex);
	return m_data.sensorTimeConfigs.size();
}

//////////////////////////////////////////////////////////////////////////

DB::SensorTimeConfig DB::getSensorTimeConfig(size_t index) const
{
    std::lock_guard<std::recursive_mutex> lg(m_dataMutex);
	Q_ASSERT(index < m_data.sensorTimeConfigs.size());
    return m_data.sensorTimeConfigs[index];
}

//////////////////////////////////////////////////////////////////////////

DB::SensorTimeConfig DB::findSensorTimeConfigForMeasurementIndex(uint32_t index) const
{
    std::lock_guard<std::recursive_mutex> lg(m_dataMutex);

	for (auto it = m_data.sensorTimeConfigs.rbegin(); it != m_data.sensorTimeConfigs.rend(); ++it)
    {
		SensorTimeConfig const& c = *it;
        if (index >= c.baselineMeasurementIndex)
        {
            return c;
        }
    }
    return getLastSensorTimeConfig();
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
		sensor.addedTimePoint = m_clock->now();
        sensor.descriptor.name = descriptor.name;
        sensor.id = ++m_data.lastSensorId;
        sensor.state = Sensor::State::Unbound;

        m_data.sensors.push_back(sensor);
		m_data.sensorsAddedOrRemoved = true;
        emit sensorAdded(sensor.id);

        s_logger.logInfo(QString("Added sensor '%1'").arg(descriptor.name.c_str()));
    }

    //refresh computed comms period as new sensors are added
    if (!m_data.sensorTimeConfigs.empty())
    {
		SensorTimeConfig& config = m_data.sensorTimeConfigs.back();
        config.computedCommsPeriod = computeActualCommsPeriod(config.descriptor);
		m_data.sensorTimeConfigsChanged = true;
        emit sensorTimeConfigChanged();
    }

	save(true);

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
	m_data.sensorsChanged = true;
    emit sensorChanged(id);

    s_logger.logInfo(QString("Changed sensor '%1'").arg(descriptor.name.c_str()));

	save(true);

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

	m_data.sensorsChanged = true;

    emit sensorBound(sensor.id);
    emit sensorChanged(sensor.id);

    s_logger.logInfo(QString("Sensor '%1' bound to address %2").arg(sensor.descriptor.name.c_str()).arg(sensor.address));

    //refresh computed comms period as new sensors are added
    if (!m_data.sensorTimeConfigs.empty())
    {
		SensorTimeConfig& config = m_data.sensorTimeConfigs.back();
        config.computedCommsPeriod = computeActualCommsPeriod(config.descriptor);
		m_data.sensorTimeConfigsChanged = true;
        emit sensorTimeConfigChanged();
    }

	save(true);

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
	m_data.sensorsChanged = true;

    emit sensorChanged(sensor.id);

    s_logger.logInfo(QString("Sensor '%1' calibration changed").arg(sensor.descriptor.name.c_str()));

	save(true);

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

	DB::SensorTimeConfig config = getLastSensorTimeConfig();
    bool hasMeasurements = sensor.estimatedStoredMeasurementCount > 0;
    bool commedRecently = (DB::m_clock->now() - sensor.lastCommsTimePoint) < config.descriptor.commsPeriod * 2;
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
	m_data.sensorsChanged = true;

    emit sensorChanged(sensor.id);

    s_logger.logInfo(QString("Sensor '%1' sleep state changed to %2").arg(sensor.descriptor.name.c_str()).arg(sleep));

	save(true);

    return success;
}

//////////////////////////////////////////////////////////////////////////

Result<void> DB::clearSensorStats(SensorId id)
{
	std::lock_guard<std::recursive_mutex> lg(m_dataMutex);

	int32_t _index = findSensorIndexById(id);
	if (_index < 0)
	{
		return Error("Invalid sensor id");
	}
	size_t index = static_cast<size_t>(_index);
	Sensor& sensor = m_data.sensors[index];
    sensor.stats = SensorStats();
	m_data.sensorsChanged = true;

	emit sensorChanged(sensor.id);

	s_logger.logInfo(QString("Sensor '%1' error counters cleared").arg(sensor.descriptor.name.c_str()));

	save(true);

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
                    sensor.sleepStateTimePoint = m_clock->now();
                }
                sensor.state = d.sleeping ? Sensor::State::Sleeping : Sensor::State::Active;
            }
        }

        if (d.hasSignalStrength)
        {
            sensor.lastSignalStrengthB2S = d.signalStrengthB2S;

            //we do this here just in case the sensor doesn't have any measurements. In that case the average signal strength will be calculated from the lastSignalStrengthB2S
            //this happens the first time a sensor is added
            if (sensor.averageSignalStrength.b2s >= 0 || sensor.averageSignalStrength.s2b >= 0)
			{
                sensor.averageSignalStrength = { d.signalStrengthB2S, d.signalStrengthB2S };
			}
        }

        if (d.hasStatsDelta)
        {
            sensor.stats.commsBlackouts += d.statsDelta.commsBlackouts;
            sensor.stats.commsFailures += d.statsDelta.commsFailures;
            sensor.stats.resetReboots += d.statsDelta.resetReboots;
            sensor.stats.unknownReboots += d.statsDelta.unknownReboots;
            sensor.stats.brownoutReboots += d.statsDelta.brownoutReboots;
            sensor.stats.powerOnReboots += d.statsDelta.powerOnReboots;
            sensor.stats.watchdogReboots += d.statsDelta.watchdogReboots;
            sensor.stats.commsRetries += d.statsDelta.commsRetries;
            sensor.stats.awake += d.statsDelta.awake;
            sensor.stats.asleep += d.statsDelta.asleep;
            sensor.stats.commsRounds += d.statsDelta.commsRounds;
            sensor.stats.measurementRounds += d.statsDelta.measurementRounds;
        }

        if (d.hasMeasurement)
        {
            sensor.isRTMeasurementValid = true;
            sensor.rtMeasurementTemperature = d.measurementTemperature;
            sensor.rtMeasurementHumidity = d.measurementHumidity;
            sensor.rtMeasurementVcc = d.measurementVcc;
        }

        emit sensorDataChanged(sensor.id);
    }
	m_data.sensorsChanged = true;

    scheduleSave();

    return ok;
}

//////////////////////////////////////////////////////////////////////////

void DB::removeSensor(size_t index)
{
    std::lock_guard<std::recursive_mutex> lg(m_dataMutex);

    Q_ASSERT(index < m_data.sensors.size());

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
        //alarm.triggersPerSensor.erase(sensorId); //this will dissapear naturally
    }
	m_data.alarmsChanged = true;

    for (Report& report: m_data.reports)
    {
        report.descriptor.sensors.erase(sensorId);
    }
	m_data.reportsChanged = true;

    s_logger.logInfo(QString("Removing sensor '%1'").arg(m_data.sensors[index].descriptor.name.c_str()));

    m_data.sensors.erase(m_data.sensors.begin() + index);
	m_data.sensorsAddedOrRemoved = true;
    emit sensorRemoved(sensorId);

    //refresh computed comms period as new sensors are removed
	if (!m_data.sensorTimeConfigs.empty())
    {
		SensorTimeConfig& config = m_data.sensorTimeConfigs.back();
        config.computedCommsPeriod = computeActualCommsPeriod(config.descriptor);
		m_data.sensorTimeConfigsChanged = true;
        emit sensorTimeConfigChanged();
    }

	save(true);
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

DB::Alarm DB::getAlarm(size_t index) const
{
    std::lock_guard<std::recursive_mutex> lg(m_dataMutex);

    Q_ASSERT(index < m_data.alarms.size());
    return m_data.alarms[index];
}

//////////////////////////////////////////////////////////////////////////

Result<void> DB::checkAlarmDescriptor(AlarmDescriptor const& descriptor) const
{
    if (descriptor.highTemperatureWatch && descriptor.highTemperatureSoft > descriptor.highTemperatureHard)
    {
        return Error(QString("High temperature soft threshold (%1) is bigger than the hard threshold (%2)").arg(descriptor.highTemperatureSoft).arg(descriptor.highTemperatureHard).toUtf8().data());
    }
	if (descriptor.lowTemperatureWatch && descriptor.lowTemperatureSoft < descriptor.lowTemperatureHard)
	{
		return Error(QString("Low temperature soft threshold (%1) is smaller than the hard threshold (%2)").arg(descriptor.lowTemperatureSoft).arg(descriptor.lowTemperatureHard).toUtf8().data());
	}
    if (descriptor.highTemperatureWatch && descriptor.lowTemperatureWatch && descriptor.highTemperatureHard <= descriptor.lowTemperatureHard)
    {
		return Error(QString("High temperature threshold (%1) is smaller than the low threshold (%2)").arg(descriptor.highTemperatureHard).arg(descriptor.lowTemperatureHard).toUtf8().data());
    }


	if (descriptor.highHumidityWatch && descriptor.highHumiditySoft > descriptor.highHumidityHard)
	{
		return Error(QString("High humidity soft threshold (%1) is bigger than the hard threshold (%2)").arg(descriptor.highHumiditySoft).arg(descriptor.highHumidityHard).toUtf8().data());
	}
	if (descriptor.lowHumidityWatch && descriptor.lowHumiditySoft < descriptor.lowHumidityHard)
	{
		return Error(QString("Low humidity soft threshold (%1) is smaller than the hard threshold (%2)").arg(descriptor.lowHumiditySoft).arg(descriptor.lowHumidityHard).toUtf8().data());
	}
	if (descriptor.highHumidityWatch && descriptor.lowHumidityWatch && descriptor.highHumidityHard <= descriptor.lowHumidityHard)
	{
		return Error(QString("High humidity threshold (%1) is smaller than the low threshold (%2)").arg(descriptor.highHumidityHard).arg(descriptor.lowHumidityHard).toUtf8().data());
	}

	return success;
}

//////////////////////////////////////////////////////////////////////////

Result<void> DB::addAlarm(AlarmDescriptor const& descriptor)
{
    std::lock_guard<std::recursive_mutex> lg(m_dataMutex);

    if (findAlarmIndexByName(descriptor.name) >= 0)
    {
        return Error("Name '" + descriptor.name + "' already in use");
    }

    Result<void> result = checkAlarmDescriptor(descriptor);
    if (result != success)
    {
        return result;
    }

    Alarm alarm;
    alarm.descriptor = descriptor;
    if (!alarm.descriptor.filterSensors)
    {
        alarm.descriptor.sensors.clear();
    }

    alarm.id = ++m_data.lastAlarmId;

    m_data.alarms.push_back(alarm);
	m_data.alarmsAddedOrRemoved = true;
    emit alarmAdded(alarm.id);

    s_logger.logInfo(QString("Added alarm '%1'").arg(descriptor.name.c_str()));

	save(true);

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

	Result<void> result = checkAlarmDescriptor(descriptor);
	if (result != success)
	{
		return result;
	}

    size_t index = static_cast<size_t>(_index);
    m_data.alarms[index].descriptor = descriptor;
	m_data.alarmsChanged = true;
    emit alarmChanged(id);

    s_logger.logInfo(QString("Changed alarm '%1'").arg(descriptor.name.c_str()));

	save(true);

    return success;
}

//////////////////////////////////////////////////////////////////////////

void DB::removeAlarm(size_t index)
{
    std::lock_guard<std::recursive_mutex> lg(m_dataMutex);

    Q_ASSERT(index < m_data.alarms.size());
    AlarmId id = m_data.alarms[index].id;

    s_logger.logInfo(QString("Removed alarm '%1'").arg(m_data.alarms[index].descriptor.name.c_str()));

    m_data.alarms.erase(m_data.alarms.begin() + index);
	m_data.alarmsAddedOrRemoved = true;
    emit alarmRemoved(id);

	save(true);
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

DB::AlarmTriggers DB::computeSensorAlarmTriggers(Sensor& sensor, std::optional<Measurement> measurement)
{
    std::lock_guard<std::recursive_mutex> lg(m_dataMutex);

	AlarmTriggers allTriggers;

    for (Alarm& alarm: m_data.alarms)
    {
		AlarmTriggers triggers = _computeSensorAlarmTriggers(alarm, sensor, measurement);
        allTriggers |= triggers;
    }

    return allTriggers;
}

//////////////////////////////////////////////////////////////////////////

DB::AlarmTriggers DB::_computeSensorAlarmTriggers(Alarm& alarm, Sensor& sensor, std::optional<Measurement> measurement)
{
    std::lock_guard<std::recursive_mutex> lg(m_dataMutex);

    uint32_t currentTriggers = 0;

	bool sensorWatched = true;
    AlarmDescriptor const& ad = alarm.descriptor;
    if (ad.filterSensors)
    {
        if (ad.sensors.find(sensor.id) == ad.sensors.end())
        {
			sensorWatched = false;
        }
    }

	uint32_t oldTriggers = 0;
	auto it = alarm.triggersPerSensor.find(sensor.id);
	if (it != alarm.triggersPerSensor.end())
	{
		oldTriggers = it->second;
	}

	if (sensorWatched)
	{
		if (measurement.has_value())
		{
			Measurement const& m = *measurement;
			if (ad.highTemperatureWatch && m.descriptor.temperature > ad.highTemperatureSoft)
			{
				currentTriggers |= AlarmTrigger::MeasurementHighTemperatureSoft;
			}
			if (ad.highTemperatureWatch && m.descriptor.temperature > ad.highTemperatureHard)
			{
				currentTriggers |= AlarmTrigger::MeasurementHighTemperatureHard;
			}
			if (ad.lowTemperatureWatch && m.descriptor.temperature < ad.lowTemperatureSoft)
			{
				currentTriggers |= AlarmTrigger::MeasurementLowTemperatureSoft;
			}
			if (ad.lowTemperatureWatch && m.descriptor.temperature < ad.lowTemperatureHard)
			{
				currentTriggers |= AlarmTrigger::MeasurementLowTemperatureHard;
			}

			if (ad.highHumidityWatch && m.descriptor.humidity > ad.highHumiditySoft)
			{
				currentTriggers |= AlarmTrigger::MeasurementHighHumiditySoft;
			}
			if (ad.highHumidityWatch && m.descriptor.humidity > ad.highHumidityHard)
			{
				currentTriggers |= AlarmTrigger::MeasurementHighHumidityHard;
			}
			if (ad.lowHumidityWatch && m.descriptor.humidity < ad.lowHumiditySoft)
			{
				currentTriggers |= AlarmTrigger::MeasurementLowHumiditySoft;
			}
			if (ad.lowHumidityWatch && m.descriptor.humidity < ad.lowHumidityHard)
			{
				currentTriggers |= AlarmTrigger::MeasurementLowHumidityHard;
			}

			if (ad.lowVccWatch && utils::getBatteryLevel(m.descriptor.vcc) <= m_data.sensorSettings.alertBatteryLevel)
			{
				currentTriggers |= AlarmTrigger::MeasurementLowVcc;
			}

			if (ad.lowSignalWatch && utils::getSignalLevel(std::min(m.descriptor.signalStrength.b2s, m.descriptor.signalStrength.s2b)) <= m_data.sensorSettings.alertSignalStrengthLevel)
			{
				currentTriggers |= AlarmTrigger::MeasurementLowSignal;
			}
		}
		else
		{
			//keep measurement triggers
			currentTriggers = oldTriggers & AlarmTrigger::MeasurementMask;
		}
	}

	if (ad.sensorBlackoutWatch && sensor.blackout)
	{
		currentTriggers |= AlarmTrigger::SensorBlackout;
	}

    if (currentTriggers != oldTriggers)
    {
        if (currentTriggers == 0)
        {
            alarm.triggersPerSensor.erase(sensor.id);
        }
        else
        {
            alarm.triggersPerSensor[sensor.id] = currentTriggers;
        }
		m_data.alarmsChanged = true;
    }

	AlarmTriggers triggers;
	triggers.current = currentTriggers;

    uint32_t diff = oldTriggers ^ currentTriggers;
    if (diff != 0)
    {
		triggers.removed = diff & oldTriggers;
		triggers.added = diff & currentTriggers;

        s_logger.logInfo(QString("Alarm '%1' triggers for measurement index %2 have changed: old %3, new %4, added %5, removed %6")
                         .arg(ad.name.c_str())
                         .arg(measurement.has_value() ? std::to_string(measurement->descriptor.index).c_str() : "N/A")
                         .arg(oldTriggers)
                         .arg(currentTriggers)
                         .arg(triggers.added)
                         .arg(triggers.removed));

		alarm.lastTriggeredTimePoint = m_clock->now();
        emit alarmSensorTriggersChanged(alarm.id, sensor.id, measurement, oldTriggers, triggers);
    }

    return triggers;
}

//////////////////////////////////////////////////////////////////////////

DB::AlarmTriggers DB::computeBaseStationAlarmTriggers(BaseStation const& bs)
{
	std::lock_guard<std::recursive_mutex> lg(m_dataMutex);

	AlarmTriggers allTriggers;

	for (Alarm& alarm : m_data.alarms)
	{
		AlarmTriggers triggers = _computeBaseStationAlarmTriggers(alarm, bs);
		allTriggers |= triggers;
	}

	return allTriggers;
}

//////////////////////////////////////////////////////////////////////////

DB::AlarmTriggers DB::_computeBaseStationAlarmTriggers(Alarm& alarm, BaseStation const& bs)
{
	std::lock_guard<std::recursive_mutex> lg(m_dataMutex);

	uint32_t currentTriggers = 0;

	AlarmDescriptor const& ad = alarm.descriptor;
	if (ad.baseStationDisconnectedWatch && !bs.isConnected)
	{
		currentTriggers |= AlarmTrigger::BaseStationDisconnected;
	}

	uint32_t oldTriggers = 0;
	auto it = alarm.triggersPerBaseStation.find(bs.id);
	if (it != alarm.triggersPerBaseStation.end())
	{
		oldTriggers = it->second;
	}

	if (currentTriggers != oldTriggers)
	{
		if (currentTriggers == 0)
		{
			alarm.triggersPerBaseStation.erase(bs.id);
		}
		else
		{
			alarm.triggersPerBaseStation[bs.id] = currentTriggers;
		}
		m_data.alarmsChanged = true;
	}

	AlarmTriggers triggers;
	triggers.current = currentTriggers;

	uint32_t diff = oldTriggers ^ currentTriggers;
	if (diff != 0)
	{
		triggers.removed = diff & oldTriggers;
		triggers.added = diff & currentTriggers;

		s_logger.logInfo(QString("Alarm '%1' triggers for base station %2 have changed: old %3, new %4, added %5, removed %6")
						 .arg(ad.name.c_str())
						 .arg(bs.id)
						 .arg(oldTriggers)
						 .arg(currentTriggers)
						 .arg(triggers.added)
						 .arg(triggers.removed));

		alarm.lastTriggeredTimePoint = m_clock->now();
		emit alarmBaseStationTriggersChanged(alarm.id, bs.id, oldTriggers, triggers);
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

DB::Report DB::getReport(size_t index) const
{
    std::lock_guard<std::recursive_mutex> lg(m_dataMutex);

    Q_ASSERT(index < m_data.reports.size());
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
	m_data.reportsAddedOrRemoved = true;
    emit reportAdded(report.id);

    s_logger.logInfo(QString("Added report '%1'").arg(descriptor.name.c_str()));

	save(true);

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
	m_data.reportsChanged = true;
    emit reportChanged(id);

    s_logger.logInfo(QString("Changed report '%1'").arg(descriptor.name.c_str()));

    save(true);

    return success;
}

//////////////////////////////////////////////////////////////////////////

void DB::removeReport(size_t index)
{
    std::lock_guard<std::recursive_mutex> lg(m_dataMutex);

    Q_ASSERT(index < m_data.reports.size());
    ReportId id = m_data.reports[index].id;

    s_logger.logInfo(QString("Removed report '%1'").arg(m_data.reports[index].descriptor.name.c_str()));

    m_data.reports.erase(m_data.reports.begin() + index);
	m_data.reportsAddedOrRemoved = true;
    emit reportRemoved(id);

	save(true);
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

bool DB::isReportTriggered(Report const& report) const
{
    if (report.descriptor.period == ReportDescriptor::Period::Daily)
    {
        QDateTime dt = QDateTime::currentDateTime();
        dt.setTime(QTime(9, 0));
        if (IClock::to_time_t(report.lastTriggeredTimePoint) < dt.toTime_t() && IClock::to_time_t(m_clock->now()) >= dt.toTime_t())
        {
            return true;
        }
    }
    else if (report.descriptor.period == ReportDescriptor::Period::Weekly)
    {
        QDateTime dt = QDateTime::currentDateTime();
        dt.setDate(dt.date().addDays(-dt.date().dayOfWeek()));
        dt.setTime(QTime(9, 0));
        if (IClock::to_time_t(report.lastTriggeredTimePoint) < dt.toTime_t() && IClock::to_time_t(m_clock->now()) >= dt.toTime_t())
        {
            return true;
        }
    }
    else if (report.descriptor.period == ReportDescriptor::Period::Weekly)
    {
        QDate date = QDate::currentDate();
        date = QDate(date.year(), date.month(), 1);
        QDateTime dt(date, QTime(9, 0));

        if (IClock::to_time_t(report.lastTriggeredTimePoint) < dt.toTime_t() && IClock::to_time_t(m_clock->now()) >= dt.toTime_t())
        {
            return true;
        }
    }
    else if (report.descriptor.period == ReportDescriptor::Period::Custom)
    {
		IClock::time_point now = m_clock->now();
		IClock::duration d = now - report.lastTriggeredTimePoint;
        if (d >= report.descriptor.customPeriod)
        {
            return true;
        }
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////

bool DB::addMeasurement(MeasurementDescriptor const& md)
{
    std::lock_guard<std::recursive_mutex> lg(m_dataMutex);

    return addSingleSensorMeasurements(md.sensorId, { md });
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
        ok &= addSingleSensorMeasurements(pair.first, std::move(pair.second));
    }
    return ok;
}

//////////////////////////////////////////////////////////////////////////

bool DB::addSingleSensorMeasurements(SensorId sensorId, std::vector<MeasurementDescriptor> mds)
{
    std::vector<Measurement> asyncMeasurements;
	asyncMeasurements.reserve(mds.size());

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

		if (!mds.empty())
		{
			sqlite3_exec(m_sqlite, "BEGIN TRANSACTION;", NULL, NULL, NULL);
			utils::epilogue epi1([this] { sqlite3_exec(m_sqlite, "END TRANSACTION;", NULL, NULL, NULL); });

			sqlite3_stmt* stmt = m_addMeasurementsStmt.get();
			for (MeasurementDescriptor const& md : mds)
			{
				//advance the last confirmed index
				if (md.index == sensor.lastConfirmedMeasurementIndex + 1)
				{
					sensor.lastConfirmedMeasurementIndex = md.index;
					m_data.sensorsChanged = true;
				}

				Measurement m;
				//measurement.id = id;
				m.descriptor = md;
				m.timePoint = computeMeasurementTimepoint(md);
                m.receivedTimePoint = m_clock->now();
				//m.alarmTriggers = computeAlarmTriggersForMeasurement(m);
                asyncMeasurements.emplace_back(m);

				sqlite3_bind_int64(stmt, 1, IClock::to_time_t(m.timePoint));
				sqlite3_bind_int64(stmt, 2, IClock::to_time_t(m.receivedTimePoint));
				sqlite3_bind_int64(stmt, 3, md.index);
				sqlite3_bind_int64(stmt, 4, md.sensorId);
				sqlite3_bind_double(stmt, 5, md.temperature);
				sqlite3_bind_double(stmt, 6, md.humidity);
				sqlite3_bind_double(stmt, 7, md.vcc);
				sqlite3_bind_int64(stmt, 8, md.signalStrength.s2b);
				sqlite3_bind_int64(stmt, 9, md.signalStrength.b2s);
				sqlite3_bind_int64(stmt, 10, md.sensorErrors);
				sqlite3_bind_int64(stmt, 11, m.alarmTriggers.current);
				sqlite3_bind_int64(stmt, 12, m.alarmTriggers.added);
				sqlite3_bind_int64(stmt, 13, m.alarmTriggers.removed);
				if (sqlite3_step(stmt) != SQLITE_DONE)
				{
					s_logger.logCritical(QString("Failed to save measurement: %1").arg(sqlite3_errmsg(m_sqlite)));
				}
				sqlite3_reset(stmt);
			}

			save(false);
		}
	}

	{
		std::lock_guard<std::recursive_mutex> lg(m_asyncMeasurementsMutex);
		std::copy(asyncMeasurements.begin(), asyncMeasurements.end(), std::back_inserter(m_asyncMeasurements));
	}

    return true;
}

//////////////////////////////////////////////////////////////////////////

void DB::addAsyncMeasurements()
{
	std::lock_guard<std::recursive_mutex> lg(m_dataMutex);

    struct SensorData
    {
		uint32_t minIndex = std::numeric_limits<uint32_t>::max();
		uint32_t maxIndex = std::numeric_limits<uint32_t>::lowest();
		MeasurementDescriptor lastMeasurementDescriptor;
    };
    std::map<SensorId, SensorData> sensorDatas;

	{
		std::lock_guard<std::recursive_mutex> lg(m_asyncMeasurementsMutex);
		for (Measurement const& m : m_asyncMeasurements)
		{
			MeasurementDescriptor const& md = m.descriptor;
            SensorData& sd = sensorDatas[md.sensorId];
			sd.minIndex = std::min(sd.minIndex, md.index);
			sd.maxIndex = std::max(sd.maxIndex, md.index);
			sd.lastMeasurementDescriptor = md;
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
				//this is also set in the setSensorInputDetails
				if (!sensor.isRTMeasurementValid)
				{
					sensor.isRTMeasurementValid = true;
					sensor.rtMeasurementTemperature = p.second.lastMeasurementDescriptor.temperature;
					sensor.rtMeasurementHumidity = p.second.lastMeasurementDescriptor.humidity;
					sensor.rtMeasurementVcc = p.second.lastMeasurementDescriptor.vcc;
				}
				sensor.averageSignalStrength = computeAverageSignalStrength(sensor.id, m_data);
				m_data.sensorsChanged = true;
				emit sensorDataChanged(sensor.id);
				s_logger.logVerbose(QString("Added measurement indices %1 to %2, sensor '%3'").arg(p.second.minIndex).arg(p.second.maxIndex).arg(sensor.descriptor.name.c_str()));
			}
			emit measurementsAdded(p.first);
		}

		if (!sensorDatas.empty())
		{
			save(true);
		}
	}
}

//////////////////////////////////////////////////////////////////////////

IClock::time_point DB::computeMeasurementTimepoint(MeasurementDescriptor const& md) const
{
    std::lock_guard<std::recursive_mutex> lg(m_dataMutex);

	SensorTimeConfig config = findSensorTimeConfigForMeasurementIndex(md.index);
    uint32_t index = md.index >= config.baselineMeasurementIndex ? md.index - config.baselineMeasurementIndex : 0;
	IClock::time_point tp = config.baselineMeasurementTimePoint + config.descriptor.measurementPeriod * index;
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
        str += std::to_string(IClock::to_time_t(filter.timePointFilter.min));
        str += " AND timePoint <= ";
        str += std::to_string(IClock::to_time_t(filter.timePointFilter.max));
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
	IClock::time_point start = m_clock->now();

    size_t count = 0;

	if (out)
	{
		out->reserve(100000);
	}

	{
		std::string sql = "SELECT id, timePoint, receivedTimePoint, idx, sensorId, temperature, humidity, vcc, signalStrengthS2B, signalStrengthB2S, sensorErrors, alarmTriggersCurrent, alarmTriggersAdded, alarmTriggersRemoved "
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
				out->emplace_back(unpackMeasurement(stmt));
			}
			count++;
		}
	}

	std::cout << (QString("Computed filtered measurements: %3ms\n")
				  .arg(std::chrono::duration_cast<std::chrono::milliseconds>(DB::m_clock->now() - start).count())).toStdString();

    return count;
}

//////////////////////////////////////////////////////////////////////////

Result<DB::Measurement> DB::getLastMeasurementForSensor(SensorId sensorId) const
{
    std::lock_guard<std::recursive_mutex> lg(m_dataMutex);

    QString sql = QString("SELECT id, timePoint, receivedTimePoint, idx, sensorId, temperature, humidity, vcc, signalStrengthS2B, signalStrengthB2S, sensorErrors, alarmTriggersCurrent, alarmTriggersAdded, alarmTriggersRemoved "
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
		return unpackMeasurement(stmt);
	}

    return Error("No data");
}

//////////////////////////////////////////////////////////////////////////

Result<DB::Measurement> DB::findMeasurementById(MeasurementId id) const
{
	std::lock_guard<std::recursive_mutex> lg(m_dataMutex);

	QString sql = QString("SELECT id, timePoint, receivedTimePoint, idx, sensorId, temperature, humidity, vcc, signalStrengthS2B, signalStrengthB2S, sensorErrors, alarmTriggersCurrent, alarmTriggersAdded, alarmTriggersRemoved "
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
		return unpackMeasurement(stmt);
	}

	return Error("Cannot find measurement");
}

//////////////////////////////////////////////////////////////////////////

Result<void> DB::setMeasurement(MeasurementId id, MeasurementDescriptor const& measurement)
{
	std::lock_guard<std::recursive_mutex> lg(m_dataMutex);

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

    emit measurementsChanged();
    return success;
}

//////////////////////////////////////////////////////////////////////////

// inline DB::MeasurementId DB::computeMeasurementId(MeasurementDescriptor const& md)
// {
//     return (MeasurementId(md.sensorId) << 32) | MeasurementId(md.index);
// }

//////////////////////////////////////////////////////////////////////////

DB::Measurement DB::unpackMeasurement(sqlite3_stmt* stmt)
{
	Measurement m;
	m.id = sqlite3_column_int64(stmt, 0);
	m.timePoint = IClock::from_time_t(sqlite3_column_int64(stmt, 1));
	m.receivedTimePoint = IClock::from_time_t(sqlite3_column_int64(stmt, 2));
	m.descriptor.index = sqlite3_column_int64(stmt, 3);
	m.descriptor.sensorId = (uint32_t)sqlite3_column_int64(stmt, 4);
	m.descriptor.temperature = (float)sqlite3_column_double(stmt, 5);
	m.descriptor.humidity = (float)sqlite3_column_double(stmt, 6);
	m.descriptor.vcc = (float)sqlite3_column_double(stmt, 7);
	m.descriptor.signalStrength.s2b = (int16_t)sqlite3_column_int(stmt, 8);
	m.descriptor.signalStrength.b2s = (int16_t)sqlite3_column_int(stmt, 9);
	m.descriptor.sensorErrors = (uint32_t)sqlite3_column_int(stmt, 10);
	m.alarmTriggers.current = (uint32_t)sqlite3_column_int(stmt, 11);
	m.alarmTriggers.added = (uint32_t)sqlite3_column_int(stmt, 12);
	m.alarmTriggers.removed = (uint32_t)sqlite3_column_int(stmt, 13);
    return m;
}

//////////////////////////////////////////////////////////////////////////

DB::SignalStrength DB::computeAverageSignalStrength(SensorId sensorId, Data const& data) const
{
	std::lock_guard<std::recursive_mutex> lg(m_dataMutex);

	QString sql = QString("SELECT id, timePoint, receivedTimePoint, idx, sensorId, temperature, humidity, vcc, signalStrengthS2B, signalStrengthB2S, sensorErrors, alarmTriggersCurrent, alarmTriggersAdded, alarmTriggersRemoved "
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
		Measurement m = unpackMeasurement(stmt);
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
			Sensor sensor = getSensor((size_t)sensorIndex);
			return { sensor.lastSignalStrengthB2S, sensor.lastSignalStrengthB2S };
		}
    }
	SignalStrength avg;
    avg.b2s = static_cast<int16_t>(avgb2s);
    avg.s2b = static_cast<int16_t>(avgs2b);
	return avg;
}

//////////////////////////////////////////////////////////////////////////

