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
    void activateBaseStation(Comms::BaseStationDescriptor const& bs, DB& db);
    void deactivateBaseStation(Comms::BaseStationDescriptor const& bs);

private:
    void process();

    void closeEvent(QCloseEvent* event);

    void readSettings();

    Comms m_comms;

    Ui::Manager m_ui;
};


