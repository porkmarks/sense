#include "SettingsWidget.h"
#include "ConfigureAlarmDialog.h"
#include <QMessageBox>
#include <QInputDialog>
#include <QProgressDialog>
#include "Smtp/SmtpMime"
#include "qftp.h"

#include "DB.h"
#include "PermissionsCheck.h"

#define NOMINMAX
#include <windows.h>

extern std::pair<std::string, int32_t> computeDurationString(DB::Clock::duration d);

DB::Clock::duration computeBatteryLife(float capacity, DB::Clock::duration measurementPeriod, DB::Clock::duration commsPeriod, float power, uint8_t hardwareVersion)
{
    float idleMAh = 0.01f;
    float powerMu = std::clamp(power, -3.f, 17.f) / 20.f;
    float commsMAh = 0;
    float measurementMAh = 0;
    if (hardwareVersion == 3)
    {
        float minMAh = 20.f;
        float maxMAh = 80.f;
        commsMAh = minMAh + (maxMAh - minMAh) * powerMu;
        measurementMAh = 3;
    }
    else
    {
        float minMAh = 60.f;
        float maxMAh = 130.f;
        commsMAh = minMAh + (maxMAh - minMAh) * powerMu;
        measurementMAh = 3;
    }

    float commsDuration = 2.f;
    float commsPeriodS = std::chrono::duration<float>(commsPeriod).count();

    float measurementDuration = 1.f;
    float measurementPeriodS = std::chrono::duration<float>(measurementPeriod).count();

    float measurementPerHour = 3600.f / measurementPeriodS;
    float commsPerHour = 3600.f / commsPeriodS;

    float measurementDurationPerHour = measurementPerHour * measurementDuration;
    float commsDurationPerHour = commsPerHour * commsDuration;

    float commsUsagePerHour = commsMAh * commsDurationPerHour / 3600.f;
    float measurementUsagePerHour = measurementMAh * measurementDurationPerHour / 3600.f;

    float usagePerHour = commsUsagePerHour + measurementUsagePerHour + idleMAh;

    float hours = capacity / usagePerHour;
    return std::chrono::hours(static_cast<int32_t>(hours));
}

//////////////////////////////////////////////////////////////////////////

SettingsWidget::SettingsWidget(QWidget *parent)
    : QWidget(parent)
{
    m_ui.setupUi(this);
    //setEnabled(false);

	m_ui.batteryIcon->setPixmap(QIcon(":/icons/ui/battery-0.png").pixmap(24, 24));
	m_ui.signalStrengthIcon->setPixmap(QIcon(":/icons/ui/signal-0.png").pixmap(24, 24));
}

//////////////////////////////////////////////////////////////////////////

SettingsWidget::~SettingsWidget()
{
    shutdown();
}

//////////////////////////////////////////////////////////////////////////

