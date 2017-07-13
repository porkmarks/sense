#include "Emailer.h"
#include <iostream>
#include <cassert>
#include "Smtp/SmtpMime"

extern float getBatteryLevel(float vcc);

//////////////////////////////////////////////////////////////////////////

Emailer::Emailer()
{
    m_emailThread = std::thread(std::bind(&Emailer::emailThreadProc, this));
}

//////////////////////////////////////////////////////////////////////////

Emailer::~Emailer()
{
    m_emailCV.notify_all();
    if (m_emailThread.joinable())
    {
        m_emailThread.join();
    }
}

//////////////////////////////////////////////////////////////////////////

void Emailer::init(DB& db)
{
    m_db = &db;

    connect(m_db, &DB::alarmWasTriggered, this, &Emailer::alarmTriggered);
    connect(m_db, &DB::alarmWasUntriggered, this, &Emailer::alarmUntriggered);

    m_timer.setSingleShot(false);
    m_timer.setInterval(60 * 1000); //every minute
    m_timer.start(1);
    connect(&m_timer, &QTimer::timeout, this, &Emailer::checkReports);
}

//////////////////////////////////////////////////////////////////////////

void Emailer::shutdown()
{
    m_db = nullptr;
}

//////////////////////////////////////////////////////////////////////////

void Emailer::checkReports()
{
    size_t count = m_db->getReportCount();
    for (size_t i = 0; i < count; i++)
    {
        DB::Report const& report = m_db->getReport(i);
        if (m_db->isReportTriggered(report.id))
        {
            if (report.descriptor.sendEmailAction && !report.descriptor.emailRecipient.empty())
            {
                sendReportEmail(report);
            }
            m_db->setReportExecuted(report.id);
        }
    }
}

//////////////////////////////////////////////////////////////////////////

void Emailer::alarmTriggered(DB::AlarmId alarmId, DB::SensorId sensorId, DB::MeasurementDescriptor const& md)
{
    int32_t alarmIndex = m_db->findAlarmIndexById(alarmId);
    int32_t sensorIndex = m_db->findSensorIndexById(sensorId);
    if (alarmIndex < 0 || sensorIndex < 0)
    {
        assert(false);
        return;
    }

    DB::Alarm const& alarm = m_db->getAlarm(alarmIndex);
    DB::Sensor const& sensor = m_db->getSensor(sensorIndex);

    if (alarm.descriptor.sendEmailAction && !alarm.descriptor.emailRecipient.empty())
    {
        sendAlarmTriggeredEmail(alarm, sensor, md);
    }
}

//////////////////////////////////////////////////////////////////////////

void Emailer::alarmUntriggered(DB::AlarmId alarmId, DB::SensorId sensorId, DB::MeasurementDescriptor const& md)
{
    int32_t alarmIndex = m_db->findAlarmIndexById(alarmId);
    int32_t sensorIndex = m_db->findSensorIndexById(sensorId);
    if (alarmIndex < 0 || sensorIndex < 0)
    {
        assert(false);
        return;
    }

    DB::Alarm const& alarm = m_db->getAlarm(alarmIndex);
    DB::Sensor const& sensor = m_db->getSensor(sensorIndex);

    if (alarm.descriptor.sendEmailAction && !alarm.descriptor.emailRecipient.empty())
    {
        sendAlarmUntriggeredEmail(alarm, sensor, md);
    }
}

//////////////////////////////////////////////////////////////////////////

void Emailer::sendAlarmTriggeredEmail(DB::Alarm const& alarm, DB::Sensor const& sensor, DB::MeasurementDescriptor const& md)
{
    Email email;
    email.subject = "Sensor '" + sensor.descriptor.name + "' triggered alarm '" + alarm.descriptor.name + "'";
    email.body = "<p>Sensor '<strong>" + sensor.descriptor.name + "</strong>' triggered alarm '<strong>" + alarm.descriptor.name + "</strong>'.</p>";

    sendAlarmEmail(email, alarm, sensor, md);
}

//////////////////////////////////////////////////////////////////////////

void Emailer::sendAlarmUntriggeredEmail(DB::Alarm const& alarm, DB::Sensor const& sensor, DB::MeasurementDescriptor const& md)
{
    Email email;
    email.subject = "Sensor '" + sensor.descriptor.name + "' stopped triggering alarm '" + alarm.descriptor.name + "'";
    email.body = "<p>Sensor '<strong>" + sensor.descriptor.name + "</strong>' stopped triggering alarm '<strong>" + alarm.descriptor.name + "</strong>'.</p>";

    sendAlarmEmail(email, alarm, sensor, md);
}

//////////////////////////////////////////////////////////////////////////

