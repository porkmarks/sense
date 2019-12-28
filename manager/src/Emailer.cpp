#include "Emailer.h"
#include <iostream>
#include <cassert>
#include "Smtp/SmtpMime"
#include "Settings.h"
#include "Logger.h"
#include "Utils.h"

extern Logger s_logger;

//////////////////////////////////////////////////////////////////////////

//static std::string getSensorErrors(uint8_t errors)
//{
//    std::string sensorErrors;
//    if (errors & DB::SensorErrors::Comms)
//    {
//        sensorErrors += "Comms";
//    }
//    if (errors & DB::SensorErrors::Hardware)
//    {
//        if (!sensorErrors.empty())
//        {
//            sensorErrors += ", ";
//        }
//        sensorErrors += "Hardware";
//    }
//    if (sensorErrors.empty())
//    {
//        sensorErrors = "None";
//    }
//    return sensorErrors;
//}

//////////////////////////////////////////////////////////////////////////

Emailer::Emailer(Settings& settings, DB& db)
    : m_settings(settings)
    , m_db(db)
{
    m_emailThread = std::thread(std::bind(&Emailer::emailThreadProc, this));

    connect(&m_db, &DB::alarmTriggersChanged, this, &Emailer::alarmTriggersChanged);

    m_timer.setSingleShot(false);
    m_timer.setInterval(60 * 1000); //every minute
    m_timer.start();
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
            sendReportEmail(report);
            m_db.setReportExecuted(report.id);
        }
    }
}

//////////////////////////////////////////////////////////////////////////

void Emailer::alarmTriggersChanged(DB::AlarmId alarmId, DB::Measurement const& m, uint8_t oldTriggers, uint8_t newTriggers, uint8_t addedTriggers, uint8_t removedTriggers)
{
    int32_t alarmIndex = m_db.findAlarmIndexById(alarmId);
    int32_t sensorIndex = m_db.findSensorIndexById(m.descriptor.sensorId);
    if (alarmIndex < 0 || sensorIndex < 0)
    {
        assert(false);
        return;
    }

    DB::Alarm const& alarm = m_db.getAlarm(static_cast<size_t>(alarmIndex));
    DB::Sensor const& sensor = m_db.getSensor(static_cast<size_t>(sensorIndex));

    if (alarm.descriptor.sendEmailAction)
    {
        if (removedTriggers != 0)
        {
            sendAlarmEmail(alarm, sensor, m, oldTriggers, newTriggers, removedTriggers, Action::Recovery);
        }
        if (addedTriggers != 0)
        {
            sendAlarmEmail(alarm, sensor, m, oldTriggers, newTriggers, addedTriggers, Action::Trigger);
        }
    }
}

//////////////////////////////////////////////////////////////////////////

