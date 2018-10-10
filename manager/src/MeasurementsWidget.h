#pragma once

#include <set>
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

    void loadSettings();
    void saveSettings();

signals:

private slots:
    void scheduleSlowRefresh();
    void scheduleFastRefresh();
    void refresh();
    void selectSensors();
    void exportData();

    void minTemperatureChanged();
    void maxTemperatureChanged();

    void minHumidityChanged();
    void maxHumidityChanged();

private:

    DB::Filter createFilter() const;

    Ui::MeasurementsWidget m_ui;

    void scheduleRefresh(DB::Clock::duration dt);
    std::unique_ptr<QTimer> m_scheduleTimer;

    DB* m_db = nullptr;
    std::unique_ptr<MeasurementsModel> m_model;
    QSortFilterProxyModel m_sortingModel;
    std::unique_ptr<MeasurementsDelegate> m_delegate;
    std::set<DB::SensorId> m_selectedSensorIds;

    std::vector<QMetaObject::Connection> m_uiConnections;
};

