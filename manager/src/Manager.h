#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_Manager.h"

#include "Comms.h"
#include "DB.h"
#include "Logger.h"

class Manager : public QMainWindow
{
public:
    Manager(QWidget* parent = 0);
    ~Manager();

private slots:
    void activateBaseStation(Settings::BaseStationId id);
    void deactivateBaseStation(Settings::BaseStationId id);

private:
    void process();
    void userLoggedIn(Settings::UserId id);
    void checkIfAdminExists();
    void login();

    void closeEvent(QCloseEvent* event);

    void readSettings();

    Comms m_comms;
    Settings m_settings;
    Logger m_logger;
    DB* m_activeDB = nullptr;

    Ui::Manager m_ui;
};


