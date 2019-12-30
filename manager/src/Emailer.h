#pragma once

#include <string>
#include <chrono>
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <QTimer>

#include "DB.h"
#include "cereal/cereal.hpp"

class Settings;

class Emailer : public QObject
{
    Q_OBJECT
public:
    Emailer(Settings& settings, DB& db);
    ~Emailer();

    void sendReportEmail(DB::Report const& report);

    struct EmailSettings
    {
        enum class Connection
        {
            Ssl,
            Tls,
            Tcp,
        };

        std::string host;
        uint16_t port = 0;
        Connection connection = Connection::Ssl;
        std::string username;
        std::string password;
        std::string sender;
        std::vector<std::string> recipients;

		template<class Archive> void serialize(Archive& archive, std::uint32_t const version)
		{
			archive(CEREAL_NVP(host), CEREAL_NVP(port), CEREAL_NVP(connection), CEREAL_NVP(username), CEREAL_NVP(password), CEREAL_NVP(sender), CEREAL_NVP(recipients));
		}
    };

private slots:
    void alarmTriggersChanged(DB::AlarmId alarmId, DB::Measurement const& m, uint8_t oldTriggers, uint8_t newTriggers, uint8_t addedTriggers, uint8_t removedTriggers);

private:
    void checkReports();

    Settings& m_settings;
    DB& m_db;
    QTimer m_timer;

    struct Email
    {
        EmailSettings settings;
        std::string subject;
        std::string body;
        std::vector<std::string> files;
    };

    std::atomic_bool m_threadsExit = { false };

    enum class Action
    {
        Trigger,
        Recovery
    };

    void sendAlarmEmail(DB::Alarm const& alarm, DB::Sensor const& sensor, DB::Measurement const& m, uint8_t oldTriggers, uint8_t newTriggers, uint8_t triggers, Action action);
    void sendAlarmEmail(Email& email, DB::Alarm const& alarm, DB::Sensor const& sensor, DB::Measurement const& m);
    void sendEmail(Email const& email);
    void emailThreadProc();
    static void sendEmails(std::vector<Email> const& emails);
    std::thread m_emailThread;
    std::condition_variable m_emailCV;
    std::mutex m_emailMutex;
    std::vector<Email> m_emails;
};
