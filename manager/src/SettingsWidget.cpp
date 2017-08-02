#include "SettingsWidget.h"
#include "ConfigureAlarmDialog.h"
#include <QMessageBox>
#include <QInputDialog>
#include "Smtp/SmtpMime"

#include "DB.h"

//////////////////////////////////////////////////////////////////////////

SettingsWidget::SettingsWidget(QWidget *parent)
    : QWidget(parent)
{
    m_ui.setupUi(this);
    //setEnabled(false);
}

//////////////////////////////////////////////////////////////////////////

SettingsWidget::~SettingsWidget()
{
    shutdown();
}

//////////////////////////////////////////////////////////////////////////

void SettingsWidget::init(Comms& comms, Settings& settings)
{
    setEnabled(true);
    m_settings = &settings;
    m_db = nullptr;

    m_ui.baseStationsWidget->init(comms, settings);
    m_ui.usersWidget->init(settings);

    setRW();
    connect(&settings, &Settings::userLoggedIn, this, &SettingsWidget::setRW);

    setEmailSettings(m_settings->getEmailSettings());
    connect(m_ui.emailAddRecipient, &QPushButton::released, this, &SettingsWidget::addEmailRecipient);
    connect(m_ui.emailRemoveRecipient, &QPushButton::released, this, &SettingsWidget::removeEmailRecipient);
    connect(m_ui.emailApply, &QPushButton::released, this, &SettingsWidget::applyEmailSettings);
    connect(m_ui.emailReset, &QPushButton::released, [this]() { setEmailSettings(m_settings->getEmailSettings()); });
    connect(m_ui.emailTest, &QPushButton::released, this, &SettingsWidget::sendTestEmail);

    connect(m_ui.sensorsApply, &QPushButton::released, this, &SettingsWidget::applySensorSettings);
    connect(m_ui.sensorsReset, &QPushButton::released, [this]() { if (m_db) setSensorSettings(m_db->getSensorSettings()); });
    m_ui.sensorsTab->setEnabled(m_settings->getActiveBaseStationId() != 0);
}

//////////////////////////////////////////////////////////////////////////

void SettingsWidget::shutdown()
{
    setEnabled(false);
    m_settings = nullptr;
    m_db = nullptr;

//    m_ui.baseStationsWidget->shutdown();
    m_ui.usersWidget->shutdown();
}

//////////////////////////////////////////////////////////////////////////

void SettingsWidget::initBaseStation(Settings::BaseStationId id)
{
    int32_t index = m_settings->findBaseStationIndexById(id);
    if (index < 0)
    {
        assert(false);
        return;
    }

    Settings::BaseStation const& bs = m_settings->getBaseStation(index);
    DB& db = m_settings->getBaseStationDB(index);
    m_db = &db;
    m_ui.reportsWidget->init(*m_settings, db);

    setSensorSettings(m_db->getSensorSettings());

    setRW();
}

//////////////////////////////////////////////////////////////////////////

void SettingsWidget::shutdownBaseStation(Settings::BaseStationId id)
{
    m_db = nullptr;
    m_ui.reportsWidget->shutdown();
    m_ui.reportsWidget->setEnabled(false);
    m_ui.sensorsTab->setEnabled(false);
}

//////////////////////////////////////////////////////////////////////////

void SettingsWidget::setRW()
{
    m_ui.emailTab->setEnabled(m_settings->isLoggedInAsAdmin());
    m_ui.ftpTab->setEnabled(m_settings->isLoggedInAsAdmin());
    m_ui.sensorsTab->setEnabled(m_settings->isLoggedInAsAdmin());
}

//////////////////////////////////////////////////////////////////////////

void SettingsWidget::addEmailRecipient()
{
    bool ok = false;
    QString recipient = QInputDialog::getText(this, "Add Recipient", "Recipient", QLineEdit::Normal, "", &ok);
    if (!ok)
    {
        return;
    }

    QRegExp mailREX("^[A-Z0-9._%-]+@[A-Z0-9.-]+\\.[A-Z]{2,4}$");//\\b[A-Z0-9._%+-]+@[A-Z0-9.-]+\\.[A-Z]{2,4}\\b");
    mailREX.setCaseSensitivity(Qt::CaseInsensitive);
    mailREX.setPatternSyntax(QRegExp::RegExp);
    if (recipient.isEmpty() || !mailREX.exactMatch(recipient))
    {
        QMessageBox::critical(this, "Error", "Please enter a valid email recipient.");
        return;
    }

    m_ui.emailRecipientList->addItem(recipient);
    m_ui.emailRemoveRecipient->setEnabled(m_ui.emailRecipientList->count() > 0);
}

//////////////////////////////////////////////////////////////////////////

void SettingsWidget::removeEmailRecipient()
{
    QList<QListWidgetItem*> selected = m_ui.emailRecipientList->selectedItems();
    if (selected.isEmpty())
    {
        QMessageBox::critical(this, "Error", "Please select the alarm you want to remove.");
        return;
    }

    m_ui.emailRecipientList->removeItemWidget(selected.at(0));
    m_ui.emailRemoveRecipient->setEnabled(m_ui.emailRecipientList->count() > 0);
}

