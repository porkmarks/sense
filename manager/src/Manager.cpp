#include "Manager.h"

#include <QInputDialog>
#include <QMessageBox>

#include "ui_LoginDialog.h"

#include "ConfigureUserDialog.h"
#include "SensorSettingsDialog.h"
#include "EmailSettingsDialog.h"
#include "FtpSettingsDialog.h"

#include "Crypt.h"

extern std::string k_passwordHashReferenceText;

//////////////////////////////////////////////////////////////////////////

Manager::Manager(QWidget *parent)
    : QMainWindow(parent)
{
    m_ui.setupUi(this);

    if (!m_settings.load("settings"))
    {
        if (!m_settings.create("settings"))
        {
            QMessageBox::critical(this, "Error", "Cannot create the settings file.");
            exit(1);
        }
    }

    m_ui.settingsWidget->init(m_comms, m_settings);

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
//    delete m_ui.baseStationsWidget;
    delete m_ui.sensorsWidget;
    delete m_ui.measurementsWidget;
    delete m_ui.alarmsWidget;
//    delete m_ui.reportsWidget;
}

//////////////////////////////////////////////////////////////////////////

void Manager::checkIfAdminExists()
{
    if (m_settings.needsAdmin())
    {
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
                int32_t userIndex = m_settings.findUserIndexByName(user.descriptor.name);
                if (userIndex >= 0)
                {
                    m_settings.setLoggedInUserId(m_settings.getUser(userIndex).id);
                }
            }
        }

        if (m_settings.needsAdmin())
        {
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
        QDialog dialog;
        Ui::LoginDialog ui;
        ui.setupUi(&dialog);

        while (true)
        {
            int result = dialog.exec();
            if (result == QDialog::Accepted)
            {
                int32_t userIndex = m_settings.findUserIndexByName(ui.username->text().toUtf8().data());
                if (userIndex < 0)
                {
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
                    QMessageBox::critical(this, "Error", "Invalid username/password.");
                    continue;
                }

                m_settings.setLoggedInUserId(user.id);
                break;
            }
            else
            {
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
        setWindowTitle(QString("Manager (%1)").arg(m_settings.getUser(index).descriptor.name.c_str()));
    }
}

//////////////////////////////////////////////////////////////////////////

void Manager::activateBaseStation(Settings::BaseStationId id)
{
    int32_t index = m_settings.findBaseStationIndexById(id);
    if (index < 0)
    {
        assert(false);
        return;
    }

    DB& db = m_settings.getBaseStationDB(index);

    m_ui.sensorsWidget->init(db);
    m_ui.measurementsWidget->init(db);
    m_ui.plotWidget->init(db);
    m_ui.alarmsWidget->init(db);
    m_ui.settingsWidget->initDB(db);

    m_ui.actionEmailSettings->setEnabled(true);
    m_ui.actionFtpSettings->setEnabled(true);
    m_ui.actionSensorSettings->setEnabled(true);

    m_activeDB = &db;

    //db.test();
}

//////////////////////////////////////////////////////////////////////////

void Manager::deactivateBaseStation(Settings::BaseStationId id)
{
    m_ui.sensorsWidget->shutdown();
    m_ui.measurementsWidget->shutdown();
    m_ui.plotWidget->shutdown();
    m_ui.alarmsWidget->shutdown();
    m_ui.settingsWidget->shutdownDB();;

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

