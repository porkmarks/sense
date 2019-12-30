#include "Settings.h"
#include <algorithm>
#include <functional>
#include <cassert>
#include <fstream>
#include <iostream>
#include <QApplication>
#include <QDateTime>
#include <QDir>

#include "Utils.h"
#include "Crypt.h"
#include "Logger.h"

#include "cereal/archives/json.hpp"
#include "cereal/types/vector.hpp"
#include "cereal/types/string.hpp"
#include "cereal/types/chrono.hpp"
#include "cereal/external/rapidjson/error/en.h"
#include "PermissionsCheck.h"

#include "sqlite3.h"
#include <numeric>

#ifdef _MSC_VER
//not #if defined(_WIN32) || defined(_WIN64) because we have strncasecmp in mingw
#define strncasecmp _strnicmp
#define strcasecmp _stricmp
#endif

#ifdef NDEBUG
#   define USE_DATA_ENCRYPTION
#endif

extern Logger s_logger;

extern std::string s_dataFolder;

constexpr uint64_t k_fileEncryptionKey = 32455153254365ULL;
//constexpr uint64_t k_emailEncryptionKey = 1735271834639209ULL;
//constexpr uint64_t k_ftpEncryptionKey = 45235321231234ULL;

//CEREAL_CLASS_VERSION(Settings::Data, 1);

CEREAL_CLASS_VERSION(Settings::UserDescriptor, 1);

//////////////////////////////////////////////////////////////////////////

Settings::Settings()
{
    m_storeThread = std::thread(std::bind(&Settings::storeThreadProc, this));

    m_db = std::unique_ptr<DB>(new DB());
    m_emailer.reset(new Emailer(*this, *m_db));
}

//////////////////////////////////////////////////////////////////////////

Settings::~Settings()
{
    m_threadsExit = true;

    m_storeCV.notify_all();
    if (m_storeThread.joinable())
    {
        m_storeThread.join();
    }

    m_sqlite = nullptr;
}

//////////////////////////////////////////////////////////////////////////

void Settings::process()
{
    m_db->process();
}

//////////////////////////////////////////////////////////////////////////

Result<void> Settings::create(sqlite3& db)
{
    char* errorMsg = nullptr;
	{
		const char* sqlCreateTable = "CREATE TABLE EmailSettings (id INTEGER PRIMARY KEY, host STRING, port INTEGER, connection INTEGER, username STRING, password STRING, sender STRING, recipients STRING);";
		if (sqlite3_exec(&db, sqlCreateTable, NULL, NULL, &errorMsg))
		{
			Error error(QString("Error executing SQLite3 statement: %1").arg(sqlite3_errmsg(&db)).toUtf8().data());
			sqlite3_free(errorMsg);
			return error;
		}
		const char* sqlInsert = "INSERT INTO EmailSettings VALUES(0, '', 465, 0, '', '', '', '');";
		if (sqlite3_exec(&db, sqlInsert, NULL, NULL, &errorMsg))
		{
			Error error(QString("Error executing SQLite3 statement: %1").arg(sqlite3_errmsg(&db)).toUtf8().data());
			sqlite3_free(errorMsg);
			return error;
		}
	}
	{
		const char* sqlCreateTable = "CREATE TABLE FtpSettings (id INTEGER PRIMARY KEY, host STRING, port INTEGER, username STRING, password STRING, folder STRING, uploadBackups BOOLEAN, uploadPeriod INTEGER);";
		if (sqlite3_exec(&db, sqlCreateTable, NULL, NULL, &errorMsg))
		{
			Error error(QString("Error executing SQLite3 statement: %1").arg(sqlite3_errmsg(&db)).toUtf8().data());
			sqlite3_free(errorMsg);
			return error;
		}
		const char* sqlInsert = "INSERT INTO FtpSettings VALUES(0, '', 21, '', '', '', 0, 14400);";
		if (sqlite3_exec(&db, sqlInsert, NULL, NULL, &errorMsg))
		{
			Error error(QString("Error executing SQLite3 statement: %1").arg(sqlite3_errmsg(&db)).toUtf8().data());
			sqlite3_free(errorMsg);
			return error;
		}
	}
	{
		const char* sqlCreateTable = "CREATE TABLE Users (id INTEGER PRIMARY KEY, name STRING, passwordHash STRING, permissions INTEGER, type INTEGER, lastLogin DATETIME);";
		if (sqlite3_exec(&db, sqlCreateTable, NULL, NULL, &errorMsg))
		{
			Error error(QString("Error executing SQLite3 statement: %1").arg(sqlite3_errmsg(&db)).toUtf8().data());
			sqlite3_free(errorMsg);
			return error;
		}
	}
    return success;
}

