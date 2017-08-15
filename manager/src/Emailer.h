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
        enum class Connection
        {
            Tcp,
            Ssl,
            Tls
        };

        std::string host;
        uint16_t port = 0;
        Connection connection = Connection::Ssl;
        std::string username;
        std::string password;
        std::string from;
        std::vector<std::string> recipients;
    };

private slots:
    void alarmTriggered(DB::AlarmId alarmId, DB::SensorId sensorId, DB::MeasurementDescriptor const& md);
    void alarmUntriggered(DB::AlarmId alarmId, DB::SensorId sensorId, DB::MeasurementDescriptor const& md);

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

    void sendAlarmTriggeredEmail(DB::Alarm const& alarm, DB::Sensor const& sensor, DB::MeasurementDescriptor const& md);
    void sendAlarmUntriggeredEmail(DB::Alarm const& alarm, DB::Sensor const& sensor, DB::MeasurementDescriptor const& md);
    void sendAlarmEmail(Email& email, DB::Alarm const& alarm, DB::Sensor const& sensor, DB::MeasurementDescriptor const& md);
    void sendEmail(Email const& email);
    void emailThreadProc();
    static void sendEmails(std::vector<Email> const& emails);
    std::atomic_bool m_emailThreadTriggered = { false };
    std::thread m_emailThread;
    std::condition_variable m_emailCV;
    std::mutex m_emailMutex;
    std::vector<Email> m_emails;
};
