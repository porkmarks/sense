#pragma once

#include <map>
#include <memory>
#include <QWidget>
#include <QtCharts>

#include "PlotToolTip.h"
#include "DB.h"
#include "Butterworth.h"
#include "ui_PlotWidget.h"


class PlotWidget : public QWidget
{
    Q_OBJECT

public:
    PlotWidget(QWidget* parent = 0);

    void init(DB& db);
    void shutdown();
    void refresh(DB::Filter const& filter);

    QSize getPlotSize() const;
    QPixmap grabPic(bool showLegend);

private slots:
    void resizeEvent(QResizeEvent* event);
    void keepTooltip();
    void tooltip(QLineSeries* series, QPointF point, bool state, DB::Sensor const& sensor, bool temeprature);
    void plotContextMenu(QPoint const& position);
    void handleMarkerClicked();
    void refreshFromDB();
    void selectSensors();
    void exportData();

    void setDateTimePreset(int preset);
    void setDateTimePresetToday();
    void setDateTimePresetYesterday();
    void setDateTimePresetThisWeek();
    void setDateTimePresetLastWeek();
    void setDateTimePresetThisMonth();
    void setDateTimePresetLastMonth();

    void minDateTimeChanged(QDateTime const& value);
    void maxDateTimeChanged(QDateTime const& value);

private:
    DB::Filter createFilter() const;
    void setMarkerVisible(QLegendMarker* marker, bool visible);

    DB* m_db = nullptr;
    DB::Filter m_filter;

    struct PlotData
    {
        DB::Sensor sensor;
        std::unique_ptr<QLineSeries> temperatureSeries;
        util::Butterworth<qreal> temperatureLpf;
        QList<QPointF> temperaturePoints;
        std::unique_ptr<QLineSeries> humiditySeries;
        util::Butterworth<qreal> humidityLpf;
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

    bool m_useFiltering = true;
    std::vector<DB::SensorId> m_selectedSensorIds;

    Ui::PlotWidget m_ui;
};