void Emailer::sendAlarmEmail(Email& email, DB::Alarm const& alarm, DB::Sensor const& sensor, DB::MeasurementDescriptor const& md)
{
    email.settings = m_db->getEmailSettings();
    email.to = alarm.descriptor.emailRecipient;

    QDateTime dt;
    dt.setTime_t(DB::Clock::to_time_t(md.timePoint));

    char buf[128];
    sprintf(buf, "%.2f", md.temperature);
    std::string temperature = buf;

    sprintf(buf, "%.2f", md.humidity);
    std::string humidity = buf;

    sprintf(buf, "%.0f", getBatteryLevel(md.vcc) * 100.f);
    std::string battery = buf;

    std::string sensorErrors;
    if (md.sensorErrors & DB::SensorErrors::Comms)
    {
        sensorErrors += "Comms";
    }
    if (md.sensorErrors & DB::SensorErrors::Hardware)
    {
        if (!sensorErrors.empty())
        {
            sensorErrors += ", ";
        }
        sensorErrors += "Hardware";
    }
    if (sensorErrors.empty())
    {
        sensorErrors = "None";
    }

    email.body +=
    "<p>Measurement:</p>"
    "<ul>"
    "<li>Temperature: <strong>" + temperature + " &deg;C</strong></li>"
    "<li>Humidity: <strong>" + humidity + " %RH</strong></li>"
    "<li>Sensor Errors: <strong>" + sensorErrors + "</strong></li>"
    "<li>Battery: <strong>" + battery + " %</strong></li>"
    "</ul>"
    "<p>Timestamp: <strong>" + std::string(dt.toString("dd-MM-yyyy HH:mm").toUtf8().data()) + "</strong> <span style=\"font-size: 8pt;\"><em>(dd-mm-yyyy hh:mm)</em></span></p>"
    "<p>&nbsp;</p>"
    "<p><span style=\"font-size: 10pt;\"><em>- Sense -</em></span></p>";

    sendEmail(email);
}

//////////////////////////////////////////////////////////////////////////

void Emailer::sendReportEmail(DB::Report const& report)
{
    Email email;
    email.settings = m_db->getEmailSettings();
    email.to = report.descriptor.emailRecipient;

    switch (report.descriptor.period)
    {
    case DB::ReportDescriptor::Period::Daily:
        email.subject = "Daily report: " + report.descriptor.name + "";
        break;
    case DB::ReportDescriptor::Period::Weekly:
        email.subject = "Weekly report: " + report.descriptor.name + "";
        break;
    case DB::ReportDescriptor::Period::Monthly:
        email.subject = "Monthly report: " + report.descriptor.name + "";
        break;
    case DB::ReportDescriptor::Period::Custom:
        email.subject = "Custom report: " + report.descriptor.name + "";
        break;
    }

    QDateTime startDt;
    startDt.setTime_t(DB::Clock::to_time_t(report.lastTriggeredTimePoint));
    QDateTime endDt = QDateTime::currentDateTime();

    email.body = QString(R"X(
<html>
<head>
<style type="text/css">
body
{
    line-height: 1.6em;
}

#hor-minimalist-a
{
    font-family: "Lucida Sans Unicode", "Lucida Grande", Sans-Serif;
    font-size: 12px;
    background: #fff;
    margin: 45px;
    width: 480px;
    border-collapse: collapse;
    text-align: left;
}
#hor-minimalist-a th
{
    font-size: 14px;
    font-weight: normal;
    color: #039;
    padding: 10px 8px;
    border-bottom: 2px solid #6678b1;
}
#hor-minimalist-a td
{
    color: #669;
    padding: 9px 8px 0px 8px;
}
#hor-minimalist-a tbody tr:hover td
{
    color: #009;
}
</style>
</head>
<body>
<p>This is an automated daily report of the <strong>%1</strong> base station.</p>
<p>Measurements between <strong>%2</strong> and&nbsp;<strong>%3</strong></p>
<table id="hor-minimalist-a">
<tbody>
<tr>
<th style="width: 80.6667px; text-align: center; white-space: nowrap;"><strong>Sensor</strong></th>
<th style="width: 80.6667px; text-align: center; white-space: nowrap;"><strong>Timestamp</strong></th>
<th style="width: 80.6667px; text-align: center; white-space: nowrap;"><strong>Temperature</strong></th>
<th style="width: 80.6667px; text-align: center; white-space: nowrap;"><strong>Humidity</strong></th>
<th style="width: 81.3333px; text-align: center; white-space: nowrap;"><strong>Alerts</strong></th>
</tr>)X").arg(m_db->getSensorSettings().descriptor.name.c_str())
         .arg(startDt.toString("dd-MM-yyyy HH:mm"))
         .arg(endDt.toString("dd-MM-yyyy HH:mm"))
         .toUtf8().data();

    DB::Filter filter;
    filter.useTimePointFilter = true;
    filter.timePointFilter.min = report.lastTriggeredTimePoint;
    filter.timePointFilter.max = DB::Clock::now();
    std::vector<DB::Measurement> measurements = m_db->getFilteredMeasurements(filter);
    std::sort(measurements.begin(), measurements.end(), [](DB::Measurement const& a, DB::Measurement const& b) { return a.descriptor.timePoint > b.descriptor.timePoint; });

    for (DB::Measurement const& m: measurements)
    {
        QDateTime dt;
        dt.setTime_t(DB::Clock::to_time_t(m.descriptor.timePoint));
        std::string sensorName = "N/A";
        int32_t sensorIndex = m_db->findSensorIndexById(m.descriptor.sensorId);
        if (sensorIndex >= 0)
        {
            sensorName = m_db->getSensor(sensorIndex).descriptor.name;
        }
        email.body += QString(R"X(
<tr>
<td style="width: 80.6667px;">%1</td>
<td style="width: 80.6667px; text-align: right; white-space: nowrap;">%2</td>
<td style="width: 80.6667px; text-align: right; white-space: nowrap;">%3&nbsp;&deg;C</td>
<td style="width: 80.6667px; text-align: right; white-space: nowrap;">%4 %RH</td>
<td style="width: 81.3333px; text-align: right; white-space: nowrap;">%5</td>
</tr>)X").arg(sensorName.c_str())
         .arg(dt.toString("dd-MM-yyyy HH:mm"))
         .arg(m.descriptor.temperature, 0, 'f', 1)
         .arg(m.descriptor.humidity, 0, 'f', 1)
         .arg(m.triggeredAlarms == 0 ? "no" : "yes")
         .toUtf8().data();
    }
    email.body += R"X(
</tbody>
</table>
<p>&nbsp;</p>
<p>&nbsp;</p>
<p><em>- Sense -</em></p>
</body>
</html>)X";

    sendEmail(email);
}

