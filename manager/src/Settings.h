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
#include "cereal/cereal.hpp"

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

		template<class Archive> void save(Archive& archive, std::uint32_t const version) const
		{
            int64_t seconds = std::chrono::duration_cast<std::chrono::seconds>(uploadBackupsPeriod).count();
			archive(CEREAL_NVP(host), CEREAL_NVP(port), CEREAL_NVP(username), CEREAL_NVP(password), CEREAL_NVP(folder), CEREAL_NVP(uploadBackups), cereal::make_nvp("uploadBackupsPeriod", seconds));
		}
		template<class Archive> void load(Archive& archive, std::uint32_t const version)
		{
            int64_t seconds;
			archive(CEREAL_NVP(host), CEREAL_NVP(port), CEREAL_NVP(username), CEREAL_NVP(password), CEREAL_NVP(folder), CEREAL_NVP(uploadBackups), cereal::make_nvp("uploadBackupsPeriod", seconds));
            uploadBackupsPeriod = std::chrono::seconds(std::max<int64_t>(seconds, 0));
		}
    };

    bool setFtpSettings(FtpSettings const& settings);
    FtpSettings const& getFtpSettings() const;

    ////////////////////////////////////////////////////////////////////////////

    struct UserDescriptor
    {
        std::string name;
        std::string passwordHash;

        enum Permissions
        {
            PermissionAddRemoveSensors = 1 << 0,
            PermissionChangeSensors = 1 << 1,

            PermissionAddRemoveBaseStations = 1 << 2,
            PermissionChangeBaseStations = 1 << 3,

            PermissionChangeMeasurements = 1 << 4,
            PermissionChangeEmailSettings = 1 << 5,
            PermissionChangeFtpSettings = 1 << 6,

            PermissionAddRemoveAlarms = 1 << 7,
            PermissionChangeAlarms = 1 << 8,

            PermissionAddRemoveReports = 1 << 9,
            PermissionChangeReports = 1 << 10,

            PermissionAddRemoveUsers = 1 << 11,
            PermissionChangeUsers = 1 << 12,
        };

        uint32_t permissions = 0;

        enum class Type
        {
            Admin, //has all the permissions
            Normal
        };
        Type type = Type::Normal;

		template<class Archive> void serialize(Archive& archive, std::uint32_t const version)
		{
            if (version == 0)
			{
				archive(CEREAL_NVP(name), CEREAL_NVP(passwordHash), CEREAL_NVP(type));
			}
            else
            {
				archive(CEREAL_NVP(name), CEREAL_NVP(passwordHash), CEREAL_NVP(permissions), CEREAL_NVP(type));
            }
		}
    };

    typedef uint32_t UserId;
    struct User
    {
        UserDescriptor descriptor;
        UserId id = 0;

		template<class Archive> void serialize(Archive& archive, std::uint32_t const version)
		{
			archive(CEREAL_NVP(descriptor), CEREAL_NVP(id));
		}
    };

    size_t getUserCount() const;
    User const& getUser(size_t index) const;
    int32_t findUserIndexByName(std::string const& name) const;
    int32_t findUserIndexById(UserId id) const;
    int32_t findUserIndexByPasswordHash(std::string const& passwordHash) const;
    bool addUser(UserDescriptor const& descriptor);
    Result<void> setUser(UserId id, UserDescriptor const& descriptor);
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
    friend class cereal::access;

    std::unique_ptr<DB> m_db;
    std::unique_ptr<Emailer> m_emailer;

    struct Data
    {
        EmailSettings emailSettings;
        FtpSettings ftpSettings;
        std::vector<User> users;
        UserId lastUserId = 0;

		template<class Archive> void serialize(Archive& archive, std::uint32_t const version)
		{
			archive(CEREAL_NVP(emailSettings), CEREAL_NVP(ftpSettings), CEREAL_NVP(users));
		}
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
