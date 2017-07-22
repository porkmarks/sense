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

        it = document.FindMember("base_stations");
        if (it == document.MemberEnd() || !it->value.IsArray())
        {
            std::cerr << "Bad or missing base_stations array\n";
            return false;
        }

        {
            rapidjson::Value const& bssj = it->value;
            for (size_t i = 0; i < bssj.Size(); i++)
            {
                BaseStation bs;
                rapidjson::Value const& bsj = bssj[i];
                auto it = bsj.FindMember("name");
                if (it == bsj.MemberEnd() || !it->value.IsString())
                {
                    std::cerr << "Bad or missing base station name\n";
                    return false;
                }
                bs.descriptor.name = it->value.GetString();

                it = bsj.FindMember("address");
                if (it == bsj.MemberEnd() || !it->value.IsString())
                {
                    std::cerr << "Bad or missing base station address\n";
                    return false;
                }
                bs.descriptor.address = QHostAddress(it->value.GetString());

                it = bsj.FindMember("id");
                if (it == bsj.MemberEnd() || !it->value.IsUint())
                {
                    std::cerr << "Bad or missing base station id\n";
                    return false;
                }
                bs.id = it->value.GetUint();

                it = bsj.FindMember("mac");
                if (it == bsj.MemberEnd() || !it->value.IsString())
                {
                    std::cerr << "Bad or missing base station mac\n";
                    return false;
                }

                int m0, m1, m2, m3, m4, m5;
                if (sscanf(it->value.GetString(), "%X:%X:%X:%X:%X:%X", &m0, &m1, &m2, &m3, &m4, &m5) != 6)
                {
                    std::cerr << "Bad base station mac\n";
                    return false;
                }
                bs.descriptor.mac = { static_cast<uint8_t>(m0&0xFF), static_cast<uint8_t>(m1&0xFF), static_cast<uint8_t>(m2&0xFF),
                            static_cast<uint8_t>(m3&0xFF), static_cast<uint8_t>(m4&0xFF), static_cast<uint8_t>(m5&0xFF) };

                data.lastBaseStationId = std::max(data.lastBaseStationId, bs.id);
                data.baseStations.push_back(bs);
            }
        }


        it = document.FindMember("active_base_station");
        if (it == document.MemberEnd() || !it->value.IsUint())
        {
            std::cerr << "Bad or missing active_base_station\n";
            return false;
        }
        BaseStationId id = it->value.GetUint();
        {
            auto it = std::find_if(data.baseStations.begin(), data.baseStations.end(), [id](BaseStation const& baseStation) { return baseStation.id == id; });
            if (it == m_mainData.baseStations.end())
            {
                std::cerr << "Cannot find active base station id\n";
                return false;
            }
        }
        data.activeBaseStationId = id;

        file.close();
    }

    //load the databases
    m_dbs.clear();
    for (BaseStation& bs: data.baseStations)
    {
        BaseStationDescriptor::Mac& mac = bs.descriptor.mac;
        char macStr[128];
        sprintf(macStr, "%X_%X_%X_%X_%X_%X", mac[0]&0xFF, mac[1]&0xFF, mac[2]&0xFF, mac[3]&0xFF, mac[4]&0xFF, mac[5]&0xFF);
        std::string dbName = macStr;

        std::unique_ptr<DB> db(new DB());
        if (!db->load(dbName))
        {
            if (!db->create(dbName))
            {
                std::cerr << (QString("Cannot open nor create a DB for Base Station '%1' (%2)\n").arg(bs.descriptor.name.c_str()).arg(macStr)).toUtf8().data();
                return false;
            }
        }
        m_dbs.push_back(std::move(db));
    }
    m_emailers.emplace_back(new Emailer(*this, *m_dbs.back()));

    //initialize backups
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

    //done!!!

    m_mainData = data;

    std::cout << "Time to load Data:" << std::chrono::duration<float>(Clock::now() - start).count() << "s\n";
    std::cout.flush();

    return true;
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

size_t Settings::getBaseStationCount() const
{
    return m_mainData.baseStations.size();
}

//////////////////////////////////////////////////////////////////////////

Settings::BaseStation const& Settings::getBaseStation(size_t index) const
{
    assert(index < m_mainData.baseStations.size());
    return m_mainData.baseStations[index];
}

//////////////////////////////////////////////////////////////////////////

bool Settings::addBaseStation(BaseStationDescriptor const& descriptor)
{
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
    if (!db->load(dbName))
    {
        if (!db->create(dbName))
        {
            std::cerr << (QString("Cannot open nor create a DB for Base Station '%1' (%2)\n").arg(descriptor.name.c_str()).arg(macStr)).toUtf8().data();
            return false;
        }
    }

    BaseStation baseStation;
    baseStation.descriptor = descriptor;
    baseStation.id = ++m_mainData.lastBaseStationId;

    emit baseStationWillBeAdded(baseStation.id);
    m_mainData.baseStations.push_back(baseStation);
    m_dbs.push_back(std::move(db));
    m_emailers.emplace_back(new Emailer(*this, *m_dbs.back()));
    emit baseStationAdded(baseStation.id);

    triggerSave();

    return true;
}

