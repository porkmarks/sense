#pragma once

#include <QtWidgets/QMainWindow>
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
    void userLoggedIn(Settings::UserId id);
    void checkIfAdminExists();
    void login();

    void closeEvent(QCloseEvent* event) override;

    void readSettings();

    void tabChanged();

    QMetaObject::Connection m_tabChangedConnection;

    Comms m_comms;
    Settings m_settings;

    Ui::Manager m_ui;
    int m_currentTabIndex = 0;

    Settings::Clock::time_point m_lastHourlyBackupTP;
    Settings::Clock::time_point m_lastDailyBackupTP;
    Settings::Clock::time_point m_lastWeeklyBackupTP;

	std::unique_ptr<sqlite3, int(*)(sqlite3*)> m_sqlite;
};


