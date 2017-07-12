#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_Manager.h"

#include "Comms.h"
#include "Emailer.h"
#include "DB.h"

class Manager : public QMainWindow
{
public:
    Manager(QWidget* parent = 0);
    ~Manager();

private slots:
    void activateBaseStation(Comms::BaseStationDescriptor const& bs, DB& db);
    void deactivateBaseStation(Comms::BaseStationDescriptor const& bs);
    void openSensorSettingsDialog();
    void openEmailSettingsDialog();
    void openFtpSettingsDialog();

private:
    void process();

    void closeEvent(QCloseEvent* event);

    void readSettings();

    Comms m_comms;
    Emailer m_emailer;
    DB* m_activeDB = nullptr;

    Ui::Manager m_ui;
};