//////////////////////////////////////////////////////////////////////////

bool Settings::setBaseStation(BaseStationId id, BaseStationDescriptor const& descriptor)
{
    int32_t index = findBaseStationIndexByName(descriptor.name);
    if (index >= 0 && getBaseStation(index).id != id)
    {
        return false;
    }
    index = findBaseStationIndexByMac(descriptor.mac);
    if (index >= 0 && getBaseStation(index).id != id)
    {
        return false;
    }

    index = findBaseStationIndexById(id);
    if (index < 0)
    {
        return false;
    }

    m_mainData.baseStations[index].descriptor = descriptor;
    emit baseStationChanged(id);

    triggerSave();

    return true;
}

//////////////////////////////////////////////////////////////////////////

void Settings::removeBaseStation(size_t index)
{
    assert(index < m_mainData.baseStations.size());
    BaseStationId id = m_mainData.baseStations[index].id;

    emit baseStationWillBeRemoved(id);
    m_mainData.baseStations.erase(m_mainData.baseStations.begin() + index);
    m_dbs.erase(m_dbs.begin() + index);
    m_emailers.erase(m_emailers.begin() + index);
    emit baseStationRemoved(id);

    triggerSave();
}

//////////////////////////////////////////////////////////////////////////

int32_t Settings::findBaseStationIndexByName(std::string const& name) const
{
    auto it = std::find_if(m_mainData.baseStations.begin(), m_mainData.baseStations.end(), [&name](BaseStation const& baseStation) { return baseStation.descriptor.name == name; });
    if (it == m_mainData.baseStations.end())
    {
        return -1;
    }
    return std::distance(m_mainData.baseStations.begin(), it);
}

//////////////////////////////////////////////////////////////////////////

int32_t Settings::findBaseStationIndexById(BaseStationId id) const
{
    auto it = std::find_if(m_mainData.baseStations.begin(), m_mainData.baseStations.end(), [id](BaseStation const& baseStation) { return baseStation.id == id; });
    if (it == m_mainData.baseStations.end())
    {
        return -1;
    }
    return std::distance(m_mainData.baseStations.begin(), it);
}

//////////////////////////////////////////////////////////////////////////

int32_t Settings::findBaseStationIndexByMac(BaseStationDescriptor::Mac const& mac) const
{
    auto it = std::find_if(m_mainData.baseStations.begin(), m_mainData.baseStations.end(), [&mac](BaseStation const& baseStation) { return baseStation.descriptor.mac == mac; });
    if (it == m_mainData.baseStations.end())
    {
        return -1;
    }
    return std::distance(m_mainData.baseStations.begin(), it);
}

//////////////////////////////////////////////////////////////////////////

DB const& Settings::getBaseStationDB(size_t index) const
{
    return *m_dbs[index];
}

//////////////////////////////////////////////////////////////////////////

DB& Settings::getBaseStationDB(size_t index)
{
    return *m_dbs[index];
}

//////////////////////////////////////////////////////////////////////////

void Settings::setActiveBaseStationId(BaseStationId id)
{
    if (m_mainData.activeBaseStationId > 0)
    {
        emit baseStationDeactivated(m_mainData.activeBaseStationId);
    }

    if (findBaseStationIndexById(id) >= 0)
    {
        m_mainData.activeBaseStationId = id;
    }
    else
    {
        m_mainData.activeBaseStationId = 0;
    }

    if (m_mainData.activeBaseStationId > 0)
    {
        emit baseStationActivated(m_mainData.activeBaseStationId);
    }

    triggerSave();
}

//////////////////////////////////////////////////////////////////////////

Settings::BaseStationId Settings::getActiveBaseStationId() const
{
    return m_mainData.activeBaseStationId;
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

        {
            rapidjson::Value bssj;
            bssj.SetArray();
            for (BaseStation const& baseStation: data.baseStations)
            {
                rapidjson::Value bsj;
                bsj.SetObject();
                bsj.AddMember("id", baseStation.id, document.GetAllocator());
                bsj.AddMember("name", rapidjson::Value(baseStation.descriptor.name.c_str(), document.GetAllocator()), document.GetAllocator());
                bsj.AddMember("address", rapidjson::Value(baseStation.descriptor.address.toString().toUtf8().data(), document.GetAllocator()), document.GetAllocator());

                BaseStationDescriptor::Mac const& mac = baseStation.descriptor.mac;
                char macStr[128];
                sprintf(macStr, "%X:%X:%X:%X:%X:%X", mac[0]&0xFF, mac[1]&0xFF, mac[2]&0xFF, mac[3]&0xFF, mac[4]&0xFF, mac[5]&0xFF);
                bsj.AddMember("mac", rapidjson::Value(macStr, document.GetAllocator()), document.GetAllocator());
                bssj.PushBack(bsj, document.GetAllocator());
            }
            document.AddMember("base_stations", bssj, document.GetAllocator());
        }
        document.AddMember("active_base_station", rapidjson::Value(data.activeBaseStationId), document.GetAllocator());

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

