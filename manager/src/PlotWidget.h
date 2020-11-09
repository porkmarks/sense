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

    void setActive(bool active);

    QSize getPlotSize() const;
    void saveToPng(const QString& fileName, bool showLegend, bool showAnnotations);

	enum class PlotType
	{
		Temperature,
		Humidity,
		Battery,
		Signal,
		Count
	};

private slots:
    void setPermissions();
    void resizeEvent(QResizeEvent* event);
    void keepAnnotation();
    void createAnnotation(DB::SensorId sensorId, QPointF point, double key, double value, DB::Sensor const& sensor, PlotType plotType);
    void plotContextMenu(QPoint const& position);
    void refresh();
    void scheduleSlowRefresh();
    void scheduleMediumRefresh();
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

    void filterChanged();
    bool canAutoRefresh() const;

    DB::Filter createFilter() const;
    void clearAnnotations();
//    void setMarkerVisible(QLegendMarker* marker, bool visible);
    void showAnnotation(const QPointF& pos);

    DB* m_db = nullptr;
    DB::Filter m_filter;

	void scheduleRefresh(IClock::duration dt);
    std::unique_ptr<QTimer> m_scheduleTimer;

    struct Plot
    {
		QCPGraph* graph = nullptr;
        std::optional<double> oldValue;
		QVector<double> keys;
		QVector<double> values;
        struct AlarmIndicator
        {
            double key = 0;
            double value = 0;
            DB::AlarmTriggers triggers;
        };
		QVector<AlarmIndicator> alarmIndicators;
		QCPAxis* axis = nullptr;
    };

    struct GraphData
    {
        DB::Sensor sensor;
        std::array<Plot, (size_t)PlotType::Count> plots;
        int64_t lastIndex = -1;
    };

    void createPlotWidgets();

    struct Annotation
    {
        std::unique_ptr<PlotToolTip> toolTip;
        DB::SensorId sensorId;
        PlotType plotType = PlotType::Temperature;
    };

    Annotation m_annotation;
    std::vector<Annotation> m_annotations;

    const Plot* findAnnotationPlot(const Annotation& annotation) const;
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
    QCPAxis* m_axisDate = nullptr;
    std::array<QCPAxis*, (size_t)PlotType::Count> m_plotAxis;
    std::vector<QCPItemTracer*> m_indicatorItems;

    std::set<DB::SensorId> m_selectedSensorIds;

    Ui::PlotWidget m_ui;
    std::vector<QMetaObject::Connection> m_uiConnections;

    int32_t m_draggedAnnotationIndex = -1;
    bool m_isApplyingFilter = false;
    //bool m_isDragging = false;
};

