#include "SettingsWidget.h"
#include "ConfigureAlarmDialog.h"
#include <QMessageBox>
#include <QInputDialog>
#include <QProgressDialog>
#include "Smtp/SmtpMime"
#include "qftp.h"

#include "DB.h"

extern std::pair<std::string, int32_t> computeDurationString(DB::Clock::duration d);

DB::Clock::duration computeBatteryLife(float capacity, DB::Clock::duration measurementPeriod, DB::Clock::duration commsPeriod, float power, uint8_t hardwareVersion)
{
    float idleMAh = 0.01f;
    float powerMu = std::min(std::max(float(power), 0.f), 20.f) / 20.f;
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

    m_uiConnections.push_back(connect(&settings, &Settings::userLoggedIn, this, &SettingsWidget::setRW));

    m_uiConnections.push_back(connect(m_ui.sensorsPower, static_cast<void(QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &SettingsWidget::computeBatteryLife));
    m_uiConnections.push_back(connect(m_ui.sensorsMeasurementPeriod, static_cast<void(QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this, &SettingsWidget::computeBatteryLife));
    m_uiConnections.push_back(connect(m_ui.sensorsCommsPeriod, static_cast<void(QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this, &SettingsWidget::computeBatteryLife));
    m_uiConnections.push_back(connect(m_ui.batteryCapacity, static_cast<void(QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &SettingsWidget::computeBatteryLife));

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
    m_uiConnections.push_back(connect(m_ui.list, &QTreeWidget::currentItemChanged, [this](QTreeWidgetItem* current, QTreeWidgetItem* previous)
    {
        m_ui.stackedWidget->setCurrentIndex(m_ui.list->indexOfTopLevelItem(current));
    }));
    m_ui.list->setCurrentItem(m_ui.list->topLevelItem(0));

    m_db = &m_settings->getDB();
    m_ui.reportsWidget->init(*m_settings);

    setSensorsConfig(m_db->getLastSensorsConfig());
    m_dbConnections.push_back(connect(m_db, &DB::sensorsConfigChanged, [this]() { if (m_db) setSensorsConfig(m_db->getLastSensorsConfig()); }));

    setRW();
}

//////////////////////////////////////////////////////////////////////////

void SettingsWidget::shutdown()
{
    setEnabled(false);
    m_settings = nullptr;
    m_db = nullptr;

    for (const QMetaObject::Connection& connection: m_dbConnections)
    {
        QObject::disconnect(connection);
    }
    m_dbConnections.clear();

//    m_ui.baseStationsWidget->shutdown();
    m_ui.usersWidget->shutdown();
}

//////////////////////////////////////////////////////////////////////////

void SettingsWidget::setRW()
{
    m_ui.emailPage->setEnabled(m_settings->isLoggedInAsAdmin());
    m_ui.ftpPage->setEnabled(m_settings->isLoggedInAsAdmin());
    m_ui.sensorsPage->setEnabled(m_settings->isLoggedInAsAdmin());
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
        QMessageBox::warning(this, "Warning", "No email recipients configured, emails will not be sent.");
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

    if (settings.recipients.empty())
    {
        QMessageBox::critical(this, "Error", "No email recipients configured.");
        return;
    }

    std::array<const char*, 5> stepMessages = { "Initializing...", "Connecting...", "Authenticating...", "Sending Email...", "Done" };

    QProgressDialog progress("Sending email...", "Abort", 0, stepMessages.size() - 1, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setAutoClose(false);
    progress.setAutoReset(false);
    progress.setMinimumDuration(0);
    progress.setMinimumWidth(300);

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
                         "<li>Battery: <strong>55 %</strong></li>"
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
    m_ui.sensorsComputedCommsPeriod->setText(QString("%1 minutes").arg(std::chrono::duration<float>(config.computedCommsPeriod).count() / 60.f, 0, 'f', 1));
    m_ui.sensorsPower->setValue(config.descriptor.sensorsPower);
}

//////////////////////////////////////////////////////////////////////////

Result<DB::SensorsConfigDescriptor> SettingsWidget::getSensorsConfig() const
{
    DB::SensorsConfigDescriptor descriptor;
    descriptor.measurementPeriod = std::chrono::seconds(static_cast<size_t>(m_ui.sensorsMeasurementPeriod->value() * 60.0));
    descriptor.commsPeriod = std::chrono::seconds(static_cast<size_t>(m_ui.sensorsCommsPeriod->value() * 60.0));
    descriptor.sensorsPower = static_cast<int8_t>(m_ui.sensorsPower->value());

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

void SettingsWidget::applySensorsConfig()
{
    if (!m_db)
    {
        return;
    }

    Result<DB::SensorsConfigDescriptor> result = getSensorsConfig();
    if (result != success)
    {
        QMessageBox::critical(this, "Error", result.error().what().c_str());
        return;
    }

    Result<void> applyResult = m_db->addSensorsConfig(result.payload());
    if (applyResult != success)
    {
        QMessageBox::critical(this, "Error", QString("Cannot set sensor settings: %1").arg(applyResult.error().what().c_str()));
    }
    setSensorsConfig(m_db->getLastSensorsConfig());
    computeBatteryLife();
}

//////////////////////////////////////////////////////////////////////////

void SettingsWidget::computeBatteryLife()
{
    if (!m_db)
    {
        return;
    }
    Result<DB::SensorsConfigDescriptor> result = getSensorsConfig();
    if (result == success)
    {
        DB::SensorsConfigDescriptor descriptor = result.extract_payload();
        DB::Clock::duration d = ::computeBatteryLife(m_ui.batteryCapacity->value(),
                                                     descriptor.measurementPeriod,
                                                     m_db->computeActualCommsPeriod(descriptor),
                                                     descriptor.sensorsPower,
                                                     1);

        int64_t seconds = std::chrono::duration_cast<std::chrono::seconds>(d).count();
        int64_t days = seconds / (24 * 3600);
        m_ui.batteryLife->setText(QString("~%1 days").arg(days));
    }
    else
    {
        m_ui.batteryLife->setText(QString("Error: %1").arg(result.error().what().c_str()));
    }
}

