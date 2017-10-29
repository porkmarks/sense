#pragma once

#include <QObject>
#include <chrono>
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <thread>
#include <atomic>
#include <array>
#include <condition_variable>
#include <QHostAddress>

#include "DB.h"
#include "Emailer.h"

class Settings : public QObject
{
    Q_OBJECT
public:
    Settings();
    ~Settings();

    typedef std::chrono::system_clock Clock;

    bool create(std::string const& name);
    bool load(std::string const& name);

    ////////////////////////////////////////////////////////////////////////////

    struct BaseStationDescriptor
    {
        typedef std::array<uint8_t, 6> Mac;

        std::string name;
        Mac mac;
        //QHostAddress address;
    };

    typedef uint32_t BaseStationId;
    struct BaseStation
    {
        BaseStationDescriptor descriptor;
        BaseStationId id;
    };

    size_t getBaseStationCount() const;
    BaseStation const& getBaseStation(size_t index) const;
    int32_t findBaseStationIndexByName(std::string const& name) const;
    int32_t findBaseStationIndexById(BaseStationId id) const;
    int32_t findBaseStationIndexByMac(BaseStationDescriptor::Mac const& mac) const;
    bool addBaseStation(BaseStationDescriptor const& descriptor);
    bool setBaseStation(BaseStationId id, BaseStationDescriptor const& descriptor);
    void removeBaseStation(size_t index);
    DB const& getBaseStationDB(size_t index) const;
    DB& getBaseStationDB(size_t index);

    void setActiveBaseStationId(BaseStationId id);
    BaseStationId getActiveBaseStationId() const;

    ////////////////////////////////////////////////////////////////////////////

    typedef Emailer::EmailSettings EmailSettings;

    bool setEmailSettings(EmailSettings const& settings);
    EmailSettings const& getEmailSettings() const;

    ////////////////////////////////////////////////////////////////////////////

    struct FtpSettings
    {
        std::string host;
        uint16_t port = 0;
        std::string username;
        std::string password;
        std::string folder;
        bool uploadBackups = false;
        Clock::duration uploadBackupsPeriod = std::chrono::hours(24);
    };

    bool setFtpSettings(FtpSettings const& settings);
    FtpSettings const& getFtpSettings() const;

    ////////////////////////////////////////////////////////////////////////////

    struct UserDescriptor
    {
        std::string name;
        std::string passwordHash;
        enum class Type
        {
            Admin,
            Normal
        };
        Type type = Type::Normal;
    };

    typedef uint32_t UserId;
    struct User
    {
        UserDescriptor descriptor;
        UserId id;
    };

    size_t getUserCount() const;
    User const& getUser(size_t index) const;
    int32_t findUserIndexByName(std::string const& name) const;
    int32_t findUserIndexById(UserId id) const;
    int32_t findUserIndexByPasswordHash(std::string const& passwordHash) const;
    bool addUser(UserDescriptor const& descriptor);
    bool setUser(UserId id, UserDescriptor const& descriptor);
    void removeUser(size_t index);
    bool needsAdmin() const;
    void setLoggedInUserId(UserId id);
    UserId getLoggedInUserId() const;
    User const* getLoggedInUser() const;
    bool isLoggedInAsAdmin() const;

    ////////////////////////////////////////////////////////////////////////////

signals:
    void baseStationWillBeAdded(BaseStationId id);
    void baseStationAdded(BaseStationId id);
    void baseStationWillBeRemoved(BaseStationId id);
    void baseStationRemoved(BaseStationId id);
    void baseStationChanged(BaseStationId id);
    void baseStationActivated(BaseStationId id);
    void baseStationDeactivated(BaseStationId id);

    void emailSettingsWillBeChanged();
    void emailSettingsChanged();

    void ftpSettingsWillBeChanged();
    void ftpSettingsChanged();

    void userWillBeAdded(UserId id);
    void userAdded(UserId id);
    void userWillBeRemoved(UserId id);
    void userRemoved(UserId id);
    void userChanged(UserId id);
    void userLoggedIn(UserId id);

private:

    std::vector<std::unique_ptr<DB>> m_dbs;
    std::vector<std::unique_ptr<Emailer>> m_emailers;

    struct Data
    {
        EmailSettings emailSettings;
        FtpSettings ftpSettings;
        std::vector<BaseStation> baseStations;
        std::vector<User> users;
        BaseStationId lastBaseStationId = 0;
        UserId lastUserId = 0;
        BaseStationId activeBaseStationId = 0;
    };

    Data m_mainData;
    UserId m_loggedInUserId = UserId(-1);

    std::atomic_bool m_threadsExit = { false };

    void triggerSave();
    void storeThreadProc();
    void save(Data const& data) const;
    std::atomic_bool m_storeThreadTriggered = { false };
    std::thread m_storeThread;
    std::condition_variable m_storeCV;
    std::mutex m_storeMutex;
    Data m_storeData;

    std::string m_dataName;

    mutable Clock::time_point m_lastDailyBackupTP = Clock::time_point(Clock::duration::zero());
    mutable Clock::time_point m_lastWeeklyBackupTP = Clock::time_point(Clock::duration::zero());
};
