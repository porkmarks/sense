#pragma once

#include <QWidget>
#include <QStandardItemModel>
#include "ui_SensorsWidget.h"
#include "Comms.h"

class SensorsWidget : public QWidget
{
    Q_OBJECT
public:
    explicit SensorsWidget(QWidget *parent = 0);
    void init(Comms& comms);

signals:

public slots:
    void sensorsReceived(std::vector<Comms::Sensor> const& sensors);
    void baseStationDisconnected(Comms::BaseStation const& bs);

private:
    Ui::SensorsWidget m_ui;
    QStandardItemModel m_model;
    Comms* m_comms = nullptr;

    void addSensor(Comms::Sensor const& sensor);
    void refreshSensor(Comms::Sensor const& sensor);
    std::vector<Comms::Sensor> m_sensors;
};

