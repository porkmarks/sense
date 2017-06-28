#pragma once

#include <QWidget>
#include <QStandardItemModel>
#include "ui_SensorsWidget.h"
#include "Comms.h"
#include "DB.h"

class SensorsWidget : public QWidget
{
    Q_OBJECT
public:
    explicit SensorsWidget(QWidget *parent = 0);
    void init(Comms& comms, DB& dm);

signals:

private slots:
    void sensorAdded(Comms::Sensor const& sensor);
    void baseStationConnected(Comms::BaseStation const& bs);
    void baseStationDisconnected(Comms::BaseStation const& bs);
    void bindSensor();
    void unbindSensor();

private:
    Ui::SensorsWidget m_ui;
    QStandardItemModel m_model;
    Comms* m_comms = nullptr;
    DB* m_db = nullptr;

    void refreshSensor(Comms::Sensor const& sensor);
    std::vector<Comms::Sensor> m_sensors;
};

