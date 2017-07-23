#include "Manager.h"

#include <QInputDialog>
#include <QMessageBox>

#include "ui_LoginDialog.h"

#include "ConfigureUserDialog.h"
#include "SensorSettingsDialog.h"
#include "EmailSettingsDialog.h"
#include "FtpSettingsDialog.h"

#include "Crypt.h"

Logger s_logger;

extern std::string k_passwordHashReferenceText;

std::string getMacStr(Settings::BaseStationDescriptor::Mac const& mac)
{
    char macStr[128];
    sprintf(macStr, "%X:%X:%X:%X:%X:%X", mac[0]&0xFF, mac[1]&0xFF, mac[2]&0xFF, mac[3]&0xFF, mac[4]&0xFF, mac[5]&0xFF);
    return macStr;
}

//////////////////////////////////////////////////////////////////////////

Manager::Manager(QWidget *parent)
    : QMainWindow(parent)
{
    m_ui.setupUi(this);

    if (!s_logger.load("log"))
    {
        if (!s_logger.create("log"))
        {
            QMessageBox::critical(this, "Error", "Cannot create the log file.");
            exit(1);
        }
    }
    s_logger.logInfo("Program started");

    if (!m_settings.load("settings"))
    {
        if (!m_settings.create("settings"))
        {
            s_logger.logError("Cannot create the settings file");
            QMessageBox::critical(this, "Error", "Cannot create the settings file.");
            exit(1);
        }
    }

    m_ui.settingsWidget->init(m_comms, m_settings);
    m_ui.logsWidget->init();

    //m_ui.baseStationsWidget->init(m_comms);

    auto* timer = new QTimer(this);
    timer->setSingleShot(false);
    timer->start(1);
    connect(timer, &QTimer::timeout, this, &Manager::process, Qt::QueuedConnection);

    connect(&m_settings, &Settings::baseStationActivated, this, &Manager::activateBaseStation);
    connect(&m_settings, &Settings::baseStationDeactivated, this, &Manager::deactivateBaseStation);

    readSettings();

    show();

    connect(&m_settings, &Settings::userLoggedIn, this, &Manager::userLoggedIn);

    checkIfAdminExists();

    login();

    if (m_settings.getActiveBaseStationId() != 0)
    {
        activateBaseStation(m_settings.getActiveBaseStationId());
    }
}

//////////////////////////////////////////////////////////////////////////

Manager::~Manager()
{
    s_logger.logInfo("Program exit");

//    delete m_ui.baseStationsWidget;
    delete m_ui.sensorsWidget;
    delete m_ui.measurementsWidget;
    delete m_ui.alarmsWidget;
    delete m_ui.logsWidget;
//    delete m_ui.reportsWidget;
}

//////////////////////////////////////////////////////////////////////////

void Manager::checkIfAdminExists()
{
    if (m_settings.needsAdmin())
    {
        s_logger.logInfo("No admin account exists, creating one");

        QMessageBox::information(this, "Admin", "No admin user exists. Please create one now.");

        ConfigureUserDialog dialog(m_settings);

        Settings::User user;
        dialog.setUser(user);
        dialog.setForcedType(Settings::UserDescriptor::Type::Admin);

        int result = dialog.exec();
        if (result == QDialog::Accepted)
        {
            user = dialog.getUser();
            if (m_settings.addUser(user.descriptor))
            {
                s_logger.logInfo(QString("Admin user '%1' created, logging in").arg(user.descriptor.name.c_str()).toUtf8().data());

                int32_t userIndex = m_settings.findUserIndexByName(user.descriptor.name);
                if (userIndex >= 0)
                {
                    m_settings.setLoggedInUserId(m_settings.getUser(userIndex).id);
                }
                else
                {
                    s_logger.logError("Internal consistency error: user not found after adding");
                }
            }
        }
        else
        {
            s_logger.logWarning("User cancelled admin creation dialog");
        }

        if (m_settings.needsAdmin())
        {
            s_logger.logError("User failed to create an admin account. Exiting");
            QMessageBox::critical(this, "Error", "No admin user exists.\nThe program will now close.");
            exit(1);
        }
    }
}

//////////////////////////////////////////////////////////////////////////

