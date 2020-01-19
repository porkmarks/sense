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
#include "Utils.h"

class Emailer : public QObject
{
    Q_OBJECT
public:
    Emailer(DB& db);
    ~Emailer();

    using EmailSettings = DB::EmailSettings;

    void sendReportEmail(DB::Report const& report, IClock::time_point from, IClock::time_point to);

private slots:
    void alarmSensorTriggersChanged(DB::AlarmId alarmId, DB::SensorId sensorId, std::optional<DB::Measurement> measurement, uint32_t oldTriggers, DB::AlarmTriggers triggers);
	void alarmStillTriggered(DB::AlarmId alarmId);
	void reportTriggered(DB::ReportId reportId, IClock::time_point from, IClock::time_point to);

private:
    DB& m_db;

    struct Email
    {
        EmailSettings settings;
        std::string subject;
        std::string body;

        struct Attachment
        {
            std::string filename;
            std::string contents;
        };
        std::vector<Attachment> attachments;
    };

    std::atomic_bool m_threadsExit = { false };

    enum class Action
    {
        Trigger,
        Recovery
    };

    void sendAlarmEmail(DB::Alarm const& alarm, DB::Sensor const& sensor, std::optional<DB::Measurement> measurement, uint32_t oldTriggers, uint32_t newTriggers, uint32_t triggers, Action action);
    void sendAlarmRetriggerEmail(DB::Alarm const& alarm);

	std::optional<utils::CsvData> getCsvData(std::vector<DB::Measurement> const& measurements, size_t index) const;


    void sendEmail(Email const& email);
    void emailThreadProc();
    static void sendEmails(std::vector<Email> const& emails);
    std::thread m_emailThread;
    std::condition_variable m_emailCV;
    std::mutex m_emailMutex;
    std::vector<Email> m_emails;
};
