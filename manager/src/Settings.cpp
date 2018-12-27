#include "Settings.h"
#include <algorithm>
#include <functional>
#include <cassert>
#include <fstream>
#include <iostream>
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

#include "CRC.h"
#include "Utils.h"
#include "Crypt.h"
#include "Logger.h"

#ifdef _MSC_VER
//not #if defined(_WIN32) || defined(_WIN64) because we have strncasecmp in mingw
#define strncasecmp _strnicmp
#define strcasecmp _stricmp
#endif

extern Logger s_logger;

extern std::string s_dataFolder;

constexpr uint64_t k_fileEncryptionKey = 32455153254365ULL;
constexpr uint64_t k_emailEncryptionKey = 1735271834639209ULL;
constexpr uint64_t k_ftpEncryptionKey = 45235321231234ULL;

//////////////////////////////////////////////////////////////////////////

Settings::Settings()
{
    m_storeThread = std::thread(std::bind(&Settings::storeThreadProc, this));
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
        }

        Crypt crypt;
        crypt.setKey(k_fileEncryptionKey);
        QByteArray decryptedData = crypt.decryptToByteArray(QByteArray(streamData.data(), int32_t(streamData.size())));

        rapidjson::Document document;
        document.Parse(decryptedData.data(), decryptedData.size());
        if (document.GetParseError() != rapidjson::ParseErrorCode::kParseErrorNone)
        {
            document.Parse(streamData.data(), streamData.size());
            if (document.GetParseError() != rapidjson::ParseErrorCode::kParseErrorNone)
            {
                s_logger.logCritical(QString("Failed to load '%1': %2").arg(dataFilename.c_str()).arg(rapidjson::GetParseError_En(document.GetParseError())));
                return false;
            }
        }
        if (!document.IsObject())
        {
            s_logger.logCritical(QString("Failed to load '%1': Bad document").arg(dataFilename.c_str()));
            return false;
        }

        auto it = document.FindMember("email_settings");
        if (it == document.MemberEnd() || !it->value.IsObject())
        {
            s_logger.logCritical(QString("Failed to load '%1': Bad or missing email settings").arg(dataFilename.c_str()));
            return false;
        }

        {
            Crypt crypt;
            crypt.setKey(k_emailEncryptionKey);

            rapidjson::Value const& esj = it->value;
            auto it = esj.FindMember("host");
            if (it == esj.MemberEnd() || !it->value.IsString())
            {
                s_logger.logCritical(QString("Failed to load '%1': Bad or missing email settings host").arg(dataFilename.c_str()));
                return false;
            }
            data.emailSettings.host = it->value.GetString();

            it = esj.FindMember("port");
            if (it == esj.MemberEnd() || !it->value.IsUint())
            {
                s_logger.logCritical(QString("Failed to load '%1': Bad or missing email settings port").arg(dataFilename.c_str()));
                return false;
            }
            data.emailSettings.port = static_cast<uint16_t>(it->value.GetUint());

            it = esj.FindMember("connection");
            if (it == esj.MemberEnd() || !it->value.IsUint())
            {
                s_logger.logCritical(QString("Failed to load '%1': Bad or missing email settings connection").arg(dataFilename.c_str()));
                return false;
            }
            data.emailSettings.connection = static_cast<EmailSettings::Connection>(it->value.GetUint());

            it = esj.FindMember("username");
            if (it == esj.MemberEnd() || !it->value.IsString())
            {
                s_logger.logCritical(QString("Failed to load '%1': Bad or missing email settings username").arg(dataFilename.c_str()));
                return false;
            }
            data.emailSettings.username = crypt.decryptToString(QString(it->value.GetString())).toUtf8().data();

            it = esj.FindMember("password");
            if (it == esj.MemberEnd() || !it->value.IsString())
            {
                s_logger.logCritical(QString("Failed to load '%1': Bad or missing email settings password").arg(dataFilename.c_str()));
                return false;
            }
            data.emailSettings.password = crypt.decryptToString(QString(it->value.GetString())).toUtf8().data();

            it = esj.FindMember("from");
            if (it == esj.MemberEnd() || !it->value.IsString())
            {
                s_logger.logCritical(QString("Failed to load '%1': Bad or missing email settings from").arg(dataFilename.c_str()));
                return false;
            }
            data.emailSettings.from = it->value.GetString();

            it = esj.FindMember("recipients");
            if (it == esj.MemberEnd() || !it->value.IsArray())
            {
                s_logger.logCritical(QString("Failed to load '%1': Bad or missing email settings recipients").arg(dataFilename.c_str()));
                return false;
            }

            {
                rapidjson::Value const& recipientsj = it->value;
                for (size_t i = 0; i < recipientsj.Size(); i++)
                {
                    rapidjson::Value const& recipientj = recipientsj[i];
                    if (!recipientj.IsString())
                    {
                        s_logger.logCritical(QString("Failed to load '%1': Bad or missing email settings secipient").arg(dataFilename.c_str()));
                        return false;
                    }
                    data.emailSettings.recipients.push_back(recipientj.GetString());
                }
            }
        }

        it = document.FindMember("ftp_settings");
        if (it == document.MemberEnd() || !it->value.IsObject())
        {
            s_logger.logCritical(QString("Failed to load '%1': Bad or missing ftp_settings").arg(dataFilename.c_str()));
            return false;
        }

        {
            Crypt crypt;
            crypt.setKey(k_ftpEncryptionKey);

            rapidjson::Value const& esj = it->value;
            auto it = esj.FindMember("host");
            if (it == esj.MemberEnd() || !it->value.IsString())
            {
                s_logger.logCritical(QString("Failed to load '%1': Bad or missing ftp settings host").arg(dataFilename.c_str()));
                return false;
            }
            data.ftpSettings.host = it->value.GetString();

            it = esj.FindMember("port");
            if (it == esj.MemberEnd() || !it->value.IsUint())
            {
                s_logger.logCritical(QString("Failed to load '%1': Bad or missing ftp settings port").arg(dataFilename.c_str()));
                return false;
            }
            data.ftpSettings.port = static_cast<uint16_t>(it->value.GetUint());

            it = esj.FindMember("username");
            if (it == esj.MemberEnd() || !it->value.IsString())
            {
                s_logger.logCritical(QString("Failed to load '%1': Bad or missing ftp settings username").arg(dataFilename.c_str()));
                return false;
            }
            data.ftpSettings.username = crypt.decryptToString(QString(it->value.GetString())).toUtf8().data();

            it = esj.FindMember("password");
            if (it == esj.MemberEnd() || !it->value.IsString())
            {
                s_logger.logCritical(QString("Failed to load '%1': Bad or missing ftp settings password").arg(dataFilename.c_str()));
                return false;
            }
            data.ftpSettings.password = crypt.decryptToString(QString(it->value.GetString())).toUtf8().data();

            it = esj.FindMember("folder");
            if (it == esj.MemberEnd() || !it->value.IsString())
            {
                s_logger.logCritical(QString("Failed to load '%1': Bad or missing ftp settings folder").arg(dataFilename.c_str()));
                return false;
            }
            data.ftpSettings.folder = crypt.decryptToString(QString(it->value.GetString())).toUtf8().data();

            it = esj.FindMember("upload_backups");
            if (it == esj.MemberEnd() || !it->value.IsBool())
            {
                s_logger.logCritical(QString("Failed to load '%1': Bad or missing ftp settings upload_backups").arg(dataFilename.c_str()));
                return false;
            }
            data.ftpSettings.uploadBackups = it->value.GetBool();

            it = esj.FindMember("upload_backups_period");
            if (it == esj.MemberEnd() || !it->value.IsUint64())
            {
                s_logger.logCritical(QString("Failed to load '%1': Bad or missing ftp settings upload_backups_period").arg(dataFilename.c_str()));
                return false;
            }
            data.ftpSettings.uploadBackupsPeriod = std::chrono::seconds(it->value.GetUint64());
        }

        it = document.FindMember("users");
        if (it == document.MemberEnd() || !it->value.IsArray())
        {
            s_logger.logCritical(QString("Failed to load '%1': Bad or missing users array").arg(dataFilename.c_str()));
            return false;
        }

        {
            rapidjson::Value const& usersj = it->value;
            for (size_t i = 0; i < usersj.Size(); i++)
            {
                User user;
                rapidjson::Value const& userj = usersj[i];
                auto it = userj.FindMember("name");
                if (it == userj.MemberEnd() || !it->value.IsString())
                {
                    s_logger.logCritical(QString("Failed to load '%1': Bad or missing user name").arg(dataFilename.c_str()));
                    return false;
                }
                user.descriptor.name = it->value.GetString();

                it = userj.FindMember("password");
                if (it == userj.MemberEnd() || !it->value.IsString())
                {
                    s_logger.logCritical(QString("Failed to load '%1': Bad or missing user password").arg(dataFilename.c_str()));
                    return false;
                }
                user.descriptor.passwordHash = it->value.GetString();

                it = userj.FindMember("id");
                if (it == userj.MemberEnd() || !it->value.IsUint())
                {
                    s_logger.logCritical(QString("Failed to load '%1': Bad or missing user id").arg(dataFilename.c_str()));
                    return false;
                }
                user.id = it->value.GetUint();

                it = userj.FindMember("type");
                if (it == userj.MemberEnd() || !it->value.IsInt())
                {
                    s_logger.logCritical(QString("Failed to load '%1': Bad or missing user type").arg(dataFilename.c_str()));
                    return false;
                }
                user.descriptor.type = static_cast<UserDescriptor::Type>(it->value.GetInt());

                data.lastUserId = std::max(data.lastUserId, user.id);
                data.users.push_back(user);
            }
        }
    }

    std::unique_ptr<DB> db(new DB());
    if (db->load() != success)
    {
        if (db->create() != success)
        {
            s_logger.logCritical(QString("Cannot open nor create the DB"));
            return false;
        }
    }
    m_db = std::move(db);
    m_emailer.reset(new Emailer(*this, *m_db));

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

    Clock::time_point start = now;

    {
        rapidjson::Document document;
        document.SetObject();
        {
            Crypt crypt;
            crypt.setKey(k_emailEncryptionKey);
            rapidjson::Value esj;
            esj.SetObject();
            esj.AddMember("host", rapidjson::Value(data.emailSettings.host.c_str(), document.GetAllocator()), document.GetAllocator());
            esj.AddMember("port", static_cast<uint32_t>(data.emailSettings.port), document.GetAllocator());
            esj.AddMember("connection", static_cast<uint32_t>(data.emailSettings.connection), document.GetAllocator());
            QString username(data.emailSettings.username.c_str());
            esj.AddMember("username", rapidjson::Value(crypt.encryptToString(username).toUtf8().data(), document.GetAllocator()), document.GetAllocator());
            QString password(data.emailSettings.password.c_str());
            esj.AddMember("password", rapidjson::Value(crypt.encryptToString(password).toUtf8().data(), document.GetAllocator()), document.GetAllocator());
            esj.AddMember("from", rapidjson::Value(data.emailSettings.from.c_str(), document.GetAllocator()), document.GetAllocator());

            {
                rapidjson::Value recipientsj;
                recipientsj.SetArray();
                for (std::string const& recipient: data.emailSettings.recipients)
                {
                    recipientsj.PushBack(rapidjson::Value(recipient.c_str(), document.GetAllocator()), document.GetAllocator());
                }
                esj.AddMember("recipients", recipientsj, document.GetAllocator());
            }

            document.AddMember("email_settings", esj, document.GetAllocator());
        }
        {
            Crypt crypt;
            crypt.setKey(k_ftpEncryptionKey);
            rapidjson::Value fsj;
            fsj.SetObject();
            fsj.AddMember("host", rapidjson::Value(data.ftpSettings.host.c_str(), document.GetAllocator()), document.GetAllocator());
            fsj.AddMember("port", static_cast<uint32_t>(data.ftpSettings.port), document.GetAllocator());
            QString username(data.ftpSettings.username.c_str());
            fsj.AddMember("username", rapidjson::Value(crypt.encryptToString(username).toUtf8().data(), document.GetAllocator()), document.GetAllocator());
            QString password(data.ftpSettings.password.c_str());
            fsj.AddMember("password", rapidjson::Value(crypt.encryptToString(password).toUtf8().data(), document.GetAllocator()), document.GetAllocator());
            fsj.AddMember("folder", rapidjson::Value(data.ftpSettings.folder.c_str(), document.GetAllocator()), document.GetAllocator());
            fsj.AddMember("upload_backups", rapidjson::Value(data.ftpSettings.uploadBackups), document.GetAllocator());
            fsj.AddMember("upload_backups_period", static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(data.ftpSettings.uploadBackupsPeriod).count()), document.GetAllocator());
            document.AddMember("ftp_settings", fsj, document.GetAllocator());
        }

        {
            rapidjson::Value usersj;
            usersj.SetArray();
            for (User const& user: data.users)
            {
                rapidjson::Value userj;
                userj.SetObject();
                userj.AddMember("id", user.id, document.GetAllocator());
                userj.AddMember("name", rapidjson::Value(user.descriptor.name.c_str(), document.GetAllocator()), document.GetAllocator());
                userj.AddMember("password", rapidjson::Value(user.descriptor.passwordHash.c_str(), document.GetAllocator()), document.GetAllocator());
                userj.AddMember("type", static_cast<int>(user.descriptor.type), document.GetAllocator());
                usersj.PushBack(userj, document.GetAllocator());
            }
            document.AddMember("users", usersj, document.GetAllocator());
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

    s_logger.logVerbose(QString("Done saving settings to '%1'. Time: %2s").arg(dataFilename.c_str()).arg(std::chrono::duration<float>(Clock::now() - start).count()));

    if (dailyBackup)
    {
        copyToBackup(m_dataName, dataFilename, s_dataFolder + "/backups/daily", 10);
    }
    if (weeklyBackup)
    {
        copyToBackup(m_dataName, dataFilename, s_dataFolder + "/backups/weekly", 10);
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