//////////////////////////////////////////////////////////////////////////

bool Settings::load(sqlite3& db)
{
	Data data;
	char* errorMsg = nullptr;
	{
        //id INTEGER PRIMARY KEY, host STRING, port INTEGER, connection INTEGER, username STRING, password STRING, sender STRING, recipients STRING
		const char* sql = "SELECT host, port, connection, username, password, sender, recipients FROM EmailSettings;";
		sqlite3_stmt* res;
		if (sqlite3_prepare_v2(&db, sql, -1, &res, 0) != SQLITE_OK) 
        {
			s_logger.logCritical(QString("Cannot load email settings: %1").arg(sqlite3_errmsg(&db)));
            return false;
		}
        if (sqlite3_step(res) != SQLITE_ROW)
        {
			sqlite3_finalize(res);
            s_logger.logCritical(QString("Cannot load email settings row: %1").arg(sqlite3_errmsg(&db)));
            return false;
        }
		data.emailSettings.host = (char const*)sqlite3_column_text(res, 0);
		data.emailSettings.port = sqlite3_column_int(res, 1);
		data.emailSettings.connection = (EmailSettings::Connection)sqlite3_column_int(res, 2);
		data.emailSettings.username = (char const*)sqlite3_column_text(res, 3);
		data.emailSettings.password = (char const*)sqlite3_column_text(res, 4);
		data.emailSettings.sender = (char const*)sqlite3_column_text(res, 5);
		QString recipients = (char const*)sqlite3_column_text(res, 6);

		QStringList l = recipients.split(QChar(';'), QString::SkipEmptyParts);
		for (QString str : l)
		{
			data.emailSettings.recipients.push_back(str.trimmed().toUtf8().data());
		}
		sqlite3_finalize(res);
	}
	{
        //FtpSettings (id INTEGER PRIMARY KEY, host STRING, port INTEGER, username STRING, password STRING, folder STRING, uploadBackups BOOLEAN, uploadPeriod INTEGER);";
		const char* sql = "SELECT host, port, username, password, folder, uploadBackups, uploadPeriod FROM FtpSettings;";
		sqlite3_stmt* res;
		if (sqlite3_prepare_v2(&db, sql, -1, &res, 0) != SQLITE_OK)
		{
			s_logger.logCritical(QString("Cannot load ftp settings: %1").arg(sqlite3_errmsg(&db)));
			return false;
		}
		if (sqlite3_step(res) != SQLITE_ROW)
		{
			sqlite3_finalize(res);
			s_logger.logCritical(QString("Cannot load ftp settings row: %1").arg(sqlite3_errmsg(&db)));
			return false;
		}

		data.ftpSettings.host = (char const*)sqlite3_column_text(res, 0);
		data.ftpSettings.port = sqlite3_column_int(res, 1);
		data.ftpSettings.username = (char const*)sqlite3_column_text(res, 2);
		data.ftpSettings.password = (char const*)sqlite3_column_text(res, 3);
		data.ftpSettings.folder = (char const*)sqlite3_column_text(res, 4);
        data.ftpSettings.uploadBackups = sqlite3_column_int(res, 5) ? true : false;
        data.ftpSettings.uploadBackupsPeriod = std::chrono::seconds(sqlite3_column_int64(res, 6));
		sqlite3_finalize(res);
	}
	{
		//Users (id INTEGER PRIMARY KEY, name STRING, passwordHash STRING, permissions INTEGER, type INTEGER, lastLogin DATETIME);";
		const char* sql = "SELECT id, name, passwordHash, permissions, type, lastLogin FROM Users;";
		sqlite3_stmt* res;
		if (sqlite3_prepare_v2(&db, sql, -1, &res, 0) != SQLITE_OK)
		{
			s_logger.logCritical(QString("Cannot load user settings: %1").arg(sqlite3_errmsg(&db)));
			return false;
		}
        while (sqlite3_step(res) == SQLITE_ROW)
        {
            User user;
            user.id = sqlite3_column_int(res, 0);
            user.descriptor.name = (char const*)sqlite3_column_text(res, 1);
            user.descriptor.passwordHash = (char const*)sqlite3_column_text(res, 2);
            user.descriptor.permissions = sqlite3_column_int(res, 3);
            user.descriptor.type = (UserDescriptor::Type)sqlite3_column_int(res, 4);
            user.lastLogin = Clock::from_time_t(sqlite3_column_int64(res, 5));
            data.users.push_back(std::move(user));
        }
		sqlite3_finalize(res);
	}

    if (m_db->load() != success)
    {
        if (m_db->create() != success)
        {
            s_logger.logCritical(QString("Cannot open nor create the DB"));
            return false;
        }
    }

	m_sqlite = &db;
    m_mainData = data;

    return true;
}

