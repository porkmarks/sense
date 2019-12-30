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
    void activateBaseStation(DB::BaseStationId id);
    void deactivateBaseStation(DB::BaseStationId id);
    void showAbout();
    void exitAction();
    void logout();

private:
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

	std::unique_ptr<sqlite3, int(*)(sqlite3*)> m_sqlite;
};


