#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_Manager.h"

#include "Comms.h"
#include "DB.h"

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

    Ui::Manager m_ui;
};


