#pragma once

#include <map>
#include <memory>
#include <set>
#include <QWidget>
#include "qcustomplot.h"

#include "PlotToolTip.h"
#include "DB.h"
#include "Butterworth.h"
#include "ui_PlotWidget.h"


class PlotWidget : public QWidget
{
    Q_OBJECT

public:
    PlotWidget(QWidget* parent = nullptr);

    void init(DB& db);
    void shutdown();

    void loadSettings();
    void saveSettings();

    QSize getPlotSize() const;
    void saveToPng(const QString& fileName, bool showLegend);

private slots:
    void resizeEvent(QResizeEvent* event);
    void keepAnnotation();
    void createAnnotation(QCPGraph* graph, QPointF point, double key, double value, bool state, DB::Sensor const& sensor, bool temperature);
    void plotContextMenu(QPoint const& position);
    void handleMarkerClicked();
    void refresh();
    void selectSensors();
    void exportData();
    void mousePressEvent(QMouseEvent* event);
    void mouseReleaseEvent(QMouseEvent* event);
    void mouseMoveEvent(QMouseEvent* event);

private:
    void applyFilter(DB::Filter const& filter);

    DB::Filter createFilter() const;
    void clearAnnotations();
//    void setMarkerVisible(QLegendMarker* marker, bool visible);
    void showAnnotation(const QPointF& pos);

    DB* m_db = nullptr;
    DB::Filter m_filter;

    struct GraphData
    {
        DB::Sensor sensor;
        QCPGraph* temperatureGraph = nullptr;
        util::Butterworth<qreal> temperatureLpf;
        QVector<double> temperatureKeys;
        QVector<double> temperatureValues;
        QCPGraph* humidityGraph = nullptr;
        util::Butterworth<qreal> humidityLpf;
        QVector<double> humidityKeys;
        QVector<double> humidityValues;
        int64_t lastIndex = -1;
    };

    void createPlotWidgets();

    std::unique_ptr<PlotToolTip> m_annotation;
    std::vector<std::unique_ptr<PlotToolTip>> m_annotations;

//    QChart* m_chart = nullptr;
//    QDateTimeAxis* m_axisX = nullptr;
//    QValueAxis* m_axisTY = nullptr;
//    QValueAxis* m_axisHY = nullptr;
    std::map<DB::SensorId, GraphData> m_graphs;
//    QChartView* m_chartView = nullptr;

    QCustomPlot* m_plot = nullptr;
    QCPAxis* m_axisD = nullptr;
    QCPAxis* m_axisT = nullptr;
    QCPAxis* m_axisH = nullptr;

    bool m_useSmoothing = true;
    bool m_fitMeasurements = true;
    std::set<DB::SensorId> m_selectedSensorIds;

    Ui::PlotWidget m_ui;
    std::vector<QMetaObject::Connection> m_uiConnections;

    PlotToolTip* m_draggedAnnotation = nullptr;
    //bool m_isDragging = false;
};

