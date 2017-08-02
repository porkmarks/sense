#include "Emailer.h"
#include <iostream>
#include <cassert>
#include "Smtp/SmtpMime"
#include "Settings.h"
#include "Logger.h"

extern Logger s_logger;

extern float getBatteryLevel(float vcc);


//////////////////////////////////////////////////////////////////////////

static std::string getSensorErrors(uint8_t errors)
{
    std::string sensorErrors;
    if (errors & DB::SensorErrors::Comms)
    {
        sensorErrors += "Comms";
    }
    if (errors & DB::SensorErrors::Hardware)
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
    return sensorErrors;
}

//////////////////////////////////////////////////////////////////////////

Emailer::Emailer(Settings& settings, DB& db)
    : m_settings(settings)
    , m_db(db)
{
    m_emailThread = std::thread(std::bind(&Emailer::emailThreadProc, this));

    connect(&m_db, &DB::alarmWasTriggered, this, &Emailer::alarmTriggered);
    connect(&m_db, &DB::alarmWasUntriggered, this, &Emailer::alarmUntriggered);

    m_timer.setSingleShot(false);
    m_timer.setInterval(60 * 1000); //every minute
    m_timer.start(1);
    connect(&m_timer, &QTimer::timeout, this, &Emailer::checkReports);
}

//////////////////////////////////////////////////////////////////////////

Emailer::~Emailer()
{
    m_threadsExit = true;
    m_emailCV.notify_all();
    if (m_emailThread.joinable())
    {
        m_emailThread.join();
    }
}

//////////////////////////////////////////////////////////////////////////

void Emailer::checkReports()
{
    size_t count = m_db.getReportCount();
    for (size_t i = 0; i < count; i++)
    {
        DB::Report const& report = m_db.getReport(i);
        if (m_db.isReportTriggered(report.id))
        {
            s_logger.logInfo(QString("Report '%1' triggered").arg(report.descriptor.name.c_str()));
            if (report.descriptor.sendEmailAction)
            {
                sendReportEmail(report);
            }
            m_db.setReportExecuted(report.id);
        }
    }
}

//////////////////////////////////////////////////////////////////////////

void Emailer::alarmTriggered(DB::AlarmId alarmId, DB::SensorId sensorId, DB::MeasurementDescriptor const& md)
{
    int32_t alarmIndex = m_db.findAlarmIndexById(alarmId);
    int32_t sensorIndex = m_db.findSensorIndexById(sensorId);
    if (alarmIndex < 0 || sensorIndex < 0)
    {
        assert(false);
        return;
    }

    DB::Alarm const& alarm = m_db.getAlarm(alarmIndex);
    DB::Sensor const& sensor = m_db.getSensor(sensorIndex);

    if (alarm.descriptor.sendEmailAction)
    {
        sendAlarmTriggeredEmail(alarm, sensor, md);
    }
}

//////////////////////////////////////////////////////////////////////////

void Emailer::alarmUntriggered(DB::AlarmId alarmId, DB::SensorId sensorId, DB::MeasurementDescriptor const& md)
{
    int32_t alarmIndex = m_db.findAlarmIndexById(alarmId);
    int32_t sensorIndex = m_db.findSensorIndexById(sensorId);
    if (alarmIndex < 0 || sensorIndex < 0)
    {
        assert(false);
        return;
    }

    DB::Alarm const& alarm = m_db.getAlarm(alarmIndex);
    DB::Sensor const& sensor = m_db.getSensor(sensorIndex);

    if (alarm.descriptor.sendEmailAction)
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
    email.settings = m_settings.getEmailSettings();

    s_logger.logInfo(QString("Sending alarm email'"));

    QDateTime dt;
    dt.setTime_t(DB::Clock::to_time_t(md.timePoint));

    email.body += QString(R"X(
                          <p>Measurement:</p>
                          <ul>
                          <li>Temperature: <strong>%1 &deg;C</strong></li>
                          <li>Humidity: <strong>%2 %RH</strong></li>
                          <li>Sensor Errors: <strong>%3</strong></li>
                          <li>Battery: <strong>%4 %</strong></li>
                          </ul>
                          <p>Timestamp: <strong>%5</strong> <span style="font-size: 8pt;"><em>(dd-mm-yyyy hh:mm)</em></span></p>
                          <p>&nbsp;</p>
                          <p><span style="font-size: 10pt;"><em>- Sense -</em></span></p>)X")
            .arg(md.temperature, 0, 'f', 1)
            .arg(md.humidity, 0, 'f', 1)
            .arg(getSensorErrors(md.sensorErrors).c_str())
            .arg(static_cast<int>(getBatteryLevel(md.vcc)*100.f))
            .arg(dt.toString("dd-MM-yyyy HH:mm"))
            .toUtf8().data();

    sendEmail(email);
}

