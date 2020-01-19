#pragma once

#include <QMainWindow>
#include "ui_Manager.h"

#include "Comms.h"
#include "DB.h"
#include "Logger.h"

struct sqlite3;

class Manager : public QMainWindow
{
public:
    Manager(QWidget* parent = 0);
    ~Manager();

private slots:
    void baseStationDiscovered(Comms::BaseStationDescriptor const& commsBS);
    void showSettingsDialog();
    void showBaseStationsDialog();
    void showUsersDialog();
    void showReportsDialog();
    void showAbout();
    void exitAction();
    void logout();

private:
    void processBackups();
    void process();
	void userLoggedIn(DB::UserId id);
    void checkIfAdminExists();
    void login();

    void closeEvent(QCloseEvent* event) override;

    void readSettings();

    void tabChanged();

    QMetaObject::Connection m_tabChangedConnection;

	std::unique_ptr<sqlite3, int(*)(sqlite3*)> m_sqlite;

    Comms m_comms;
	DB m_db;

    Ui::Manager m_ui;
    int m_currentTabIndex = 0;

    IClock::time_point m_lastHourlyBackupTP;
    IClock::time_point m_lastDailyBackupTP;
    IClock::time_point m_lastWeeklyBackupTP;
};


