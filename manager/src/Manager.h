#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_Manager.h"

#include "Comms.h"
#include "DB.h"
#include "Alarms.h"

class Manager : public QMainWindow
{
public:
    Manager(QWidget* parent = 0);
    ~Manager();

private slots:
    void activateBaseStation(Comms::BaseStation const& bs);

private:
    void process();

    void closeEvent(QCloseEvent* event);

    void readSettings();

    Comms m_comms;
    DB m_db;
    Alarms m_alarms;

    Ui::Manager m_ui;
};


