#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_Manager.h"

#include "Comms.h"

class Manager : public QMainWindow
{
public:
    Manager(QWidget* parent = 0);
    ~Manager();

private:
    void process();

    void closeEvent(QCloseEvent* event);

    void readSettings();

    Ui::Manager m_ui;

    Comms m_comms;
};


