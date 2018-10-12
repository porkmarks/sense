#include "SettingsWidget.h"
#include "ConfigureAlarmDialog.h"
#include <QMessageBox>
#include <QInputDialog>
#include <QProgressDialog>
#include "Smtp/SmtpMime"
#include "qftp.h"

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
    for (const QMetaObject::Connection& connection: m_uiConnections)
    {
        QObject::disconnect(connection);
    }
    m_uiConnections.clear();

    setEnabled(true);
    m_settings = &settings;
    m_db = nullptr;

    m_ui.baseStationsWidget->init(comms, settings);
    m_ui.usersWidget->init(settings);

    setRW();
    m_uiConnections.push_back(connect(&settings, &Settings::userLoggedIn, this, &SettingsWidget::setRW));

    setEmailSettings(m_settings->getEmailSettings());
    m_uiConnections.push_back(connect(m_ui.emailAddRecipient, &QPushButton::released, this, &SettingsWidget::addEmailRecipient));
    m_uiConnections.push_back(connect(m_ui.emailRemoveRecipient, &QPushButton::released, this, &SettingsWidget::removeEmailRecipient));
    m_uiConnections.push_back(connect(m_ui.emailApply, &QPushButton::released, this, &SettingsWidget::applyEmailSettings));
    m_uiConnections.push_back(connect(m_ui.emailReset, &QPushButton::released, [this]() { setEmailSettings(m_settings->getEmailSettings()); }));
    m_uiConnections.push_back(connect(m_ui.emailTest, &QPushButton::released, this, &SettingsWidget::sendTestEmail));

    m_uiConnections.push_back(connect(m_ui.ftpApply, &QPushButton::released, this, &SettingsWidget::applyFtpSettings));
    m_uiConnections.push_back(connect(m_ui.ftpReset, &QPushButton::released, [this]() { setFtpSettings(m_settings->getFtpSettings()); }));
    m_uiConnections.push_back(connect(m_ui.ftpTest, &QPushButton::released, this, &SettingsWidget::testFtpSettings));

    m_uiConnections.push_back(connect(m_ui.sensorsApply, &QPushButton::released, this, &SettingsWidget::applySensorsConfig));
    m_uiConnections.push_back(connect(m_ui.sensorsReset, &QPushButton::released, [this]() { if (m_db) setSensorsConfig(m_db->getLastSensorsConfig()); }));
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
    int32_t _index = m_settings->findBaseStationIndexById(id);
    if (_index < 0)
    {
        assert(false);
        return;
    }

    //Settings::BaseStation const& bs = m_settings->getBaseStation(index);
    size_t index = static_cast<size_t>(_index);
    DB& db = m_settings->getBaseStationDB(index);
    m_db = &db;
    m_ui.reportsWidget->init(*m_settings, db);

    setSensorsConfig(m_db->getLastSensorsConfig());

    setRW();
}

//////////////////////////////////////////////////////////////////////////

void SettingsWidget::shutdownBaseStation(Settings::BaseStationId /*id*/)
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
    //delete reinterpret_cast<QString*>(0xFEE1DEAD);

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

    delete selected.at(0);
    m_ui.emailRemoveRecipient->setEnabled(m_ui.emailRecipientList->count() > 0);
}

//////////////////////////////////////////////////////////////////////////

