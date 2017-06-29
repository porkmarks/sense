#pragma once

#include <QWidget>
#include <QStandardItemModel>
#include "ui_SensorsWidget.h"
#include "Comms.h"
#include "DB.h"
#include "SensorsModel.h"

class SensorsWidget : public QWidget
{
    Q_OBJECT
public:
    explicit SensorsWidget(QWidget *parent = 0);
    ~SensorsWidget();
    void init(Comms& comms, DB& dm);

signals:

private slots:
    void baseStationConnected(Comms::BaseStation const& bs);
    void baseStationDisconnected(Comms::BaseStation const& bs);
    void bindSensor();
    void unbindSensor();

private:
    Ui::SensorsWidget m_ui;
    std::unique_ptr<SensorsModel> m_model;
    Comms* m_comms = nullptr;
};