void SettingsWidget::init(Comms& comms, DB& db)
{
    for (const QMetaObject::Connection& connection: m_uiConnections)
    {
        QObject::disconnect(connection);
    }
    m_uiConnections.clear();

    setEnabled(true);
    m_db = &db;

	m_uiConnections.push_back(connect(&db, &DB::userLoggedIn, this, &SettingsWidget::setPermissions));

    m_ui.showDebugConsole->setChecked(IsWindowVisible(GetConsoleWindow()) == SW_SHOW);
    m_ui.showDebugConsole->setVisible(db.isLoggedInAsAdmin());
    m_uiConnections.push_back(connect(m_ui.showDebugConsole, &QCheckBox::stateChanged, this, [](int state) 
    {
        ShowWindow(GetConsoleWindow(), state ? SW_SHOW : SW_HIDE);
    }));

    m_uiConnections.push_back(connect(m_ui.radioPower, static_cast<void(QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &SettingsWidget::computeBatteryLife));
    m_uiConnections.push_back(connect(m_ui.sensorsMeasurementPeriod, static_cast<void(QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this, &SettingsWidget::computeBatteryLife));
    m_uiConnections.push_back(connect(m_ui.sensorsCommsPeriod, static_cast<void(QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this, &SettingsWidget::computeBatteryLife));
    m_uiConnections.push_back(connect(m_ui.batteryCapacity, static_cast<void(QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &SettingsWidget::computeBatteryLife));

	setEmailSettings(m_db->getEmailSettings());
    m_uiConnections.push_back(connect(m_ui.emailHost, &QLineEdit::textChanged, this, &SettingsWidget::resetEmailProviderPreset));
    m_uiConnections.push_back(connect(m_ui.emailPort, static_cast<void(QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &SettingsWidget::resetEmailProviderPreset));
    m_uiConnections.push_back(connect(m_ui.emailConnection, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &SettingsWidget::resetEmailProviderPreset));

	m_uiConnections.push_back(connect(m_ui.emailSmtpProviderPreset, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &SettingsWidget::emailSmtpProviderPresetChanged));
    m_uiConnections.push_back(connect(m_ui.emailAddRecipient, &QPushButton::released, this, &SettingsWidget::addEmailRecipient));
    m_uiConnections.push_back(connect(m_ui.emailRemoveRecipient, &QPushButton::released, this, &SettingsWidget::removeEmailRecipient));
    //m_uiConnections.push_back(connect(m_ui.emailApply, &QPushButton::released, this, &SettingsWidget::applyEmailSettings));
    //m_uiConnections.push_back(connect(m_ui.emailReset, &QPushButton::released, [this]() { setEmailSettings(m_settings->getEmailSettings()); }));
    m_uiConnections.push_back(connect(m_ui.emailTest, &QPushButton::released, this, &SettingsWidget::sendTestEmail));

    //m_uiConnections.push_back(connect(m_ui.ftpApply, &QPushButton::released, this, &SettingsWidget::applyFtpSettings));
    //m_uiConnections.push_back(connect(m_ui.ftpReset, &QPushButton::released, [this]() { setFtpSettings(m_settings->getFtpSettings()); }));
    m_uiConnections.push_back(connect(m_ui.ftpTest, &QPushButton::released, this, &SettingsWidget::testFtpSettings));

    //m_uiConnections.push_back(connect(m_ui.sensorsApply, &QPushButton::released, this, &SettingsWidget::applySensorTimeConfig));
    //m_uiConnections.push_back(connect(m_ui.sensorsReset, &QPushButton::released, [this]() { if (m_db) setSensorTimeConfig(m_db->getLastSensorTimeConfig()); }));
//     m_uiConnections.push_back(connect(m_ui.list, &QTreeWidget::currentItemChanged, [this](QTreeWidgetItem* current, QTreeWidgetItem* previous)
//     {
//         m_ui.stackedWidget->setCurrentIndex(m_ui.list->indexOfTopLevelItem(current));
//     }));
//     m_ui.list->setCurrentItem(m_ui.list->topLevelItem(0));

    setSensorSettings(m_db->getSensorSettings());
    setSensorTimeConfig(m_db->getLastSensorTimeConfig());
    m_dbConnections.push_back(connect(m_db, &DB::sensorSettingsChanged, [this]() { if (m_db) setSensorSettings(m_db->getSensorSettings()); }));
    m_dbConnections.push_back(connect(m_db, &DB::sensorTimeConfigChanged, [this]() { if (m_db) setSensorTimeConfig(m_db->getLastSensorTimeConfig()); }));

    setPermissions();
}

//////////////////////////////////////////////////////////////////////////

void SettingsWidget::shutdown()
{
    setEnabled(false);
    m_db = nullptr;

    for (const QMetaObject::Connection& connection: m_dbConnections)
    {
        QObject::disconnect(connection);
    }
    m_dbConnections.clear();
}

//////////////////////////////////////////////////////////////////////////

void SettingsWidget::save()
{
    applyEmailSettings();
    applyFtpSettings();
    applySensorTimeConfig();
    applySensorSettings();
}

//////////////////////////////////////////////////////////////////////////

void SettingsWidget::setPermissions()
{
	m_ui.emailTab->setEnabled(hasPermission(*m_db, DB::UserDescriptor::PermissionChangeEmailSettings));
	m_ui.ftpTab->setEnabled(hasPermission(*m_db, DB::UserDescriptor::PermissionChangeFtpSettings));
	m_ui.batteryCapacity->setEnabled(hasPermission(*m_db, DB::UserDescriptor::PermissionChangeSensorSettings));
	m_ui.radioPower->setEnabled(hasPermission(*m_db, DB::UserDescriptor::PermissionChangeSensorSettings));
	m_ui.batteryAlertThreshold->setEnabled(hasPermission(*m_db, DB::UserDescriptor::PermissionChangeSensorSettings));
	m_ui.signalStrengthAlertThreshold->setEnabled(hasPermission(*m_db, DB::UserDescriptor::PermissionChangeSensorSettings));
	m_ui.sensorsCommsPeriod->setEnabled(hasPermission(*m_db, DB::UserDescriptor::PermissionChangeSensorSettings));
	m_ui.sensorsMeasurementPeriod->setEnabled(hasPermission(*m_db, DB::UserDescriptor::PermissionChangeSensorSettings));
}

//////////////////////////////////////////////////////////////////////////

void SettingsWidget::resetEmailProviderPreset()
{
    m_ui.emailSmtpProviderPreset->setCurrentIndex(0);
}

//////////////////////////////////////////////////////////////////////////

void SettingsWidget::emailSmtpProviderPresetChanged()
{
    if (m_ui.emailSmtpProviderPreset->currentIndex() == 0)
    {
        return;
    }

    if (m_ui.emailSmtpProviderPreset->currentIndex() == 1) //gmail
    {
        m_ui.emailHost->blockSignals(true);
        m_ui.emailHost->setText("smtp.gmail.com");
        m_ui.emailHost->blockSignals(false);

        m_ui.emailPort->blockSignals(true);
        m_ui.emailPort->setValue(587);
        m_ui.emailPort->blockSignals(false);

        m_ui.emailConnection->blockSignals(true);
		m_ui.emailConnection->setCurrentIndex((int)DB::EmailSettings::Connection::Tls);
        m_ui.emailConnection->blockSignals(false);

        m_ui.emailUsername->setPlaceholderText("example@gmail.com");
    }
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

void SettingsWidget::setEmailSettings(DB::EmailSettings const& settings)
{
    m_ui.emailHost->setText(settings.host.c_str());
    m_ui.emailPort->setValue(settings.port);
    m_ui.emailConnection->setCurrentIndex(static_cast<int>(settings.connection));
    m_ui.emailUsername->setText(settings.username.c_str());
    m_ui.emailPassword->setText(settings.password.c_str());
    m_ui.emailSender->setText(settings.sender.c_str());

    m_ui.emailRecipientList->clear();
    for (std::string const& recipient: settings.recipients)
    {
        m_ui.emailRecipientList->addItem(recipient.c_str());
    }
    m_ui.emailRemoveRecipient->setEnabled(m_ui.emailRecipientList->count() > 0);
}

//////////////////////////////////////////////////////////////////////////

bool SettingsWidget::getEmailSettings(DB::EmailSettings& settings)
{
    settings.host = m_ui.emailHost->text().toUtf8().data();
    settings.port = static_cast<uint16_t>(m_ui.emailPort->value());
    settings.connection = static_cast<DB::EmailSettings::Connection>(m_ui.emailConnection->currentIndex());
    settings.username = m_ui.emailUsername->text().toUtf8().data();
    settings.password = m_ui.emailPassword->text().toUtf8().data();
    settings.sender = m_ui.emailSender->text().toUtf8().data();

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
        QMessageBox::warning(this, "Warning", "No email recipients configured, emails will not be sent.");
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////

void SettingsWidget::applyEmailSettings()
{
    DB::EmailSettings settings;
    if (!getEmailSettings(settings))
    {
        return;
    }

	m_db->setEmailSettings(settings);
}

//////////////////////////////////////////////////////////////////////////

void SettingsWidget::sendTestEmail()
{
	DB::EmailSettings settings;
    if (!getEmailSettings(settings))
    {
        return;
    }

    if (settings.recipients.empty())
    {
        QMessageBox::critical(this, "Error", "No email recipients configured.");
        return;
    }

    std::array<const char*, 5> stepMessages = { "Initializing...", "Connecting...", "Authenticating...", "Sending Email...", "Done" };

	QProgressDialog progress("Sending email...", "Abort", 0, (int)stepMessages.size() - 1, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setAutoClose(false);
    progress.setAutoReset(false);
    progress.setMinimumDuration(0);
    progress.setMinimumWidth(300);

    std::mutex errorMessageMutex;
    QString errorMessage;

    std::atomic_int step = { 0 };
    std::thread thread([&errorMessage, &errorMessageMutex, &step, settings]()
    {
        SmtpClient::ConnectionType connectionType = SmtpClient::SslConnection;
        switch (settings.connection)
        {
        case DB::EmailSettings::Connection::Ssl: connectionType = SmtpClient::SslConnection; break;
        case DB::EmailSettings::Connection::Tcp: connectionType = SmtpClient::TcpConnection; break;
        case DB::EmailSettings::Connection::Tls: connectionType = SmtpClient::TlsConnection; break;
        }

        SmtpClient smtp(QString::fromUtf8(settings.host.c_str()), settings.port, connectionType);

        step = 0;

        connect(&smtp, &SmtpClient::smtpError, [&errorMessageMutex, &errorMessage, &step, &smtp](SmtpClient::SmtpError error)
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
                errorMessage += "Server error: ";
                errorMessage += smtp.getResponseMessage().toUtf8().data();
                errorMessage += "\n";
            }
                break;
            case SmtpClient::ClientError:
            {
                std::lock_guard<std::mutex> lg(errorMessageMutex);
                errorMessage += "Client error: ";
                errorMessage += smtp.getResponseMessage().toUtf8().data();
                errorMessage += "\n";
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

        message.setSender(new EmailAddress(QString::fromUtf8(settings.sender.c_str()), QString::fromUtf8(settings.sender.c_str())));
        for (std::string const& recipient: settings.recipients)
        {
            message.addRecipient(new EmailAddress(QString::fromUtf8(recipient.c_str())));
        }
        message.setSubject(QString::fromUtf8("Test Email: Sensor 'Sensor1' triggered alarm 'Alarm1'"));

        MimeHtml body;
        body.setHtml(QString::fromUtf8(
                         "<p><span style=\"color: #ff0000; font-size: 12pt;\">&lt;&lt;&lt; This is a test email! &gt;&gt;&gt;</span></p>"
                         "<p>Alarm '<strong>Alarm1</strong>' was triggered by sensor '<strong>Sensor1</strong>'.</p>"
                         "<p>Measurement:</p>"
                         "<ul>"
                         "<li>Temperature: <strong>23&deg;C</strong></li>"
                         "<li>Humidity: <strong>77%</strong></li>"
                         "<li>Battery: <strong>55%</strong></li>"
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
            progress.setLabelText(QString("Error: %1\nSSL Info: %2").arg(errorMessage.isEmpty() ? QString("Unknown") : errorMessage).arg(QSslSocket::sslLibraryBuildVersionString()));
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

void SettingsWidget::setFtpSettings(DB::FtpSettings const& settings)
{
    m_ui.ftpHost->setText(settings.host.c_str());
    m_ui.ftpPort->setValue(settings.port);
    m_ui.ftpUsername->setText(settings.username.c_str());
    m_ui.ftpPassword->setText(settings.password.c_str());
}

//////////////////////////////////////////////////////////////////////////

bool SettingsWidget::getFtpSettings(DB::FtpSettings& settings)
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
    DB::FtpSettings settings;
    if (!getFtpSettings(settings))
    {
        return;
    }

	m_db->setFtpSettings(settings);
}

//////////////////////////////////////////////////////////////////////////

void SettingsWidget::testFtpSettings()
{
	DB::FtpSettings settings;
    if (!getFtpSettings(settings))
    {
        return;
    }

	static const DB::Clock::duration timeout = std::chrono::seconds(10);

    QFtp* ftp = new QFtp(this);

    ftp->connectToHost(settings.host.c_str(), settings.port);

	DB::Clock::time_point start = DB::Clock::now();
    while (ftp->hasPendingCommands())
    {
        if (DB::Clock::now() - start > timeout)
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
    start = DB::Clock::now();
    while (ftp->hasPendingCommands())
    {
        if (DB::Clock::now() - start > timeout)
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
    start = DB::Clock::now();
    while (ftp->hasPendingCommands())
    {
        if (DB::Clock::now() - start > timeout)
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

void SettingsWidget::setSensorTimeConfig(DB::SensorTimeConfig const& config)
{
    m_ui.sensorsMeasurementPeriod->setValue(std::chrono::duration<float>(config.descriptor.measurementPeriod).count() / 60.f);
    m_ui.sensorsCommsPeriod->setValue(std::chrono::duration<float>(config.descriptor.commsPeriod).count() / 60.f);
    m_ui.sensorsComputedCommsPeriod->setText(QString("%1 minutes").arg(std::chrono::duration<float>(config.computedCommsPeriod).count() / 60.f, 0, 'f', 1));
}

//////////////////////////////////////////////////////////////////////////

Result<DB::SensorTimeConfigDescriptor> SettingsWidget::getSensorTimeConfig() const
{
    DB::SensorTimeConfigDescriptor descriptor;
    descriptor.measurementPeriod = std::chrono::seconds(static_cast<size_t>(m_ui.sensorsMeasurementPeriod->value() * 60.0));
    descriptor.commsPeriod = std::chrono::seconds(static_cast<size_t>(m_ui.sensorsCommsPeriod->value() * 60.0));

    if (descriptor.commsPeriod.count() == 0)
    {
        return Error("You need to specify a valid comms duration.");
    }
    if (descriptor.measurementPeriod.count() == 0)
    {
        return Error("You need to specify a valid measurement duration.");
    }
    if (descriptor.commsPeriod < descriptor.measurementPeriod)
    {
        return Error("The comms period cannot be smaller than the measurement period.");
    }
    return descriptor;
}

//////////////////////////////////////////////////////////////////////////

void SettingsWidget::applySensorTimeConfig()
{
    if (!m_db)
    {
        return;
    }

    Result<DB::SensorTimeConfigDescriptor> result = getSensorTimeConfig();
    if (result != success)
    {
        QMessageBox::critical(this, "Error", result.error().what().c_str());
        return;
    }

    Result<void> applyResult = m_db->addSensorTimeConfig(result.payload());
    if (applyResult != success)
    {
        QMessageBox::critical(this, "Error", QString("Cannot set sensor settings: %1").arg(applyResult.error().what().c_str()));
    }
    setSensorTimeConfig(m_db->getLastSensorTimeConfig());
    computeBatteryLife();
}

//////////////////////////////////////////////////////////////////////////

void SettingsWidget::setSensorSettings(DB::SensorSettings const& settings)
{
	m_ui.radioPower->setValue(settings.radioPower);
    m_ui.batteryAlertThreshold->setValue(std::clamp(settings.alertBatteryLevel * 100.f, 0.f, 100.f));
    m_ui.signalStrengthAlertThreshold->setValue(std::clamp(settings.alertSignalStrengthLevel * 100.f, 0.f, 100.f));
}

//////////////////////////////////////////////////////////////////////////

Result<DB::SensorSettings> SettingsWidget::getSensorSettings() const
{
    DB::SensorSettings settings = m_db->getSensorSettings();
	settings.radioPower = static_cast<int8_t>(m_ui.radioPower->value());
    settings.alertBatteryLevel = std::clamp((float)m_ui.batteryAlertThreshold->value() / 100.f, 0.f, 1.f);
    settings.alertSignalStrengthLevel = std::clamp((float)m_ui.signalStrengthAlertThreshold->value() / 100.f, 0.f, 1.f);
    return settings;
}

//////////////////////////////////////////////////////////////////////////

void SettingsWidget::applySensorSettings()
{
	if (!m_db)
	{
		return;
	}

	Result<DB::SensorSettings> result = getSensorSettings();
	if (result != success)
	{
		QMessageBox::critical(this, "Error", result.error().what().c_str());
		return;
	}

	Result<void> applyResult = m_db->setSensorSettings(result.payload());
	if (applyResult != success)
	{
		QMessageBox::critical(this, "Error", QString("Cannot set sensor settings: %1").arg(applyResult.error().what().c_str()));
	}
	setSensorSettings(m_db->getSensorSettings());
	computeBatteryLife();
}

//////////////////////////////////////////////////////////////////////////

void SettingsWidget::computeBatteryLife()
{
    if (!m_db)
    {
        return;
    }
    Result<DB::SensorTimeConfigDescriptor> timeConfigResult = getSensorTimeConfig();
    if (timeConfigResult != success)
    {
		m_ui.batteryLife->setText(QString("Error: %1").arg(timeConfigResult.error().what().c_str()));
        return;
    }
	Result<DB::SensorSettings> settingsResult = getSensorSettings();
	if (settingsResult != success)
	{
		m_ui.batteryLife->setText(QString("Error: %1").arg(settingsResult.error().what().c_str()));
        return;
	}

    DB::SensorTimeConfigDescriptor descriptor = timeConfigResult.extract_payload();
    DB::SensorSettings settings = settingsResult.extract_payload();
    DB::Clock::duration d = ::computeBatteryLife(m_ui.batteryCapacity->value(),
                                                    descriptor.measurementPeriod,
                                                    m_db->computeActualCommsPeriod(descriptor),
                                                    settings.radioPower,
                                                    1);

    int64_t seconds = std::chrono::duration_cast<std::chrono::seconds>(d).count();
    int64_t days = seconds / (24 * 3600);
    m_ui.batteryLife->setText(QString("~%1 days").arg(days));
}

