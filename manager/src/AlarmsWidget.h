#pragma once

#include <QWidget>
#include "ui_AlarmsWidget.h"
#include "Comms.h"
#include "DB.h"
#include "Alarms.h"
#include "AlarmsModel.h"


class AlarmsWidget : public QWidget
{
    Q_OBJECT
public:
    explicit AlarmsWidget(QWidget *parent = 0);
    ~AlarmsWidget();
    void init(Comms& comms, DB& db, Alarms& alarms);

signals:

private slots:
    void baseStationConnected(Comms::BaseStation const& bs);
    void baseStationDisconnected(Comms::BaseStation const& bs);
    void addAlarm();
    void removeAlarms();

private:
    Ui::AlarmsWidget m_ui;
    std::unique_ptr<AlarmsModel> m_model;
    Comms* m_comms = nullptr;
    Alarms* m_alarms = nullptr;
};