//////////////////////////////////////////////////////////////////////////

void SettingsWidget::setEmailSettings(Settings::EmailSettings const& settings)
{
    m_ui.emailHost->setText(settings.host.c_str());
    m_ui.emailPort->setValue(settings.port);
    m_ui.emailUsername->setText(settings.username.c_str());
    m_ui.emailPassword->setText(settings.password.c_str());
    m_ui.emailFrom->setText(settings.from.c_str());

    m_ui.emailRecipientList->clear();
    for (std::string const& recipient: settings.recipients)
    {
        m_ui.emailRecipientList->addItem(recipient.c_str());
    }
    m_ui.emailRemoveRecipient->setEnabled(m_ui.emailRecipientList->count() > 0);
}

//////////////////////////////////////////////////////////////////////////

bool SettingsWidget::getEmailSettings(Settings::EmailSettings& settings)
{
    settings.host = m_ui.emailHost->text().toUtf8().data();
    settings.port = static_cast<uint16_t>(m_ui.emailPort->value());
    settings.username = m_ui.emailUsername->text().toUtf8().data();
    settings.password = m_ui.emailPassword->text().toUtf8().data();
    settings.from = m_ui.emailFrom->text().toUtf8().data();

    settings.recipients.clear();
    for (int i = 0; i < m_ui.emailRecipientList->count(); i++)
    {
        std::string recipient = m_ui.emailRecipientList->item(i)->text().toUtf8().data();
        if (!recipient.empty())
        {
            settings.recipients.push_back(recipient);
        }
    }

    if (settings.host.empty())
    {
        QMessageBox::critical(this, "Error", "You need to specify a valid host.");
        return false;
    }
    if (settings.recipients.empty())
    {
        QMessageBox::critical(this, "Error", "You need to specify a valid recipient.");
        return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////

void SettingsWidget::applyEmailSettings()
{
    Settings::EmailSettings settings;
    if (!getEmailSettings(settings))
    {
        return;
    }

    m_settings->setEmailSettings(settings);
}

//////////////////////////////////////////////////////////////////////////

void SettingsWidget::sendTestEmail()
{
    Settings::EmailSettings settings;
    if (!getEmailSettings(settings))
    {
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

    message.setSender(new EmailAddress(QString::fromUtf8(settings.from.c_str()), QString::fromUtf8(settings.from.c_str())));
    for (std::string const& recipient: settings.recipients)
    {
        message.addRecipient(new EmailAddress(QString::fromUtf8(recipient.c_str())));
    }
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
        QMessageBox::information(this, "Done", QString("Email sent."));
    }
    smtp.quit();
}

//////////////////////////////////////////////////////////////////////////

void SettingsWidget::setSensorSettings(DB::SensorSettings const& settings)
{
    m_ui.sensorsMeasurementPeriod->setValue(std::chrono::duration<float>(settings.descriptor.measurementPeriod).count() / 60.f);
    m_ui.sensorsCommsPeriod->setValue(std::chrono::duration<float>(settings.descriptor.commsPeriod).count() / 60.f);
    m_ui.sensorsComputedCommsPeriod->setValue(std::chrono::duration<float>(settings.computedCommsPeriod).count() / 60.f);
    m_ui.sensorsSleeping->setChecked(settings.descriptor.sensorsSleeping);
}

//////////////////////////////////////////////////////////////////////////

bool SettingsWidget::getSensorSettings(DB::SensorSettingsDescriptor& descriptor)
{
    descriptor.measurementPeriod = std::chrono::seconds(static_cast<size_t>(m_ui.sensorsMeasurementPeriod->value() * 60.0));
    descriptor.commsPeriod = std::chrono::seconds(static_cast<size_t>(m_ui.sensorsCommsPeriod->value() * 60.0));
    descriptor.sensorsSleeping = m_ui.sensorsSleeping->isChecked();

    if (descriptor.commsPeriod.count() == 0)
    {
        QMessageBox::critical(this, "Error", "You need to specify a valid comms duration.");
        return false;
    }
    if (descriptor.measurementPeriod.count() == 0)
    {
        QMessageBox::critical(this, "Error", "You need to specify a valid measurement duration.");
        return false;
    }
    if (descriptor.commsPeriod < descriptor.measurementPeriod)
    {
        QMessageBox::critical(this, "Error", "The comms period cannot be smaller than the measurement period.");
        return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////

void SettingsWidget::applySensorSettings()
{
    if (!m_db)
    {
        return;
    }

    DB::SensorSettingsDescriptor settings;
    if (!getSensorSettings(settings))
    {
        return;
    }

    if (!m_db->setSensorSettings(settings))
    {
        QMessageBox::critical(this, "Error", "Cannot set sensor settings.");
    }
}

