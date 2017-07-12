#include "EmailSettingsDialog.h"
#include "Smtp/SmtpMime"

//////////////////////////////////////////////////////////////////////////

EmailSettingsDialog::EmailSettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    m_ui.setupUi(this);
    connect(m_ui.sendTestEmail, &QPushButton::released, this, &EmailSettingsDialog::sendTestEmail);
}

//////////////////////////////////////////////////////////////////////////

EmailSettingsDialog::~EmailSettingsDialog()
{
}

//////////////////////////////////////////////////////////////////////////

void EmailSettingsDialog::setEmailSettings(DB::EmailSettings const& settings)
{
    m_settings = settings;

    m_ui.host->setText(settings.host.c_str());
    m_ui.port->setValue(settings.port);
    m_ui.username->setText(settings.username.c_str());
    m_ui.password->setText(settings.password.c_str());
    m_ui.from->setText(settings.from.c_str());
}

//////////////////////////////////////////////////////////////////////////

DB::EmailSettings const& EmailSettingsDialog::getEmailSettings() const
{
    return m_settings;
}

//////////////////////////////////////////////////////////////////////////

bool EmailSettingsDialog::getSettings(DB::EmailSettings& settings)
{
    settings.host = m_ui.host->text().toUtf8().data();
    settings.port = static_cast<uint16_t>(m_ui.port->value());
    settings.username = m_ui.username->text().toUtf8().data();
    settings.password = m_ui.password->text().toUtf8().data();
    settings.from = m_ui.from->text().toUtf8().data();

    if (settings.host.empty())
    {
        QMessageBox::critical(this, "Error", "You need to specify a valid host.");
        return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////

void EmailSettingsDialog::accept()
{
    DB::EmailSettings settings;
    if (!getSettings(settings))
    {
        return;
    }

    m_settings = settings;
    QDialog::accept();
}

//////////////////////////////////////////////////////////////////////////

void EmailSettingsDialog::sendTestEmail()
{
    DB::EmailSettings settings;
    if (!getSettings(settings))
    {
        return;
    }
    if (m_ui.testEmailTo->text().isEmpty())
    {
        QMessageBox::critical(this, "Error", "You need to specify a test email recipient.");
        return;
    }

    SmtpClient smtp(QString::fromUtf8(settings.host.c_str()), settings.port, SmtpClient::SslConnection);

    bool hasError = false;
    connect(&smtp, &SmtpClient::smtpError, [this, &hasError](SmtpClient::SmtpError error)
    {
        switch (error)
        {
        case SmtpClient::ConnectionTimeoutError:
            QMessageBox::critical(this, "Error", "Connection timeout.");
            break;
        case SmtpClient::ResponseTimeoutError:
            QMessageBox::critical(this, "Error", "Response timeout.");
            break;
        case SmtpClient::SendDataTimeoutError:
            QMessageBox::critical(this, "Error", "Send data timeout.");
            break;
        case SmtpClient::AuthenticationFailedError:
            QMessageBox::critical(this, "Error", "Authentication failed.");
            break;
        case SmtpClient::ServerError:
            QMessageBox::critical(this, "Error", "Server error.");
            break;
        case SmtpClient::ClientError:
            QMessageBox::critical(this, "Error", "Client error.");
            break;
        default:
            QMessageBox::critical(this, "Error", "Unknown error.");
            break;
        }

        hasError = true;
    });

    smtp.setUser(QString::fromUtf8(settings.username.c_str()));
    smtp.setPassword(QString::fromUtf8(settings.password.c_str()));

    MimeMessage message;

    message.setSender(new EmailAddress(QString::fromUtf8(settings.from.c_str())));
    message.addRecipient(new EmailAddress(m_ui.testEmailTo->text()));
    message.setSubject(QString::fromUtf8("Test Email: Sensor 'Sensor1' triggered alarm 'Alarm1'"));

    MimeHtml body;
    body.setHtml(QString::fromUtf8(
                "<p><span style=\"color: #ff0000; font-size: 12pt;\">&lt;&lt;&lt; This is a test email! &gt;&gt;&gt;</span></p>"
                "<p>Alarm '<strong>Alarm1</strong>' was triggered by sensor '<strong>Sensor1</strong>.</p>"
                "<p>Measurement:</p>"
                "<ul>"
                "<li>Temperature: <strong>23 &deg;C</strong></li>"
                "<li>Humidity: <strong>77 %RH</strong></li>"
                "<li>Sensor Errors: <strong>None</strong></li>"
                "<li>Battery: <strong>55&nbsp;%</strong></li>"
                "</ul>"
                "<p>Timestamp: <strong>12-23-2017 12:00</strong> <span style=\"font-size: 8pt;\"><em>(dd-mm-yyyy hh:mm)</em></span></p>"
                "<p><span style=\"font-size: 12pt; color: #ff0000;\">&lt;&lt;&lt; This is a test email! &gt;&gt;&gt;</span></p>"
                "<p><span style=\"font-size: 10pt;\"><em>- Sense -</em></span></p>"
                     ));

    message.addPart(&body);

    if (smtp.connectToHost() && smtp.login() && smtp.sendMail(message))
    {
        QMessageBox::information(this, "Done", QString("Email sent to '%1'.").arg(m_ui.testEmailTo->text()));
    }
    smtp.quit();
}
