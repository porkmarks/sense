#include "Manager.h"

#include <QInputDialog>
#include <QMessageBox>
#include "ConfigureUserDialog.h"

#include "ui_LoginDialog.h"
#include "ui_AboutDialog.h"
#include "ui_SettingsDialog.h"
#include "ui_BaseStationsDialog.h"
#include "ui_UsersDialog.h"
#include "ui_ReportsDialog.h"
#include "Crypt.h"
#include "Logger.h"
#include "DB.h"

#define SQLITE_HAS_CODEC
#include "sqlite3.h"
#include "Utils.h"

#ifdef NDEBUG
#   define CHECK_PASSWORD
#endif

static const std::string s_version = "1.0.17";

Logger s_logger;
extern std::string s_dataFolder;
extern std::string k_passwordHashReferenceText;

//////////////////////////////////////////////////////////////////////////

Manager::Manager(QWidget *parent)
    : QMainWindow(parent)
	, m_sqlite(nullptr, &sqlite3_close)
{
    m_ui.setupUi(this);

    {
		std::pair<std::string, time_t> bkf = utils::getMostRecentBackup("sense.db", s_dataFolder + "/backups/hourly");
		m_lastHourlyBackupTP = bkf.first.empty() ? IClock::rtNow() : IClock::from_time_t(bkf.second);
		bkf = utils::getMostRecentBackup("sense.db", s_dataFolder + "/backups/daily");
		m_lastDailyBackupTP = bkf.first.empty() ? IClock::rtNow() : IClock::from_time_t(bkf.second);
		bkf = utils::getMostRecentBackup("sense.db", s_dataFolder + "/backups/weekly");
		m_lastWeeklyBackupTP = bkf.first.empty() ? IClock::rtNow() : IClock::from_time_t(bkf.second);
    }

	{
		std::string dataFilename = s_dataFolder + "/sense.db";
		sqlite3* db;
		if (sqlite3_open_v2(dataFilename.c_str(), &db, SQLITE_OPEN_READWRITE, nullptr))
		{
            if (sqlite3_open_v2(dataFilename.c_str(), &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr))
            {
                QMessageBox::critical(this, "Error", QString("Cannot load nor create the database file: %1").arg(sqlite3_errmsg(db)));
                sqlite3_close(db);
                exit(-1);
            }

//            sqlite3_key_v2(db, "sense", "sense", -1);

            sqlite3_exec(db, "PRAGMA journal_mode = WAL;", NULL, NULL, nullptr);
// 			sqlite3_exec(db, "PRAGMA synchronous = OFF;", NULL, NULL, nullptr);
// 			sqlite3_exec(db, "PRAGMA journal_mode = MEMORY;", NULL, NULL, nullptr);
// 			sqlite3_exec(db, "PRAGMA main.locking_mode = EXCLUSIVE;", NULL, NULL, nullptr);

            Result<void> result = Logger::create(*db);
            if (result != success)
			{
				QMessageBox::critical(this, "Error", QString("Cannot create logging db structure: %1").arg(result.error().what().c_str()));
				sqlite3_close(db);
                remove(dataFilename.c_str());
				exit(-1);
			}
			result = DB::create(*db);
			if (result != success)
			{
				QMessageBox::critical(this, "Error", QString("Cannot create db structure: %1").arg(result.error().what().c_str()));
				sqlite3_close(db);
                remove(dataFilename.c_str());
				exit(-1);
			}
		}

        m_sqlite = std::unique_ptr<sqlite3, int(*)(sqlite3*)>(db, &sqlite3_close);
	}

    if (!s_logger.load(*m_sqlite))
    {
        QMessageBox::critical(this, "Error", "Cannot load the logging module.");
        exit(1);
    }
    s_logger.logInfo("Program started");

    Result<void> dbLoadResult = m_db.load(*m_sqlite);
	if (dbLoadResult != success)
    {
        s_logger.logCritical(QString("Cannot load db: %1").arg(dbLoadResult.error().what().c_str()));
        QMessageBox::critical(this, "Error", QString("Cannot load db: %1").arg(dbLoadResult.error().what().c_str()));
        exit(1);
    }

    //m_ui.settingsWidget->init(m_comms, m_db);
    m_ui.logsWidget->init(m_db);

    m_ui.sensorsWidget->init(m_db);
    m_ui.measurementsWidget->init(m_db);
    m_ui.plotWidget->init(m_db);
    m_ui.alarmsWidget->init(m_db);

//     m_ui.actionEmailSettings->setEnabled(true);
//     m_ui.actionFtpSettings->setEnabled(true);
//     m_ui.actionSensorSettings->setEnabled(true);

    //m_ui.baseStationsWidget->init(m_comms);

    auto* timer = new QTimer(this);
    timer->setSingleShot(false);
    timer->start(1);
    connect(timer, &QTimer::timeout, this, &Manager::process, Qt::QueuedConnection);

    connect(m_ui.actionSettings, &QAction::triggered, this, &Manager::showSettingsDialog);
    connect(m_ui.actionBaseStations, &QAction::triggered, this, &Manager::showBaseStationsDialog);
    connect(m_ui.actionUsers, &QAction::triggered, this, &Manager::showUsersDialog);
    connect(m_ui.actionReports, &QAction::triggered, this, &Manager::showReportsDialog);
    connect(m_ui.actionExit, &QAction::triggered, this, &Manager::exitAction);
    connect(m_ui.actionAbout, &QAction::triggered, this, &Manager::showAbout);
    connect(m_ui.actionLogout, &QAction::triggered, this, &Manager::logout);

    m_tabChangedConnection = connect(m_ui.tabWidget, &QTabWidget::currentChanged, this, &Manager::tabChanged);
    m_currentTabIndex = m_ui.tabWidget->currentIndex();

    readSettings();

    show();

	connect(&m_db, &DB::userLoggedIn, this, &Manager::userLoggedIn);
    connect(&m_comms, &Comms::baseStationDiscovered, this, &Manager::baseStationDiscovered);

    checkIfAdminExists();

    login();

    m_comms.init();
}