void Emailer::sendAlarmEmail(DB::Alarm const& alarm, DB::Sensor const& sensor, DB::Measurement const& m, uint8_t oldTriggers, uint8_t newTriggers, uint8_t triggers, Action action)
{
    auto toString = [](uint8_t triggers)
    {
        std::string str;
        str += (triggers & DB::AlarmTrigger::Temperature) ? "Temperature, " : "";
        str += (triggers & DB::AlarmTrigger::Humidity) ? "Humidity, " : "";
        str += (triggers & DB::AlarmTrigger::LowVcc) ? "Battery, " : "";
        str += (triggers & DB::AlarmTrigger::LowSignal) ? "Signal, " : "";
        if (!str.empty())
        {
            str.pop_back(); //' '
            str.pop_back(); //','
        }
        return str;
    };
    auto getColor = [action, triggers, oldTriggers](uint8_t trigger)
    {
        return (action == Action::Trigger && (triggers & trigger)) ? "red" :
                (action == Action::Recovery && (triggers & trigger)) ? "green" :
                (oldTriggers & trigger) ? "orange" :
                                                                      "black";
    };

    //this returns:
    //      "triggered" for new triggers
    //      "back to normal" when a trigger dies
    //      "" for old triggers
    //      "normal" for untriggered stuff
    auto getStatus = [action, triggers, oldTriggers](uint8_t trigger)
    {
        return (action == Action::Trigger && (triggers & trigger)) ? "triggered now" :
                (action == Action::Recovery && (triggers & trigger)) ? "back to normal" :
               (oldTriggers & trigger) ? "still triggered" :
                                         "normal";
    };


    Email email;

    ///////////////////////////////
    // SUBJECT
    if (action == Action::Trigger)
    {
        email.subject = "ALARM - Sensor '" + sensor.descriptor.name + "' triggered alarm '" + alarm.descriptor.name + "': " + toString(triggers);
    }
    else
    {
        email.subject = "RECOVERY - Sensor '" + sensor.descriptor.name + "' recovered from alarm '" + alarm.descriptor.name + "': " + toString(triggers);
    }

    ///////////////////////////////
    // BODY
    email.body = "<p>Sensor '<strong>" + sensor.descriptor.name + "</strong>' / alarm '<strong>" + alarm.descriptor.name + "</strong>'.</p>";

    email.settings = m_settings.getEmailSettings();

    s_logger.logInfo(QString("Sending alarm email'"));

    QDateTime dt;
    dt.setTime_t(DB::Clock::to_time_t(m.timePoint));

    email.body += QString(R"X(
                          <p>Measurement:</p>
                          <ul>
                          <li><span style="color:%1">Temperature: <strong>%2 &deg;C</strong> (%3)</span></li>
                          <li><span style="color:%4">Humidity: <strong>%5 %RH</strong> (%6)</span></li>
                          <li><span style="color:%7">Battery: <strong>%8 %</strong> (%9)</span></li>
                          <li><span style="color:%10">Signal: <strong>%11 %</strong> (%12)</span></li>
                          </ul>
                          <p>Timestamp: <strong>%13</strong> <span style="font-size: 8pt;"><em>(dd-mm-yyyy hh:mm)</em></span></p>
                          <p>&nbsp;</p>
                          <p><span style="font-size: 10pt;"><em>- Sense -</em></span></p>)X")
            .arg(getColor(DB::AlarmTrigger::Temperature))
            .arg(m.descriptor.temperature, 0, 'f', 1)
            .arg(getStatus(DB::AlarmTrigger::Temperature))

            .arg(getColor(DB::AlarmTrigger::Humidity))
            .arg(m.descriptor.humidity, 0, 'f', 1)
            .arg(getStatus(DB::AlarmTrigger::Humidity))

            .arg(getColor(DB::AlarmTrigger::LowVcc))
            .arg(static_cast<int>(getBatteryLevel(m.descriptor.vcc)*100.f))
            .arg(getStatus(DB::AlarmTrigger::LowVcc))

            .arg(getColor(DB::AlarmTrigger::LowSignal))
            .arg(static_cast<int>(getSignalLevel(m.combinedSignalStrength)*100.f))
            .arg(getStatus(DB::AlarmTrigger::LowSignal))

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
    filter.useSensorFilter = report.descriptor.filterSensors;
    filter.sensorIds = report.descriptor.sensors;
    std::vector<DB::Measurement> measurements = m_db.getFilteredMeasurements(filter);
    std::sort(measurements.begin(), measurements.end(), [](DB::Measurement const& a, DB::Measurement const& b) { return a.timePoint > b.timePoint; });

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
                          <p>This is an automated daily report</p>
                          <p>Measurements between <strong>%1</strong> and&nbsp;<strong>%2</strong></p>)X")
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
            bool hasData = false;
            std::string name;
            float minTemperature = std::numeric_limits<float>::max();
            float maxTemperature = std::numeric_limits<float>::lowest();
            float minHumidity = std::numeric_limits<float>::max();
            float maxHumidity = std::numeric_limits<float>::lowest();
            uint8_t alarmTriggers = 0;
        };

        std::vector<SensorData> sensorDatas;
        for (DB::Measurement const& m: measurements)
        {
            QDateTime dt;
            dt.setTime_t(DB::Clock::to_time_t(m.timePoint));
            std::string sensorName = "N/A";
            int32_t _sensorIndex = m_db.findSensorIndexById(m.descriptor.sensorId);
            if (_sensorIndex < 0)
            {
                continue;
            }
            size_t sensorIndex = static_cast<size_t>(_sensorIndex);
            if (sensorIndex >= sensorDatas.size())
            {
                sensorDatas.resize(sensorIndex + 1);
            }
            SensorData& sd = sensorDatas[sensorIndex];
            sd.hasData = true;
            sd.name = m_db.getSensor(sensorIndex).descriptor.name;
            sd.maxTemperature = std::max(sd.maxTemperature, m.descriptor.temperature);
            sd.minTemperature = std::min(sd.minTemperature, m.descriptor.temperature);
            sd.maxHumidity = std::max(sd.maxHumidity, m.descriptor.humidity);
            sd.minHumidity = std::min(sd.minHumidity, m.descriptor.humidity);
            sd.alarmTriggers |= m.alarmTriggers;
        }

        for (SensorData const& sd: sensorDatas)
        {
            if (sd.hasData)
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
                        .arg(sd.alarmTriggers == 0 ? "no" : "yes")
                        .toUtf8().data();
            }
            else
            {
                email.body += QString(R"X(
                                      <tr>
                                      <td style="width: 80.6667px;">%1</td>
                                      <td style="width: 80.6667px; text-align: right; white-space: nowrap;">N/A</td>
                                      <td style="width: 80.6667px; text-align: right; white-space: nowrap;">N/A</td>
                                      <td style="width: 80.6667px; text-align: right; white-space: nowrap;">N/A</td>
                                      <td style="width: 81.3333px; text-align: right; white-space: nowrap;">N/A</td>
                                      <td style="width: 81.3333px; text-align: right; white-space: nowrap;">N/A</td>
                                      </tr>
                                      )X")
                        .arg(sd.name.c_str())
                        .toUtf8().data();
            }
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
            dt.setTime_t(DB::Clock::to_time_t(m.timePoint));
            std::string sensorName = "N/A";
            int32_t sensorIndex = m_db.findSensorIndexById(m.descriptor.sensorId);
            if (sensorIndex >= 0)
            {
                sensorName = m_db.getSensor(static_cast<size_t>(sensorIndex)).descriptor.name;
            }
            email.body += QString(R"X(
                                  <tr>
                                      <td style="width: 80.6667px;">%1</td>
                                      <td style="width: 80.6667px; text-align: right; white-space: nowrap;">%2</td>
                                      <td style="width: 80.6667px; text-align: right; white-space: nowrap;">%3 &deg;C</td>
                                      <td style="width: 80.6667px; text-align: right; white-space: nowrap;">%4 %RH</td>
                                      <td style="width: 81.3333px; text-align: right; white-space: nowrap;">%5</td>
                                  </tr>)X").arg(sensorName.c_str())
                    .arg(dt.toString("dd-MM-yyyy HH:mm"))
                    .arg(m.descriptor.temperature, 0, 'f', 1)
                    .arg(m.descriptor.humidity, 0, 'f', 1)
                    .arg(m.alarmTriggers == 0 ? "no" : "yes")
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
    }
    m_emailCV.notify_all();
}

