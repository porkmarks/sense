#pragma once

#include <QWidget>
#include <QStandardItemModel>
#include <QSortFilterProxyModel>
#include "ui_MeasurementsWidget.h"
#include "DB.h"
#include "MeasurementsModel.h"
#include "MeasurementsDelegate.h"

class MeasurementsWidget : public QWidget
{
    Q_OBJECT
public:
    explicit MeasurementsWidget(QWidget *parent = 0);
    ~MeasurementsWidget();
    void init(DB& db);
    void shutdown();

signals:

private slots:
    void refresh();
    void setMinDateTimeNow();
    void setMaxDateTimeNow();
    void selectSensors();
    void exportData();

    void setDateTimePreset(int preset);
    void setDateTimePresetToday();
    void setDateTimePresetYesterday();
    void setDateTimePresetThisWeek();
    void setDateTimePresetLastWeek();
    void setDateTimePresetThisMonth();
    void setDateTimePresetLastMonth();

    void minDateTimeChanged();
    void maxDateTimeChanged();

    void minTemperatureChanged();
    void maxTemperatureChanged();

    void minHumidityChanged();
    void maxHumidityChanged();

private:

    DB::Filter createFilter() const;

    Ui::MeasurementsWidget m_ui;

    DB* m_db = nullptr;
    std::unique_ptr<MeasurementsModel> m_model;
    QSortFilterProxyModel m_sortingModel;
    std::unique_ptr<MeasurementsDelegate> m_delegate;
    std::vector<DB::SensorId> m_selectedSensorIds;
};

