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
    void saveToPng(const QString& fileName, bool showLegend, bool showAnnotations);

private slots:
    void resizeEvent(QResizeEvent* event);
    void keepAnnotation();
    void createAnnotation(DB::SensorId sensorId, QPointF point, double key, double value, DB::Sensor const& sensor, bool temperature);
    void plotContextMenu(QPoint const& position);
    void refresh();
    void scheduleSlowRefresh();
    void scheduleFastRefresh();
    void selectSensors();
	void sensorAdded(DB::SensorId id);
	void sensorRemoved(DB::SensorId id);
    void exportData();
    void mousePressEvent(QMouseEvent* event);
    void mouseReleaseEvent(QMouseEvent* event);
    void mouseMoveEvent(QMouseEvent* event);
    void legendSelectionChanged(QCPLegend* l, QCPAbstractLegendItem* ai, QMouseEvent* me);

private:
    void applyFilter(DB::Filter const& filter);

    DB::Filter createFilter() const;
    void clearAnnotations();
//    void setMarkerVisible(QLegendMarker* marker, bool visible);
    void showAnnotation(const QPointF& pos);

    DB* m_db = nullptr;
    DB::Filter m_filter;

    void scheduleRefresh(DB::Clock::duration dt);
    std::unique_ptr<QTimer> m_scheduleTimer;

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

    struct Annotation
    {
        std::unique_ptr<PlotToolTip> toolTip;
        DB::SensorId sensorId;
        bool isTemperature = false;
    };

    Annotation m_annotation;
    std::vector<Annotation> m_annotations;

	QCPGraph* findAnnotationGraph(const Annotation& annotation) const;

//    QChart* m_chart = nullptr;
//    QDateTimeAxis* m_axisX = nullptr;
//    QValueAxis* m_axisTY = nullptr;
//    QValueAxis* m_axisHY = nullptr;
    std::map<DB::SensorId, GraphData> m_graphs;
//    QChartView* m_chartView = nullptr;

    QCPLayoutGrid* m_legendLayout = nullptr;

    QCustomPlot* m_plot = nullptr;
    QCPLayer* m_graphsLayer = nullptr;
    QCPLayer* m_annotationsLayer = nullptr;
    QCPAxis* m_axisD = nullptr;
    QCPAxis* m_axisT = nullptr;
    QCPAxis* m_axisH = nullptr;

    bool m_useSmoothing = true;
    bool m_fitMeasurements = true;
    std::set<DB::SensorId> m_selectedSensorIds;

    Ui::PlotWidget m_ui;
    std::vector<QMetaObject::Connection> m_uiConnections;

    int32_t m_draggedAnnotationIndex = -1;
    //bool m_isDragging = false;
};

