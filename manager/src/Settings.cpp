#include "Settings.h"
#include <algorithm>
#include <functional>
#include <cassert>
#include <fstream>
#include <iostream>
#include <QApplication>
#include <QDateTime>
#include <QDir>

#include "CRC.h"
#include "Utils.h"
#include "Crypt.h"
#include "Logger.h"

#include "cereal/archives/json.hpp"
#include "cereal/types/vector.hpp"
#include "cereal/types/string.hpp"
#include "cereal/types/chrono.hpp"
#include "cereal/external/rapidjson/error/en.h"

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
}

//////////////////////////////////////////////////////////////////////////

void Settings::process()
{
    m_db->process();
}

//////////////////////////////////////////////////////////////////////////

bool Settings::create(std::string const& name)
{
    m_dataName = "sense-" + name + ".settings";

    std::string dataFilename = s_dataFolder + "/" + m_dataName;
    moveToBackup(m_dataName, dataFilename, s_dataFolder + "/backups/deleted", 50);

    remove((dataFilename).c_str());

    m_mainData = Data();
    save(m_mainData);

    s_logger.logVerbose(QString("Creating settings file: '%1'").arg(m_dataName.c_str()));

    return true;
}

//////////////////////////////////////////////////////////////////////////

bool Settings::load(std::string const& name)
{
    Clock::time_point start = Clock::now();

    m_dataName = "sense-" + name + ".settings";

    std::string dataFilename = (s_dataFolder + "/" + m_dataName);

    Data data;

	std::string streamData;
	{
		std::ifstream file(dataFilename, std::ios_base::binary);
		if (!file.is_open())
		{
			s_logger.logCritical(QString("Failed to open '%1': %2").arg(dataFilename.c_str()).arg(std::strerror(errno)));
			return false;
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
        archive(cereal::make_nvp("settings", data));
	}
    catch (std::exception e)
    {
        try
		{
			std::istringstream stream(streamData);
			cereal::JSONInputArchive archive(stream);
            archive(cereal::make_nvp("settings", data));
		}
		catch (std::exception e)
		{
			s_logger.logCritical(QString("Failed to load '%1': %2").arg(dataFilename.c_str()).arg(e.what()));
			return false;
		}
    }

	for (const User& user : data.users)
	{
		data.lastUserId = std::max(data.lastUserId, user.id);
	}

    if (m_db->load() != success)
    {
        if (m_db->create() != success)
        {
            s_logger.logCritical(QString("Cannot open nor create the DB"));
            return false;
        }
    }

    //initialize backups
    std::pair<std::string, time_t> bkf = getMostRecentBackup(dataFilename, s_dataFolder + "/backups/daily");
    if (bkf.first.empty())
    {
        m_lastDailyBackupTP = Settings::Clock::now();
    }
    else
    {
        m_lastDailyBackupTP = Settings::Clock::from_time_t(bkf.second);
    }
    bkf = getMostRecentBackup(dataFilename, s_dataFolder + "/backups/weekly");
    if (bkf.first.empty())
    {
        m_lastWeeklyBackupTP = Settings::Clock::now();
    }
    else
    {
        m_lastWeeklyBackupTP = Settings::Clock::from_time_t(bkf.second);
    }

    //done!!!

    m_mainData = data;

    s_logger.logVerbose(QString("Done loading settings from '%1'. Time: %2s").arg(m_dataName.c_str()).arg(std::chrono::duration<float>(Clock::now() - start).count()));

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
    if (settings.from.empty())
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

bool Settings::setUser(UserId id, UserDescriptor const& descriptor)
{
    int32_t index = findUserIndexByName(descriptor.name);
    if (index >= 0 && getUser(static_cast<size_t>(index)).id != id)
    {
        return false;
    }

    index = findUserIndexById(id);
    if (index < 0)
    {
        return false;
    }

    m_mainData.users[static_cast<size_t>(index)].descriptor = descriptor;
    emit userChanged(id);

    s_logger.logInfo(QString("Changed user '%1'").arg(descriptor.name.c_str()));

    triggerSave();

    return true;
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

    //Clock::time_point start = now;

	std::stringstream jsonString;
	{
		cereal::JSONOutputArchive archive(jsonString);
		archive(cereal::make_nvp("settings", data));
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
			crypt.setCompressionLevel(1);
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

	if (!renameFile(tempFilename.c_str(), dataFilename.c_str()))
	{
		s_logger.logCritical(QString("Failed to rename file '%1' to '%2': %3").arg(tempFilename.c_str()).arg(dataFilename.c_str()).arg(getLastErrorAsString().c_str()));
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