//////////////////////////////////////////////////////////////////////////

void Emailer::sendEmail(Email const& email)
{
    {
        std::unique_lock<std::mutex> lg(m_emailMutex);
        m_emails.push_back(email);
        m_emailThreadTriggered = true;
    }
    m_emailCV.notify_all();

    std::cout << "Email triggered\n";
}

//////////////////////////////////////////////////////////////////////////

void Emailer::sendEmails(std::vector<Email> const& emails)
{
    for (Email const& email: emails)
    {
        SmtpClient smtp(QString::fromUtf8(email.settings.host.c_str()), email.settings.port, SmtpClient::SslConnection);

        std::string errorMsg;
        connect(&smtp, &SmtpClient::smtpError, [&errorMsg](SmtpClient::SmtpError error)
        {
            switch (error)
            {
            case SmtpClient::ConnectionTimeoutError: errorMsg = "Connection timeout."; break;
            case SmtpClient::ResponseTimeoutError: errorMsg = "Response timeout."; break;
            case SmtpClient::SendDataTimeoutError: errorMsg = "Send data timeout."; break;
            case SmtpClient::AuthenticationFailedError: errorMsg = "Authentication failed."; break;
            case SmtpClient::ServerError: errorMsg = "Server error."; break;
            case SmtpClient::ClientError: errorMsg = "Client error."; break;
            default: errorMsg = "Unknown error."; break;
            }
        });

        smtp.setUser(QString::fromUtf8(email.settings.username.c_str()));
        smtp.setPassword(QString::fromUtf8(email.settings.password.c_str()));

        MimeMessage message;

        message.setSender(new EmailAddress(QString::fromUtf8(email.settings.from.c_str())));
        message.addRecipient(new EmailAddress(QString::fromUtf8(email.to.c_str())));
        message.setSubject(QString::fromUtf8(email.subject.c_str()));

        MimeHtml body;
        body.setHtml(QString::fromUtf8(email.body.c_str()));

        message.addPart(&body);

        if (smtp.connectToHost() && smtp.login() && smtp.sendMail(message))
        {
            std::cout << "Sent email\n";
        }
        else
        {
            std::cout << "Failed to send email: " << errorMsg << "\n";
        }
        std::cout.flush();
        smtp.quit();

//        QStringList files;
//        for (std::string const& f: email.files)
//        {
//            files.append(QString::fromUtf8(f.c_str()));
//        }
//        smtp.sendHtmlMail(QString::fromUtf8(email.settings.from.c_str()),
//                          QString::fromUtf8(email.to.c_str()),
//                          QString::fromUtf8(email.subject.c_str()),
//                          QString::fromUtf8(email.body.c_str()),
//                          files);
    }
}

//////////////////////////////////////////////////////////////////////////

void Emailer::emailThreadProc()
{
    while (!m_threadsExit)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        std::vector<Email> emails;
        {
            //wait for data
            std::unique_lock<std::mutex> lg(m_emailMutex);
            if (!m_emailThreadTriggered)
            {
                m_emailCV.wait(lg, [this]{ return m_emailThreadTriggered || m_threadsExit; });
            }
            if (m_threadsExit)
            {
                break;
            }

            emails = m_emails;
            m_emails.clear();
            m_emailThreadTriggered = false;
        }

        sendEmails(emails);
    }
}

//////////////////////////////////////////////////////////////////////////
