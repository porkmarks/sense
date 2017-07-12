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
}

//////////////////////////////////////////////////////////////////////////

void Emailer::shutdown()
{
    m_db = nullptr;
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
