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
        enum class Connection : uint8_t
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
    };

private slots:
    void alarmTriggersChanged(DB::AlarmId alarmId, DB::Measurement const& m, uint32_t oldTriggers, uint32_t newTriggers, uint32_t addedTriggers, uint32_t removedTriggers);
	void alarmStillTriggered(DB::AlarmId alarmId);

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

    void sendAlarmEmail(DB::Alarm const& alarm, DB::Sensor const& sensor, DB::Measurement const& m, uint32_t oldTriggers, uint32_t newTriggers, uint32_t triggers, Action action);
    void sendAlarmRetriggerEmail(DB::Alarm const& alarm);
    void sendEmail(Email const& email);
    void emailThreadProc();
    static void sendEmails(std::vector<Email> const& emails);
    std::thread m_emailThread;
    std::condition_variable m_emailCV;
    std::mutex m_emailMutex;
    std::vector<Email> m_emails;
};