//////////////////////////////////////////////////////////////////////////

DB const& Settings::getDB() const
{
    return *m_db;
}

//////////////////////////////////////////////////////////////////////////

DB& Settings::getDB()
{
    return *m_db;
}

//////////////////////////////////////////////////////////////////////////

//void Settings::setActiveBaseStation(Mac mac)
//{
//    emit baseStationWillChanged();
//    m_mainData.activeBaseStation = mac;
//    emit baseStationChanged();

//    triggerSave();
//}

//////////////////////////////////////////////////////////////////////////

//Settings::Mac Settings::getActiveBaseStation() const
//{
//    return m_mainData.activeBaseStation;
//}

//////////////////////////////////////////////////////////////////////////

bool Settings::setEmailSettings(EmailSettings const& settings)
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

    emit emailSettingsWillBeChanged();
    m_mainData.emailSettings = settings;
    emit emailSettingsChanged();

    s_logger.logInfo("Changed email settings");

    triggerSave();

    return true;
}

//////////////////////////////////////////////////////////////////////////

Settings::EmailSettings const& Settings::getEmailSettings() const
{
    return m_mainData.emailSettings;
}

//////////////////////////////////////////////////////////////////////////

bool Settings::setFtpSettings(FtpSettings const& settings)
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

    emit ftpSettingsWillBeChanged();
    m_mainData.ftpSettings = settings;
    emit ftpSettingsChanged();

    s_logger.logInfo("Changed ftp settings");

    triggerSave();

    return true;
}

//////////////////////////////////////////////////////////////////////////

Settings::FtpSettings const& Settings::getFtpSettings() const
{
    return m_mainData.ftpSettings;
}

//////////////////////////////////////////////////////////////////////////

size_t Settings::getUserCount() const
{
    return m_mainData.users.size();
}

//////////////////////////////////////////////////////////////////////////

Settings::User const& Settings::getUser(size_t index) const
{
    assert(index < m_mainData.users.size());
    return m_mainData.users[index];
}

//////////////////////////////////////////////////////////////////////////

bool Settings::addUser(UserDescriptor const& descriptor)
{
    if (findUserIndexByName(descriptor.name) >= 0)
    {
        return false;
    }

    User user;
    user.descriptor = descriptor;
    user.id = ++m_mainData.lastUserId;

    emit userWillBeAdded(user.id);
    m_mainData.users.push_back(user);
    emit userAdded(user.id);

    s_logger.logInfo(QString("Added user '%1'").arg(descriptor.name.c_str()));

    triggerSave();

    return true;
}

//////////////////////////////////////////////////////////////////////////

