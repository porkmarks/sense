#include "Manager.h"

#include <QInputDialog>
#include <QMessageBox>
#include "ConfigureUserDialog.h"

#include "ui_LoginDialog.h"
#include "Crypt.h"

#ifdef NDEBUG
#   define CHECK_PASSWORD
#endif

static const std::string s_version = "1.0.10";

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

    statusBar()->showMessage(("Version: " + s_version).c_str(), 0);

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
            s_logger.logCritical("Cannot create the settings file");
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

    m_tabChangedConnection = connect(m_ui.tabWidget, &QTabWidget::currentChanged, this, &Manager::tabChanged);
    m_currentTabIndex = m_ui.tabWidget->currentIndex();

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
    s_logger.shutdown();

    QObject::disconnect(m_tabChangedConnection);

    delete m_ui.sensorsWidget;
    delete m_ui.measurementsWidget;
    delete m_ui.alarmsWidget;
    delete m_ui.logsWidget;
    delete m_ui.tabWidget;
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
                    m_settings.setLoggedInUserId(m_settings.getUser(static_cast<size_t>(userIndex)).id);
                }
                else
                {
                    s_logger.logCritical("Internal consistency error: user not found after adding\nThe program will now close.");
                    QMessageBox::critical(this, "Error", "Internal consistency error: user not found after adding\nThe program will now close.");
                    exit(1);
                }
            }
            else
            {
                s_logger.logCritical(QString("Cannot add user %1").arg(user.descriptor.name.c_str()));
                QMessageBox::critical(this, "Error", QString("Cannot add user %1").arg(user.descriptor.name.c_str()));
            }
        }
        else
        {
            s_logger.logWarning("User cancelled admin creation dialog");
        }

        if (m_settings.needsAdmin())
        {
            s_logger.logCritical("User failed to create an admin account. Exiting");
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
                int32_t _userIndex = m_settings.findUserIndexByName(ui.username->text().toUtf8().data());
                if (_userIndex < 0)
                {
                    attempts++;
                    s_logger.logCritical(QString("Invalid login credentials (user not found), attempt %1").arg(attempts).toUtf8().data());
                    QMessageBox::critical(this, "Error", "Invalid username/password.");
                    continue;
                }

                size_t userIndex = static_cast<size_t>(_userIndex);
                Settings::User const& user = m_settings.getUser(userIndex);

#ifdef CHECK_PASSWORD
                Crypt crypt;
                crypt.setAddRandomSalt(false);
                crypt.setIntegrityProtectionMode(Crypt::ProtectionHash);
                crypt.setCompressionMode(Crypt::CompressionAlways);
                crypt.setKey(ui.password->text());
                std::string passwordHash = crypt.encryptToString(QString(k_passwordHashReferenceText.c_str())).toUtf8().data();
                if (user.descriptor.passwordHash != passwordHash)
                {
                    attempts++;
                    s_logger.logCritical(QString("Invalid login credentials (wrong password), attempt %1").arg(attempts).toUtf8().data());
                    QMessageBox::critical(this, "Error", "Invalid username/password.");
                    continue;
                }
#endif

                m_settings.setLoggedInUserId(user.id);
                break;
            }
            else
            {
                s_logger.logCritical("User failed to log in. Exiting");
                QMessageBox::critical(this, "Error", "You need to be logged in to user this program.\nThe program will now close.");
                exit(1);
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////////

void Manager::userLoggedIn(Settings::UserId id)
{
    int32_t _index = m_settings.findUserIndexById(id);
    if (_index < 0)
    {
        setWindowTitle("Manager (Not Logged In");
    }
    else
    {
        size_t index = static_cast<size_t>(_index);
        Settings::User const& user = m_settings.getUser(index);
        setWindowTitle(QString("Manager (%1)").arg(user.descriptor.name.c_str()));
    }
}

//////////////////////////////////////////////////////////////////////////

void Manager::tabChanged()
{
    if (m_currentTabIndex == 1 && m_ui.tabWidget->currentIndex() == 2) //measurements to plots
    {
        m_ui.measurementsWidget->saveSettings();
        m_ui.plotWidget->loadSettings();
    }
    else if (m_currentTabIndex == 2 && m_ui.tabWidget->currentIndex() == 1) //plots to measurements
    {
        m_ui.plotWidget->saveSettings();
        m_ui.measurementsWidget->loadSettings();
    }

    if (m_ui.tabWidget->currentWidget() == m_ui.logsWidget)
    {
        m_ui.logsWidget->setAutoRefresh(true);
        m_ui.logsWidget->refresh();
    }
    else
    {
        m_ui.logsWidget->setAutoRefresh(false);
    }

    m_currentTabIndex = m_ui.tabWidget->currentIndex();
}

//////////////////////////////////////////////////////////////////////////

void Manager::activateBaseStation(Settings::BaseStationId id)
{
    int32_t _index = m_settings.findBaseStationIndexById(id);
    if (_index < 0)
    {
        s_logger.logCritical("Tried to activate an inexisting base station");
        assert(false);
        return;
    }

    size_t index = static_cast<size_t>(_index);
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
    int32_t _index = m_settings.findBaseStationIndexById(id);
    if (_index < 0)
    {
        s_logger.logCritical("Tried to deactivate an inexisting base station");
        assert(false);
    }
    else
    {
        size_t index = static_cast<size_t>(_index);
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
    s_logger.process();
    m_settings.process();
}

//////////////////////////////////////////////////////////////////////////

