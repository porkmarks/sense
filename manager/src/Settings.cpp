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

#include "CRC.h"
#include "Utils.h"
#include "Crypt.h"

extern std::string s_programFolder;

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

bool Settings::create(std::string const& name)
{
    m_dataName = "sense-" + name + ".settings";

    QDir().mkpath((s_programFolder + "/data").c_str());

    moveToBackup(m_dataName, s_programFolder + "/data/" + m_dataName, s_programFolder + "/backups/deleted");

    remove((s_programFolder + "/data/" + m_dataName).c_str());

    return true;
}

//////////////////////////////////////////////////////////////////////////

bool Settings::load(std::string const& name)
{
    Clock::time_point start = Clock::now();

    m_dataName = "sense-" + name + ".settings";

    std::string dataFilename = (s_programFolder + "/data/" + m_dataName);

    Data data;

    {
        std::ifstream file(dataFilename);
        if (!file.is_open())
        {
            std::cerr << "Failed to open " << dataFilename << " file: " << std::strerror(errno) << "\n";
            return false;
        }

        rapidjson::Document document;
        rapidjson::BasicIStreamWrapper<std::ifstream> reader(file);
        document.ParseStream(reader);
        if (document.GetParseError() != rapidjson::ParseErrorCode::kParseErrorNone)
        {
            std::cerr << "Failed to open " << dataFilename << " file: " << rapidjson::GetParseErrorFunc(document.GetParseError()) << "\n";
            return false;
        }
        if (!document.IsObject())
        {
            std::cerr << "Bad document.\n";
            return false;
        }

        auto it = document.FindMember("email_settings");
        if (it == document.MemberEnd() || !it->value.IsObject())
        {
            std::cerr << "Bad or missing email_settings\n";
            return false;
        }

        {
            Crypt crypt;
            crypt.setKey(k_emailEncryptionKey);

            rapidjson::Value const& esj = it->value;
            auto it = esj.FindMember("host");
            if (it == esj.MemberEnd() || !it->value.IsString())
            {
                std::cerr << "Bad or missing email settings host\n";
                return false;
            }
            data.emailSettings.host = it->value.GetString();

            it = esj.FindMember("port");
            if (it == esj.MemberEnd() || !it->value.IsUint())
            {
                std::cerr << "Bad or missing email settings port\n";
                return false;
            }
            data.emailSettings.port = it->value.GetUint();

            it = esj.FindMember("username");
            if (it == esj.MemberEnd() || !it->value.IsString())
            {
                std::cerr << "Bad or missing email settings username\n";
                return false;
            }
            data.emailSettings.username = crypt.decryptToString(QString(it->value.GetString())).toUtf8().data();

            it = esj.FindMember("password");
            if (it == esj.MemberEnd() || !it->value.IsString())
            {
                std::cerr << "Bad or missing email settings password\n";
                return false;
            }
            data.emailSettings.password = crypt.decryptToString(QString(it->value.GetString())).toUtf8().data();

            it = esj.FindMember("from");
            if (it == esj.MemberEnd() || !it->value.IsString())
            {
                std::cerr << "Bad or missing email settings from\n";
                return false;
            }
            data.emailSettings.from = it->value.GetString();
        }

        it = document.FindMember("ftp_settings");
        if (it == document.MemberEnd() || !it->value.IsObject())
        {
            std::cerr << "Bad or missing ftp_settings\n";
            return false;
        }

        {
            Crypt crypt;
            crypt.setKey(k_ftpEncryptionKey);

            rapidjson::Value const& esj = it->value;
            auto it = esj.FindMember("host");
            if (it == esj.MemberEnd() || !it->value.IsString())
            {
                std::cerr << "Bad or missing ftp settings host\n";
                return false;
            }
            data.ftpSettings.host = it->value.GetString();

            it = esj.FindMember("port");
            if (it == esj.MemberEnd() || !it->value.IsUint())
            {
                std::cerr << "Bad or missing ftp settings port\n";
                return false;
            }
            data.ftpSettings.port = it->value.GetUint();

            it = esj.FindMember("username");
            if (it == esj.MemberEnd() || !it->value.IsString())
            {
                std::cerr << "Bad or missing ftp settings username\n";
                return false;
            }
            data.ftpSettings.username = crypt.decryptToString(QString(it->value.GetString())).toUtf8().data();

            it = esj.FindMember("password");
            if (it == esj.MemberEnd() || !it->value.IsString())
            {
                std::cerr << "Bad or missing ftp settings password\n";
                return false;
            }
            data.ftpSettings.password = crypt.decryptToString(QString(it->value.GetString())).toUtf8().data();
        }

        it = document.FindMember("users");
        if (it == document.MemberEnd() || !it->value.IsArray())
        {
            std::cerr << "Bad or missing users array\n";
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
                    std::cerr << "Bad or missing user name\n";
                    return false;
                }
                user.descriptor.name = it->value.GetString();

                it = userj.FindMember("password");
                if (it == userj.MemberEnd() || !it->value.IsString())
                {
                    std::cerr << "Bad or missing user password\n";
                    return false;
                }
                user.descriptor.passwordHash = it->value.GetString();

                it = userj.FindMember("id");
                if (it == userj.MemberEnd() || !it->value.IsUint())
                {
                    std::cerr << "Bad or missing user id\n";
                    return false;
                }
                user.id = it->value.GetUint();

                it = userj.FindMember("type");
                if (it == userj.MemberEnd() || !it->value.IsInt())
                {
                    std::cerr << "Bad or missing user type\n";
                    return false;
                }
                user.descriptor.type = static_cast<UserDescriptor::Type>(it->value.GetInt());

                data.lastUserId = std::max(data.lastUserId, user.id);
                data.users.push_back(user);
            }
        }

        file.close();
    }

    std::pair<std::string, time_t> bkf = getMostRecentBackup(dataFilename, s_programFolder + "/backups/daily");
    if (bkf.first.empty())
    {
        m_lastDailyBackupTP = Settings::Clock::now();
    }
    else
    {
        m_lastDailyBackupTP = Settings::Clock::from_time_t(bkf.second);
    }
    bkf = getMostRecentBackup(dataFilename, s_programFolder + "/backups/weekly");
    if (bkf.first.empty())
    {
        m_lastWeeklyBackupTP = Settings::Clock::now();
    }
    else
    {
        m_lastWeeklyBackupTP = Settings::Clock::from_time_t(bkf.second);
    }

    m_mainData = data;

    std::cout << "Time to load Data:" << std::chrono::duration<float>(Clock::now() - start).count() << "s\n";
    std::cout.flush();

    return true;
}

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

    triggerSave();

    return true;
}

