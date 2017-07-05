#pragma once

#include <map>
#include <memory>
#include <QWidget>
#include <QtCharts>

#include "MeasurementsModel.h"
#include "ui_PlotWidget.h"

class PlotWidget : public QWidget
{
public:
    PlotWidget(QWidget* parent = 0);

    void refresh(DB& db, MeasurementsModel& model);

private:

    struct PlotData
    {
        std::unique_ptr<QLineSeries> temperatureSeries;
        QList<QPointF> temperaturePoints;
        std::unique_ptr<QLineSeries> humiditySeries;
        QList<QPointF> humidityPoints;
    };

    struct Plot
    {
        QChart* chart = nullptr;
        QDateTimeAxis* axisX = nullptr;
        QValueAxis* axisTY = nullptr;
        QValueAxis* axisHY = nullptr;
        std::map<DB::SensorId, PlotData> series;
        QChartView* chartView = nullptr;
    };

    void createPlotWidgets();

    Plot m_plot;

    Ui::PlotWidget m_ui;
};