void SettingsWidget::setEmailSettings(Settings::EmailSettings const& settings)
{
    m_ui.emailHost->setText(settings.host.c_str());
    m_ui.emailPort->setValue(settings.port);
    m_ui.emailConnection->setCurrentIndex(static_cast<int>(settings.connection));
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
    settings.connection = static_cast<Settings::EmailSettings::Connection>(m_ui.emailConnection->currentIndex());
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

    std::array<const char*, 5> stepMessages = { "Initializing...", "Connecting...", "Authenticating...", "Sending Email...", "Done" };

    QProgressDialog progress("Sending email...", "Abort", 0, stepMessages.size() - 1, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setAutoClose(false);
    progress.setAutoReset(false);
    progress.setMinimumDuration(0);

    std::mutex errorMessageMutex;
    QString errorMessage;

    std::atomic_int step = { 0 };
    std::thread thread([this, &errorMessage, &errorMessageMutex, &step, settings]()
    {
        SmtpClient::ConnectionType connectionType = SmtpClient::SslConnection;
        switch (settings.connection)
        {
        case Emailer::EmailSettings::Connection::Ssl: connectionType = SmtpClient::SslConnection; break;
        case Emailer::EmailSettings::Connection::Tcp: connectionType = SmtpClient::TcpConnection; break;
        case Emailer::EmailSettings::Connection::Tls: connectionType = SmtpClient::TlsConnection; break;
        }

        SmtpClient smtp(QString::fromUtf8(settings.host.c_str()), settings.port, connectionType);

        step = 0;

        connect(&smtp, &SmtpClient::smtpError, [&errorMessageMutex, &errorMessage, &step](SmtpClient::SmtpError error)
        {
            switch (error)
            {
            case SmtpClient::ConnectionTimeoutError:
            {
                std::lock_guard<std::mutex> lg(errorMessageMutex);
                errorMessage += "Connection timeout.\n";
            }
                break;
            case SmtpClient::ResponseTimeoutError:
            {
                std::lock_guard<std::mutex> lg(errorMessageMutex);
                errorMessage += "Response timeout.\n";
            }
                break;
            case SmtpClient::SendDataTimeoutError:
            {
                std::lock_guard<std::mutex> lg(errorMessageMutex);
                errorMessage += "Send data timeout.\n";
            }
                break;
            case SmtpClient::AuthenticationFailedError:
            {
                std::lock_guard<std::mutex> lg(errorMessageMutex);
                errorMessage += "Authentication failed.\n";
            }
                break;
            case SmtpClient::ServerError:
            {
                std::lock_guard<std::mutex> lg(errorMessageMutex);
                errorMessage += "Server error.\n";
            }
                break;
            case SmtpClient::ClientError:
            {
                std::lock_guard<std::mutex> lg(errorMessageMutex);
                errorMessage += "Client error.\n";
            }
                break;
            default:
            {
                std::lock_guard<std::mutex> lg(errorMessageMutex);
                errorMessage += "Unknown error.\n";
            }
                break;
            }
            step = -1;
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
                         "<li>Battery: <strong>55&nbsp;%</strong></li>"
                         "</ul>"
                         "<p>Timestamp: <strong>12-23-2017 12:00</strong> <span style=\"font-size: 8pt;\"><em>(dd-mm-yyyy hh:mm)</em></span></p>"
                         "<p><span style=\"font-size: 12pt; color: #ff0000;\">&lt;&lt;&lt; This is a test email! &gt;&gt;&gt;</span></p>"
                         "<p><span style=\"font-size: 10pt;\"><em>- Sense -</em></span></p>"
                         ));

        message.addPart(&body);

        step = 1;
        if (!smtp.connectToHost() || step < 0)
        {
            step = -1;
            smtp.quit();
            return;
        }
        step = 2;
        if (!smtp.login() || step < 0)
        {
            step = -1;
            smtp.quit();
            return;
        }
        step = 3;
        if (!smtp.sendMail(message) || step < 0)
        {
            step = -1;
            smtp.quit();
            return;
        }
        step = 4;
        smtp.quit();
        return;
    });

    do
    {
        if (step >= 0 && step < static_cast<int>(stepMessages.size()))
        {
            progress.setLabelText(stepMessages[step]);
            progress.setValue(step);
            if (step == static_cast<int>(stepMessages.size()) - 1)
            {
                progress.setCancelButtonText("Ok");
            }
        }
        else
        {
            std::lock_guard<std::mutex> lg(errorMessageMutex);
            progress.setLabelText(QString("Error: %1").arg(errorMessage.isEmpty() ? QString("Unknown") : errorMessage));
            progress.setValue(progress.maximum());
        }
        QApplication::processEvents();
    } while (!progress.wasCanceled());


    while (thread.joinable())
    {
        thread.join();
    }
}

//////////////////////////////////////////////////////////////////////////

void SettingsWidget::setFtpSettings(Settings::FtpSettings const& settings)
{
    m_ui.ftpHost->setText(settings.host.c_str());
    m_ui.ftpPort->setValue(settings.port);
    m_ui.ftpUsername->setText(settings.username.c_str());
    m_ui.ftpPassword->setText(settings.password.c_str());
}

//////////////////////////////////////////////////////////////////////////

bool SettingsWidget::getFtpSettings(Settings::FtpSettings& settings)
{
    settings.host = m_ui.ftpHost->text().toUtf8().data();
    settings.port = static_cast<uint16_t>(m_ui.ftpPort->value());
    settings.username = m_ui.ftpUsername->text().toUtf8().data();
    settings.password = m_ui.ftpPassword->text().toUtf8().data();

    if (settings.host.empty())
    {
        QMessageBox::critical(this, "Error", "You need to specify a valid host.");
        return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////

void SettingsWidget::applyFtpSettings()
{
    Settings::FtpSettings settings;
    if (!getFtpSettings(settings))
    {
        return;
    }

    m_settings->setFtpSettings(settings);
}

//////////////////////////////////////////////////////////////////////////

void SettingsWidget::testFtpSettings()
{
    Settings::FtpSettings settings;
    if (!getFtpSettings(settings))
    {
        return;
    }

    static const Settings::Clock::duration timeout = std::chrono::seconds(10);

    QFtp* ftp = new QFtp(this);

    ftp->connectToHost(settings.host.c_str(), settings.port);

    Settings::Clock::time_point start = Settings::Clock::now();
    while (ftp->hasPendingCommands())
    {
        if (Settings::Clock::now() - start > timeout)
        {
            QMessageBox::critical(this, "Error", "Connection timed out.");
            return;
        }
        if (ftp->error() != QFtp::Error::NoError)
        {
            QMessageBox::critical(this, "Error", QString("Connection error: %1.").arg(ftp->errorString()));
            return;
        }
    }

    ftp->login(settings.username.c_str(), settings.password.c_str());
    start = Settings::Clock::now();
    while (ftp->hasPendingCommands())
    {
        if (Settings::Clock::now() - start > timeout)
        {
            QMessageBox::critical(this, "Error", "Authentication timed out.");
            return;
        }
        if (ftp->error() != QFtp::Error::NoError)
        {
            QMessageBox::critical(this, "Error", QString("Authentication error: %1.").arg(ftp->errorString()));
            return;
        }
    }

    ftp->cd(settings.folder.c_str());
    start = Settings::Clock::now();
    while (ftp->hasPendingCommands())
    {
        if (Settings::Clock::now() - start > timeout)
        {
            QMessageBox::critical(this, "Error", "Selecting folder timed out.");
            return;
        }
        if (ftp->error() != QFtp::Error::NoError)
        {
            QMessageBox::critical(this, "Error", QString("Selecting folder error: %1.").arg(ftp->errorString()));
            return;
        }
    }

    ftp->close();
    ftp->deleteLater();

    QMessageBox::information(this, "Success", "The FTP configuration seems ok");
}

//////////////////////////////////////////////////////////////////////////

void SettingsWidget::setSensorsConfig(DB::SensorsConfig const& config)
{
    m_ui.sensorsMeasurementPeriod->setValue(std::chrono::duration<float>(config.descriptor.measurementPeriod).count() / 60.f);
    m_ui.sensorsCommsPeriod->setValue(std::chrono::duration<float>(config.descriptor.commsPeriod).count() / 60.f);
    m_ui.sensorsComputedCommsPeriod->setValue(std::chrono::duration<float>(config.computedCommsPeriod).count() / 60.f);
    m_ui.sensorsSleeping->setChecked(config.descriptor.sensorsSleeping);
    m_ui.sensorsPower->setValue(config.descriptor.sensorsPower);
}

//////////////////////////////////////////////////////////////////////////

bool SettingsWidget::getSensorsConfig(DB::SensorsConfigDescriptor& descriptor)
{
    descriptor.measurementPeriod = std::chrono::seconds(static_cast<size_t>(m_ui.sensorsMeasurementPeriod->value() * 60.0));
    descriptor.commsPeriod = std::chrono::seconds(static_cast<size_t>(m_ui.sensorsCommsPeriod->value() * 60.0));
    descriptor.sensorsSleeping = m_ui.sensorsSleeping->isChecked();
    descriptor.sensorsPower = static_cast<uint8_t>(m_ui.sensorsPower->value());

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

void SettingsWidget::applySensorsConfig()
{
    if (!m_db)
    {
        return;
    }

    DB::SensorsConfigDescriptor config;
    if (!getSensorsConfig(config))
    {
        return;
    }

    if (!m_db->addSensorsConfig(config))
    {
        QMessageBox::critical(this, "Error", "Cannot set sensor settings.");
    }
}