//////////////////////////////////////////////////////////////////////////

bool Settings::setUser(UserId id, UserDescriptor const& descriptor)
{
    int32_t index = findUserIndexByName(descriptor.name);
    if (index >= 0 && getUser(index).id != id)
    {
        return false;
    }

    index = findUserIndexById(id);
    if (index < 0)
    {
        return false;
    }

    m_mainData.users[index].descriptor = descriptor;
    emit userChanged(id);

    triggerSave();

    return true;
}

//////////////////////////////////////////////////////////////////////////

void Settings::removeUser(size_t index)
{
    assert(index < m_mainData.users.size());
    UserId id = m_mainData.users[index].id;

    emit userWillBeRemoved(id);
    m_mainData.users.erase(m_mainData.users.begin() + index);
    emit userRemoved(id);

    triggerSave();
}

//////////////////////////////////////////////////////////////////////////

int32_t Settings::findUserIndexByName(std::string const& name) const
{
    auto it = std::find_if(m_mainData.users.begin(), m_mainData.users.end(), [&name](User const& user) { return user.descriptor.name == name; });
    if (it == m_mainData.users.end())
    {
        return -1;
    }
    return std::distance(m_mainData.users.begin(), it);
}

//////////////////////////////////////////////////////////////////////////

int32_t Settings::findUserIndexById(UserId id) const
{
    auto it = std::find_if(m_mainData.users.begin(), m_mainData.users.end(), [id](User const& user) { return user.id == id; });
    if (it == m_mainData.users.end())
    {
        return -1;
    }
    return std::distance(m_mainData.users.begin(), it);
}

//////////////////////////////////////////////////////////////////////////

int32_t Settings::findUserIndexByPasswordHash(std::string const& passwordHash) const
{
    auto it = std::find_if(m_mainData.users.begin(), m_mainData.users.end(), [&passwordHash](User const& user) { return user.descriptor.passwordHash == passwordHash; });
    if (it == m_mainData.users.end())
    {
        return -1;
    }
    return std::distance(m_mainData.users.begin(), it);
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
    if (findUserIndexById(id) >= 0)
    {
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
    int32_t index = findUserIndexById(m_loggedInUserId);
    if (index >= 0)
    {
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

    std::cout << "Save triggered\n";
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

    std::string dataFilename = (s_programFolder + "/data/" + m_dataName);

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
            QString username(data.emailSettings.username.c_str());
            esj.AddMember("username", rapidjson::Value(crypt.encryptToString(username).toUtf8().data(), document.GetAllocator()), document.GetAllocator());
            QString password(data.emailSettings.password.c_str());
            esj.AddMember("password", rapidjson::Value(crypt.encryptToString(password).toUtf8().data(), document.GetAllocator()), document.GetAllocator());
            esj.AddMember("from", rapidjson::Value(data.emailSettings.from.c_str(), document.GetAllocator()), document.GetAllocator());
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

        std::string tempFilename = (s_programFolder + "/data/" + m_dataName + "_temp");
        std::ofstream file(tempFilename);
        if (!file.is_open())
        {
            std::cerr << "Failed to open " << tempFilename << " file: " << std::strerror(errno) << "\n";
        }
        else
        {
            rapidjson::BasicOStreamWrapper<std::ofstream> buffer{file};
            //rapidjson::Writer<rapidjson::BasicOStreamWrapper<std::ofstream>> writer(buffer);
            rapidjson::PrettyWriter<rapidjson::BasicOStreamWrapper<std::ofstream>> writer(buffer);
            document.Accept(writer);
        }
        file.flush();
        file.close();

        copyToBackup(m_dataName, dataFilename, s_programFolder + "/backups/incremental");

        if (!renameFile(tempFilename.c_str(), dataFilename.c_str()))
        {
            std::cerr << "Error renaming files: " << getLastErrorAsString() << "\n";
        }
    }

    std::cout << "Time to save data:" << std::chrono::duration<float>(Clock::now() - start).count() << "s\n";
    std::cout.flush();

    if (dailyBackup)
    {
        copyToBackup(m_dataName, dataFilename, s_programFolder + "/backups/daily");
    }
    if (weeklyBackup)
    {
        copyToBackup(m_dataName, dataFilename, s_programFolder + "/backups/weekly");
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