Result<void> Settings::setUser(UserId id, UserDescriptor const& descriptor)
{
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

    User& user = m_mainData.users[static_cast<size_t>(index)];

    int32_t loggedInUserIndex = findUserIndexById(m_loggedInUserId);
    if (loggedInUserIndex >= 0)
    {
		const User& loggedInUser = m_mainData.users[static_cast<size_t>(loggedInUserIndex)];
        if (loggedInUser.descriptor.type != UserDescriptor::Type::Admin)
		{
			if (user.descriptor.permissions != descriptor.permissions &&
				(loggedInUser.descriptor.permissions & Settings::UserDescriptor::PermissionChangeUsers) == 0)
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
    emit userChanged(id);

    s_logger.logInfo(QString("Changed user '%1'").arg(descriptor.name.c_str()));

    triggerSave();

    return success;
}

//////////////////////////////////////////////////////////////////////////

void Settings::removeUser(size_t index)
{
    assert(index < m_mainData.users.size());
    UserId id = m_mainData.users[index].id;

    s_logger.logInfo(QString("Removed user '%1'").arg(m_mainData.users[index].descriptor.name.c_str()));

    emit userWillBeRemoved(id);
    m_mainData.users.erase(m_mainData.users.begin() + index);
    emit userRemoved(id);

    triggerSave();
}

//////////////////////////////////////////////////////////////////////////

int32_t Settings::findUserIndexByName(std::string const& name) const
{
    auto it = std::find_if(m_mainData.users.begin(), m_mainData.users.end(), [&name](User const& user)
    {
        return strcasecmp(user.descriptor.name.c_str(), name.c_str()) == 0;
    });

    if (it == m_mainData.users.end())
    {
        return -1;
    }
    return int32_t(std::distance(m_mainData.users.begin(), it));
}

//////////////////////////////////////////////////////////////////////////

int32_t Settings::findUserIndexById(UserId id) const
{
    auto it = std::find_if(m_mainData.users.begin(), m_mainData.users.end(), [id](User const& user) { return user.id == id; });
    if (it == m_mainData.users.end())
    {
        return -1;
    }
    return int32_t(std::distance(m_mainData.users.begin(), it));
}

//////////////////////////////////////////////////////////////////////////

int32_t Settings::findUserIndexByPasswordHash(std::string const& passwordHash) const
{
    auto it = std::find_if(m_mainData.users.begin(), m_mainData.users.end(), [&passwordHash](User const& user) { return user.descriptor.passwordHash == passwordHash; });
    if (it == m_mainData.users.end())
    {
        return -1;
    }
    return int32_t(std::distance(m_mainData.users.begin(), it));
}

//////////////////////////////////////////////////////////////////////////

bool Settings::needsAdmin() const
{
    auto it = std::find_if(m_mainData.users.begin(), m_mainData.users.end(), [](User const& user) { return user.descriptor.type == UserDescriptor::Type::Admin; });
    if (it == m_mainData.users.end())
    {
        return true;
    }
    return false;
}

//////////////////////////////////////////////////////////////////////////

void Settings::setLoggedInUserId(UserId id)
{
    int32_t _index = findUserIndexById(id);
    if (_index >= 0)
    {
        size_t index = static_cast<size_t>(_index);
		s_logger.logInfo(QString("User '%1' logged in").arg(m_mainData.users[index].descriptor.name.c_str()));

        for (size_t i = 0; i < 10000; i++)
		{
			s_logger.logInfo(QString("User '%1' logged in").arg(m_mainData.users[index].descriptor.name.c_str()));
		}

        m_loggedInUserId = id;
        emit userLoggedIn(id);
    }
}

//////////////////////////////////////////////////////////////////////////

Settings::UserId Settings::getLoggedInUserId() const
{
    return m_loggedInUserId;
}

//////////////////////////////////////////////////////////////////////////

Settings::User const* Settings::getLoggedInUser() const
{
    int32_t _index = findUserIndexById(m_loggedInUserId);
    if (_index >= 0)
    {
        size_t index = static_cast<size_t>(_index);
        return &m_mainData.users[index];
    }
    return nullptr;
}

//////////////////////////////////////////////////////////////////////////

bool Settings::isLoggedInAsAdmin() const
{
    User const* user = getLoggedInUser();
    if (user == nullptr)
    {
        return false;
    }
    return user->descriptor.type == UserDescriptor::Type::Admin;
}

//////////////////////////////////////////////////////////////////////////

void Settings::triggerSave()
{
    {
        std::unique_lock<std::mutex> lg(m_storeMutex);
        m_storeData = m_mainData;
        m_storeThreadTriggered = true;
    }
    m_storeCV.notify_all();

    //s_logger.logVerbose("Settings save triggered");
}

//////////////////////////////////////////////////////////////////////////

void Settings::save(Data const& data) const
{
    Clock::time_point now = Clock::now();

	{
		sqlite3_stmt* stmt;
		sqlite3_prepare_v2(m_sqlite, "REPLACE INTO EmailSettings (id, host, port, connection, username, password, sender, recipients) VALUES (0, ?1, ?2, ?3, ?4, ?5, ?6, ?7);", -1, &stmt, NULL);
		sqlite3_bind_text(stmt, 1, data.emailSettings.host.c_str(), -1, SQLITE_STATIC);
		sqlite3_bind_int(stmt, 2, data.emailSettings.port);
		sqlite3_bind_int(stmt, 3, (int)data.emailSettings.connection);
		sqlite3_bind_text(stmt, 4, data.emailSettings.username.c_str(), -1, SQLITE_STATIC);
		sqlite3_bind_text(stmt, 5, data.emailSettings.password.c_str(), -1, SQLITE_STATIC);
		sqlite3_bind_text(stmt, 6, data.emailSettings.sender.c_str(), -1, SQLITE_STATIC);
		std::string recipients = std::accumulate(data.emailSettings.recipients.begin(), data.emailSettings.recipients.end(), std::string(";"));
		sqlite3_bind_text(stmt, 7, recipients.c_str(), -1, SQLITE_STATIC);
		if (sqlite3_step(stmt) != SQLITE_DONE)
		{
			s_logger.logCritical(QString("Failed to save email settings: %1").arg(sqlite3_errmsg(m_sqlite)));
			sqlite3_finalize(stmt);
			return;
		}
	}

	{
		sqlite3_stmt* stmt;
		sqlite3_prepare_v2(m_sqlite, "REPLACE INTO FtpSettings (id, host, port, username, password, folder, uploadBackups, uploadPeriod) VALUES (0, ?1, ?2, ?3, ?4, ?5, ?6, ?7);", -1, &stmt, NULL);
		sqlite3_bind_text(stmt, 1, data.ftpSettings.host.c_str(), -1, SQLITE_STATIC);
		sqlite3_bind_int(stmt, 2, data.ftpSettings.port);
		sqlite3_bind_text(stmt, 3, data.ftpSettings.username.c_str(), -1, SQLITE_STATIC);
		sqlite3_bind_text(stmt, 4, data.ftpSettings.password.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 5, data.ftpSettings.folder.c_str(), -1, SQLITE_STATIC);
		sqlite3_bind_int(stmt, 6, data.ftpSettings.uploadBackups ? 1 : 0);
        sqlite3_bind_int64(stmt, 7, std::chrono::duration_cast<std::chrono::seconds>(data.ftpSettings.uploadBackupsPeriod).count());
		if (sqlite3_step(stmt) != SQLITE_DONE)
		{
			s_logger.logCritical(QString("Failed to save ftp settings: %1").arg(sqlite3_errmsg(m_sqlite)));
			sqlite3_finalize(stmt);
			return;
		}
	}

	{
		{
            char* errorMsg = nullptr;
			const char* sqlCreateTable = "DELETE FROM Users;";
			if (sqlite3_exec(m_sqlite, sqlCreateTable, NULL, NULL, &errorMsg))
			{
                s_logger.logCritical(QString("Failed to clear users: %1").arg(sqlite3_errmsg(m_sqlite)));
				return;
			}
        }
        for (User const& user: data.users)
		{
			sqlite3_stmt* stmt;
			sqlite3_prepare_v2(m_sqlite, "REPLACE INTO Users (id, name, passwordHash, permissions, type, lastLogin) VALUES(?1, ?2, ?3, ?4, ?5, ?6);", -1, &stmt, NULL);
			sqlite3_bind_int(stmt, 1, user.id);
			sqlite3_bind_text(stmt, 2, user.descriptor.name.c_str(), -1, SQLITE_STATIC);
			sqlite3_bind_text(stmt, 3, user.descriptor.passwordHash.c_str(), -1, SQLITE_STATIC);
			sqlite3_bind_int(stmt, 4, user.descriptor.permissions);
			sqlite3_bind_int(stmt, 5, (int)user.descriptor.type);
            sqlite3_bind_int64(stmt, 6, Clock::to_time_t(user.lastLogin));
			if (sqlite3_step(stmt) != SQLITE_DONE)
			{
				s_logger.logCritical(QString("Failed to save user: %1").arg(sqlite3_errmsg(m_sqlite)));
				sqlite3_finalize(stmt);
				return;
			}
		}
	}

}

//////////////////////////////////////////////////////////////////////////

void Settings::storeThreadProc()
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