//////////////////////////////////////////////////////////////////////////

void Emailer::sendEmails(std::vector<Email> const& emails)
{
    for (Email const& email: emails)
    {
        if (email.settings.recipients.empty())
        {
            s_logger.logCritical(QString("Failed to send email: no recipients configured"));
            continue;
        }

        SmtpClient::ConnectionType connectionType = SmtpClient::SslConnection;
        switch (email.settings.connection)
        {
        case EmailSettings::Connection::Ssl: connectionType = SmtpClient::SslConnection; break;
        case EmailSettings::Connection::Tcp: connectionType = SmtpClient::TcpConnection; break;
        case EmailSettings::Connection::Tls: connectionType = SmtpClient::TlsConnection; break;
        }

        SmtpClient smtp(QString::fromUtf8(email.settings.host.c_str()), email.settings.port, connectionType);

        std::string errorMsg;
        connect(&smtp, &SmtpClient::smtpError, [&errorMsg, &smtp](SmtpClient::SmtpError error)
        {
            switch (error)
            {
            case SmtpClient::ConnectionTimeoutError: errorMsg = "Connection timeout."; break;
            case SmtpClient::ResponseTimeoutError: errorMsg = "Response timeout."; break;
            case SmtpClient::SendDataTimeoutError: errorMsg = "Send data timeout."; break;
            case SmtpClient::AuthenticationFailedError: errorMsg = "Authentication failed."; break;
            case SmtpClient::ServerError: errorMsg = std::string("Server error: ") + smtp.getResponseMessage().toUtf8().data(); break;
            case SmtpClient::ClientError: errorMsg = std::string("Client error: ") + smtp.getResponseMessage().toUtf8().data(); break;
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
            s_logger.logCritical(QString("Failed to send email: %2").arg(errorMsg.c_str()));
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
            if (m_emails.empty())
            {
                m_emailCV.wait(lg, [this]{ return !m_emails.empty() || m_threadsExit; });
            }
            if (m_threadsExit)
            {
                break;
            }

            emails = m_emails;
            m_emails.clear();
        }

        sendEmails(emails);
    }
}

//////////////////////////////////////////////////////////////////////////