//////////////////////////////////////////////////////////////////////////

void Emailer::sendReportEmail(DB::Report const& report)
{
    Email email;
    email.settings = m_settings.getEmailSettings();

    s_logger.logInfo(QString("Sending report email"));

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

    DB::Filter filter;
    filter.useTimePointFilter = true;
    filter.timePointFilter.min = report.lastTriggeredTimePoint;
    filter.timePointFilter.max = DB::Clock::now();
    std::vector<DB::Measurement> measurements = m_db.getFilteredMeasurements(filter);
    std::sort(measurements.begin(), measurements.end(), [](DB::Measurement const& a, DB::Measurement const& b) { return a.descriptor.timePoint > b.descriptor.timePoint; });

    email.body += QString(R"X(
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
                          <p>Measurements between <strong>%2</strong> and&nbsp;<strong>%3</strong></p>)X")
                            .arg(m_db.getSensorSettings().descriptor.name.c_str())
                            .arg(startDt.toString("dd-MM-yyyy HH:mm"))
                            .arg(endDt.toString("dd-MM-yyyy HH:mm"))
                            .toUtf8().data();


    if (report.descriptor.data == DB::ReportDescriptor::Data::Summary ||
            report.descriptor.data == DB::ReportDescriptor::Data::All)
    {
        email.body += QString(R"X(
                              <table id="hor-minimalist-a">
                              <tbody>
                                  <tr>
                                      <th style="width: 80.6667px; text-align: center; white-space: nowrap;"><strong>Sensor</strong></th>
                                      <th style="width: 80.6667px; text-align: center; white-space: nowrap;"><strong>Min Temperature</strong></th>
                                      <th style="width: 80.6667px; text-align: center; white-space: nowrap;"><strong>Max Temperature</strong></th>
                                      <th style="width: 80.6667px; text-align: center; white-space: nowrap;"><strong>Min Humidity</strong></th>
                                      <th style="width: 81.3333px; text-align: center; white-space: nowrap;"><strong>Max Humidity</strong></th>
                                      <th style="width: 81.3333px; text-align: center; white-space: nowrap;"><strong>Alerts</strong></th>
                                  </tr>)X").toUtf8().data();

                struct SensorData
        {
                std::string name;
                float minTemperature = std::numeric_limits<float>::max();
                float maxTemperature = std::numeric_limits<float>::lowest();
                float minHumidity = std::numeric_limits<float>::max();
                float maxHumidity = std::numeric_limits<float>::lowest();
                uint8_t sensorErrors = 0;
    };

        std::vector<SensorData> sensorDatas;
        for (DB::Measurement const& m: measurements)
        {
            QDateTime dt;
            dt.setTime_t(DB::Clock::to_time_t(m.descriptor.timePoint));
            std::string sensorName = "N/A";
            int32_t sensorIndex = m_db.findSensorIndexById(m.descriptor.sensorId);
            if (sensorIndex < 0)
            {
                continue;
            }
            if (static_cast<size_t>(sensorIndex) >= sensorDatas.size())
            {
                sensorDatas.resize(sensorIndex + 1);
            }
            SensorData& sd = sensorDatas[sensorIndex];
            sd.name = m_db.getSensor(sensorIndex).descriptor.name;
            sd.maxTemperature = std::max(sd.maxTemperature, m.descriptor.temperature);
            sd.minTemperature = std::min(sd.minTemperature, m.descriptor.temperature);
            sd.maxHumidity = std::max(sd.maxHumidity, m.descriptor.humidity);
            sd.minHumidity = std::min(sd.minHumidity, m.descriptor.humidity);
            sd.sensorErrors |= m.descriptor.sensorErrors;
        }

        for (SensorData const& sd: sensorDatas)
        {
            email.body += QString(R"X(
                                  <tr>
                                      <td style="width: 80.6667px;">%1</td>
                                      <td style="width: 80.6667px; text-align: right; white-space: nowrap;">%2 &deg;C</td>
                                      <td style="width: 80.6667px; text-align: right; white-space: nowrap;">%3 &deg;C</td>
                                      <td style="width: 80.6667px; text-align: right; white-space: nowrap;">%4 %RH</td>
                                      <td style="width: 81.3333px; text-align: right; white-space: nowrap;">%5 %RH</td>
                                      <td style="width: 81.3333px; text-align: right; white-space: nowrap;">%6</td>
                                  </tr>
                                  )X")
                    .arg(sd.name.c_str())
                    .arg(sd.minTemperature, 0, 'f', 1)
                    .arg(sd.maxTemperature, 0, 'f', 1)
                    .arg(sd.minHumidity, 0, 'f', 1)
                    .arg(sd.maxHumidity, 0, 'f', 1)
                    .arg(getSensorErrors(sd.sensorErrors).c_str())
                    .toUtf8().data();
        }

        email.body += "</tbody>"
                      "</table>";
    }

    if (report.descriptor.data == DB::ReportDescriptor::Data::All)
    {
        email.body += QString(R"X(
                              <table id="hor-minimalist-a">
                              <tbody>
                                  <tr>
                                      <th style="width: 80.6667px; text-align: center; white-space: nowrap;"><strong>Sensor</strong></th>
                                      <th style="width: 80.6667px; text-align: center; white-space: nowrap;"><strong>Timestamp</strong></th>
                                      <th style="width: 80.6667px; text-align: center; white-space: nowrap;"><strong>Temperature</strong></th>
                                      <th style="width: 80.6667px; text-align: center; white-space: nowrap;"><strong>Humidity</strong></th>
                                      <th style="width: 81.3333px; text-align: center; white-space: nowrap;"><strong>Alerts</strong></th>
                                  </tr>)X").toUtf8().data();

        for (DB::Measurement const& m: measurements)
        {
            QDateTime dt;
            dt.setTime_t(DB::Clock::to_time_t(m.descriptor.timePoint));
            std::string sensorName = "N/A";
            int32_t sensorIndex = m_db.findSensorIndexById(m.descriptor.sensorId);
            if (sensorIndex >= 0)
            {
                sensorName = m_db.getSensor(sensorIndex).descriptor.name;
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
                      <p><em>- Sense -</em></p>
                      </body>
                      </html>)X";
    }

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
        for (std::string const& recipient: email.settings.recipients)
        {
            message.addRecipient(new EmailAddress(QString::fromUtf8(recipient.c_str())));
        }
        message.setSubject(QString::fromUtf8(email.subject.c_str()));

        MimeHtml body;
        body.setHtml(QString::fromUtf8(email.body.c_str()));

        message.addPart(&body);

        if (smtp.connectToHost() && smtp.login() && smtp.sendMail(message))
        {
            s_logger.logInfo(QString("Successfully sent email"));
        }
        else
        {
            s_logger.logError(QString("Failed to send email: %2").arg(errorMsg.c_str()));
        }
        smtp.quit();
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