//////////////////////////////////////////////////////////////////////////

Manager::~Manager()
{
    s_logger.logInfo("Program exit");

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
	if (m_db.needsAdmin())
    {
        s_logger.logInfo("No admin account exists, creating one");

        QMessageBox::information(this, "Admin", "No admin user exists. Please create one now.");

        ConfigureUserDialog dialog(m_db, this);

		DB::User user;
        dialog.setUser(user);
        dialog.setForcedType(DB::UserDescriptor::Type::Admin);

        int result = dialog.exec();
        if (result == QDialog::Accepted)
        {
            user = dialog.getUser();
			if (m_db.addUser(user.descriptor))
            {
                s_logger.logInfo(QString("Admin user '%1' created, logging in").arg(user.descriptor.name.c_str()).toUtf8().data());

                std::optional<DB::User> user = m_db.findUserByName(user->descriptor.name);
                if (user.has_value())
                {
                    m_db.setLoggedInUserId(user->id);
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

        if (m_db.needsAdmin())
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
    s_logger.logInfo("Asking the user to log in");

    QDialog dialog(this);
    Ui::LoginDialog ui;
    ui.setupUi(&dialog);

    size_t attempts = 0;
    while (true)
    {
        int result = dialog.exec();
        if (result == QDialog::Accepted)
        {
            std::optional<DB::User> user = m_db.findUserByName(ui.username->text().toUtf8().data());
            if (!user.has_value())
            {
                attempts++;
                s_logger.logCritical(QString("Invalid login credentials (user not found), attempt %1").arg(attempts).toUtf8().data());
                QMessageBox::critical(this, "Error", QString("Invalid username '%1'.").arg(ui.username->text()));
                continue;
            }

#ifdef CHECK_PASSWORD
            Crypt crypt;
            crypt.setAddRandomSalt(false);
            crypt.setIntegrityProtectionMode(Crypt::ProtectionHash);
            crypt.setCompressionMode(Crypt::CompressionAlways);
            crypt.setKey(ui.password->text());
            std::string passwordHash = crypt.encryptToString(QString(k_passwordHashReferenceText.c_str())).toUtf8().data();
            if (user->descriptor.passwordHash != passwordHash)
            {
                attempts++;
                s_logger.logCritical(QString("Invalid login credentials (wrong password), attempt %1").arg(attempts).toUtf8().data());
                QMessageBox::critical(this, "Error", "Invalid username/password.");
                continue;
            }
#endif

            m_db.setLoggedInUserId(user->id);
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

//////////////////////////////////////////////////////////////////////////

void Manager::logout()
{
    login();
}

//////////////////////////////////////////////////////////////////////////

void Manager::showAbout()
{
    QDialog dialog(this);
    Ui::AboutDialog ui;
    ui.setupUi(&dialog);
    ui.version->setText(s_version.c_str());

    dialog.exec();
}

//////////////////////////////////////////////////////////////////////////

void Manager::showSettingsDialog()
{
	QDialog dialog(this);
	Ui::SettingsDialog ui;
	ui.setupUi(&dialog);
    ui.settingsWidget->init(m_db);

	QSettings settings;
    if (settings.contains("settingsDialogGeometry"))
	{
		dialog.restoreGeometry(settings.value("settingsDialogGeometry").toByteArray());
	}

    if (dialog.exec() == QDialog::Accepted)
    {
        ui.settingsWidget->save();
    }

	settings.setValue("settingsDialogGeometry", dialog.saveGeometry());
}

//////////////////////////////////////////////////////////////////////////

void Manager::showBaseStationsDialog()
{
	QDialog dialog(this);
	Ui::BaseStationsDialog ui;
	ui.setupUi(&dialog);
	ui.baseStationsWidget->init(m_comms, m_db);

	QSettings settings;
	if (settings.contains("baseStationsDialogGeometry"))
	{
		dialog.restoreGeometry(settings.value("baseStationsDialogGeometry").toByteArray());
	}

    dialog.exec();

	settings.setValue("baseStationsDialogGeometry", dialog.saveGeometry());
}

//////////////////////////////////////////////////////////////////////////

void Manager::showUsersDialog()
{
	QDialog dialog(this);
	Ui::UsersDialog ui;
	ui.setupUi(&dialog);
	ui.usersWidget->init(m_db);

	QSettings settings;
	if (settings.contains("usersDialogGeometry"))
	{
		dialog.restoreGeometry(settings.value("usersDialogGeometry").toByteArray());
	}

	dialog.exec();

	settings.setValue("usersDialogGeometry", dialog.saveGeometry());
}

//////////////////////////////////////////////////////////////////////////

void Manager::showReportsDialog()
{
	QDialog dialog(this);
	Ui::ReportsDialog ui;
	ui.setupUi(&dialog);
	ui.reportsWidget->init(m_db);

	QSettings settings;
	if (settings.contains("reportsDialogGeometry"))
	{
		dialog.restoreGeometry(settings.value("reportsDialogGeometry").toByteArray());
	}

	dialog.exec();
	
	settings.setValue("reportsDialogGeometry", dialog.saveGeometry());
}

//////////////////////////////////////////////////////////////////////////

void Manager::exitAction()
{
    exit(0);
}

//////////////////////////////////////////////////////////////////////////

void Manager::userLoggedIn(DB::UserId id)
{
    std::optional<DB::User> user = m_db.findUserById(id);
    if (!user.has_value())
    {
        setWindowTitle("Manager (Not Logged In");
    }
    else
    {
        setWindowTitle(QString("Manager (%1)").arg(user->descriptor.name.c_str()));
    }
}

//////////////////////////////////////////////////////////////////////////

void Manager::baseStationDiscovered(Comms::BaseStationDescriptor const& commsBS)
{
	auto id = std::this_thread::get_id();

    std::optional<DB::BaseStation> bs = m_db.findBaseStationByMac(commsBS.mac);
    if (bs.has_value())
	{
        m_comms.connectToBaseStation(m_db, bs->descriptor.mac);
	}
}

//////////////////////////////////////////////////////////////////////////

void Manager::tabChanged()
{
    if (m_currentTabIndex == m_ui.tabWidget->currentIndex())
    {
        return;
    }

	if (m_currentTabIndex == 1) //measurements out
	{
		m_ui.measurementsWidget->setActive(false);
	}
	if (m_currentTabIndex == 2) //plots out
	{
		m_ui.plotWidget->setActive(false);
	}
    if (m_ui.tabWidget->currentIndex() == 1) //measurements in
    {
		m_ui.measurementsWidget->setActive(true);
    }
    if (m_ui.tabWidget->currentIndex() == 2) //plots in
    {
		m_ui.plotWidget->setActive(true);
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

void Manager::processBackups()
{
	IClock::time_point now = IClock::rtNow();
    if (now - m_lastHourlyBackupTP >= std::chrono::hours(1))
    {
        m_lastHourlyBackupTP = now;
        utils::copyToBackup("sense.db", s_dataFolder + "/sense.db", s_dataFolder + "/backups/hourly", 24);
    }
	if (now - m_lastDailyBackupTP >= std::chrono::hours(24))
	{
		m_lastDailyBackupTP = now;
		utils::copyToBackup("sense.db", s_dataFolder + "/sense.db", s_dataFolder + "/backups/daily", 7);
	}
	if (now - m_lastWeeklyBackupTP >= std::chrono::hours(24 * 7))
	{
		m_lastWeeklyBackupTP = now;
		utils::copyToBackup("sense.db", s_dataFolder + "/sense.db", s_dataFolder + "/backups/weekly", 30);
	}
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

    auto id = std::this_thread::get_id();

	m_db.process();

    processBackups();
}

//////////////////////////////////////////////////////////////////////////