void Manager::login()
{
    if (!m_settings.getLoggedInUser())
    {
        s_logger.logInfo("Asking the user to log in");

        QDialog dialog;
        Ui::LoginDialog ui;
        ui.setupUi(&dialog);

        size_t attempts = 0;
        while (true)
        {
            int result = dialog.exec();
            if (result == QDialog::Accepted)
            {
                int32_t userIndex = m_settings.findUserIndexByName(ui.username->text().toUtf8().data());
                if (userIndex < 0)
                {
                    attempts++;
                    s_logger.logError(QString("Invalid login credentials (user not found), attempt %1").arg(attempts).toUtf8().data());
                    QMessageBox::critical(this, "Error", "Invalid username/password.");
                    continue;
                }

                Settings::User const& user = m_settings.getUser(userIndex);

                Crypt crypt;
                crypt.setAddRandomSalt(false);
                crypt.setIntegrityProtectionMode(Crypt::ProtectionHash);
                crypt.setCompressionMode(Crypt::CompressionAlways);
                crypt.setKey(ui.password->text());
                std::string passwordHash = crypt.encryptToString(QString(k_passwordHashReferenceText.c_str())).toUtf8().data();
                if (user.descriptor.passwordHash != passwordHash)
                {
                    attempts++;
                    s_logger.logError(QString("Invalid login credentials (wrong password), attempt %1").arg(attempts).toUtf8().data());
                    QMessageBox::critical(this, "Error", "Invalid username/password.");
                    continue;
                }

                m_settings.setLoggedInUserId(user.id);
                break;
            }
            else
            {
                s_logger.logError("User failed to log in. Exiting");
                QMessageBox::critical(this, "Error", "You need to be logged in to user this program.\nThe program will now close.");
                exit(1);
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////////

void Manager::userLoggedIn(Settings::UserId id)
{
    int32_t index = m_settings.findUserIndexById(id);
    if (index < 0)
    {
        setWindowTitle("Manager (Not Logged In");
    }
    else
    {
        Settings::User const& user = m_settings.getUser(index);
        setWindowTitle(QString("Manager (%1)").arg(user.descriptor.name.c_str()));
    }
}

//////////////////////////////////////////////////////////////////////////

void Manager::activateBaseStation(Settings::BaseStationId id)
{
    int32_t index = m_settings.findBaseStationIndexById(id);
    if (index < 0)
    {
        s_logger.logError("Tried to activate an inexisting base station");
        assert(false);
        return;
    }

    Settings::BaseStation const& bs = m_settings.getBaseStation(index);
    DB& db = m_settings.getBaseStationDB(index);

    s_logger.logInfo(QString("Activated base station '%1' / %2").arg(bs.descriptor.name.c_str()).arg(getMacStr(bs.descriptor.mac).c_str()).toUtf8().data());

    m_ui.sensorsWidget->init(m_settings, db);
    m_ui.measurementsWidget->init(db);
    m_ui.plotWidget->init(db);
    m_ui.alarmsWidget->init(m_settings, db);
    m_ui.settingsWidget->initBaseStation(id);

    m_ui.actionEmailSettings->setEnabled(true);
    m_ui.actionFtpSettings->setEnabled(true);
    m_ui.actionSensorSettings->setEnabled(true);

    m_activeDB = &db;

    //db.test();
}

//////////////////////////////////////////////////////////////////////////

void Manager::deactivateBaseStation(Settings::BaseStationId id)
{
    int32_t index = m_settings.findBaseStationIndexById(id);
    if (index < 0)
    {
        s_logger.logError("Tried to deactivate an inexisting base station");
        assert(false);
    }
    else
    {
        Settings::BaseStation const& bs = m_settings.getBaseStation(index);
        s_logger.logInfo(QString("Deactivated base station '%1' / %2").arg(bs.descriptor.name.c_str()).arg(getMacStr(bs.descriptor.mac).c_str()).toUtf8().data());
    }

    m_ui.sensorsWidget->shutdown();
    m_ui.measurementsWidget->shutdown();
    m_ui.plotWidget->shutdown();
    m_ui.alarmsWidget->shutdown();
    m_ui.settingsWidget->shutdownBaseStation(id);

    m_ui.actionEmailSettings->setEnabled(false);
    m_ui.actionFtpSettings->setEnabled(false);
    m_ui.actionSensorSettings->setEnabled(false);

    m_activeDB = nullptr;
}

//////////////////////////////////////////////////////////////////////////

void Manager::closeEvent(QCloseEvent* event)
{
    QSettings settings;
    settings.setValue("geometry", saveGeometry());
    settings.setValue("windowState", saveState());

    QMainWindow::closeEvent(event);
}

//////////////////////////////////////////////////////////////////////////

void Manager::readSettings()
{
    QSettings settings;
    restoreGeometry(settings.value("geometry").toByteArray());
    restoreState(settings.value("windowState").toByteArray());
}

//////////////////////////////////////////////////////////////////////////

void Manager::process()
{
//    if (m_comms.is_connected())
//    {
//        m_ui.statusbar->showMessage(("Connected to " + m_remote_address).c_str());
//    }
//    else
//    {
//        m_ui.statusbar->showMessage("");
//    }

    m_comms.process();
}

//////////////////////////////////////////////////////////////////////////

