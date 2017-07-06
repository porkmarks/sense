#pragma once

#include <map>
#include <memory>
#include <QWidget>
#include <QtCharts>

#include "PlotToolTip.h"
#include "DB.h"
#include "MeasurementsModel.h"
#include "ui_PlotWidget.h"

class PlotWidget : public QWidget
{
    Q_OBJECT

public:
    PlotWidget(QWidget* parent = 0);

    void refresh(DB& db, MeasurementsModel& model);

private slots:
    void resizeEvent(QResizeEvent* event);
    void keepTooltip();
    void tooltip(QLineSeries* series, QPointF point, bool state, DB::Sensor const& sensor, bool temeprature);

private:

    struct PlotData
    {
        DB::Sensor sensor;
        std::unique_ptr<QLineSeries> temperatureSeries;
        QList<QPointF> temperaturePoints;
        std::unique_ptr<QLineSeries> humiditySeries;
        QList<QPointF> humidityPoints;
    };

    void createPlotWidgets();

    std::unique_ptr<PlotToolTip> m_tooltip;
    std::vector<std::unique_ptr<PlotToolTip>> m_tooltips;

    QChart* m_chart = nullptr;
    QDateTimeAxis* m_axisX = nullptr;
    QValueAxis* m_axisTY = nullptr;
    QValueAxis* m_axisHY = nullptr;
    std::map<DB::SensorId, PlotData> m_series;
    QChartView* m_chartView = nullptr;

    Ui::PlotWidget m_ui;
};

