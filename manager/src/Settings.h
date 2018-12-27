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

    void process();
    bool create(std::string const& name);
    bool load(std::string const& name);

    DB const& getDB() const;
    DB& getDB();

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

    std::unique_ptr<DB> m_db;
    std::unique_ptr<Emailer> m_emailer;

    struct Data
    {
        EmailSettings emailSettings;
        FtpSettings ftpSettings;
        std::vector<User> users;
        UserId lastUserId = 0;
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
