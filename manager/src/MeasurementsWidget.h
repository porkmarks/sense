#pragma once

#include <QWidget>
#include <QStandardItemModel>
#include "ui_MeasurementsWidget.h"
#include "Comms.h"
#include "DB.h"
#include "MeasurementsModel.h"

class MeasurementsWidget : public QWidget
{
    Q_OBJECT
public:
    explicit MeasurementsWidget(QWidget *parent = 0);
    void init(Comms& comms, DB& db);

signals:

private slots:
    void baseStationConnected(Comms::BaseStation const& bs);
    void baseStationDisconnected(Comms::BaseStation const& bs);

    void refreshFromDB();
    void setMinDateTimeNow();
    void setMaxDateTimeNow();
    void setDateTimeThisDay();
    void setDateTimeThisWeek();
    void setDateTimeThisMonth();
    void selectSensors();

    void minDateTimeChanged(QDateTime const& value);
    void maxDateTimeChanged(QDateTime const& value);

    void minTemperatureChanged();
    void maxTemperatureChanged();

    void minHumidityChanged();
    void maxHumidityChanged();

private:

    DB::Filter createFilter() const;

    Ui::MeasurementsWidget m_ui;

    Comms* m_comms = nullptr;
    DB* m_db = nullptr;
    std::unique_ptr<MeasurementsModel> m_model;
    std::vector<Comms::Sensor_Id> m_selectedSensorIds;
};

