#include "PlotWidget.h"
#include <QSettings>
#include <array>

#include "ExportPicDialog.h"
#include "ExportDataDialog.h"
#include "SensorsModel.h"
#include "SensorsDelegate.h"
#include "ui_SensorsFilterDialog.h"


static std::array<uint32_t, 128> k_colors =
{
    0xFF000000, 0xFFFFFF00, 0xFF1CE6FF, 0xFFFF34FF, 0xFFFF4A46, 0xFF008941, 0xFF006FA6, 0xFFA30059,
    0xFFFFDBE5, 0xFF7A4900, 0xFF0000A6, 0xFF63FFAC, 0xFFB79762, 0xFF004D43, 0xFF8FB0FF, 0xFF997D87,
    0xFF5A0007, 0xFF809693, 0xFFFEFFE6, 0xFF1B4400, 0xFF4FC601, 0xFF3B5DFF, 0xFF4A3B53, 0xFFFF2F80,
    0xFF61615A, 0xFFBA0900, 0xFF6B7900, 0xFF00C2A0, 0xFFFFAA92, 0xFFFF90C9, 0xFFB903AA, 0xFFD16100,
    0xFFDDEFFF, 0xFF000035, 0xFF7B4F4B, 0xFFA1C299, 0xFF300018, 0xFF0AA6D8, 0xFF013349, 0xFF00846F,
    0xFF372101, 0xFFFFB500, 0xFFC2FFED, 0xFFA079BF, 0xFFCC0744, 0xFFC0B9B2, 0xFFC2FF99, 0xFF001E09,
    0xFF00489C, 0xFF6F0062, 0xFF0CBD66, 0xFFEEC3FF, 0xFF456D75, 0xFFB77B68, 0xFF7A87A1, 0xFF788D66,
    0xFF885578, 0xFFFAD09F, 0xFFFF8A9A, 0xFFD157A0, 0xFFBEC459, 0xFF456648, 0xFF0086ED, 0xFF886F4C,
    0xFF34362D, 0xFFB4A8BD, 0xFF00A6AA, 0xFF452C2C, 0xFF636375, 0xFFA3C8C9, 0xFFFF913F, 0xFF938A81,
    0xFF575329, 0xFF00FECF, 0xFFB05B6F, 0xFF8CD0FF, 0xFF3B9700, 0xFF04F757, 0xFFC8A1A1, 0xFF1E6E00,
    0xFF7900D7, 0xFFA77500, 0xFF6367A9, 0xFFA05837, 0xFF6B002C, 0xFF772600, 0xFFD790FF, 0xFF9B9700,
    0xFF549E79, 0xFFFFF69F, 0xFF201625, 0xFF72418F, 0xFFBC23FF, 0xFF99ADC0, 0xFF3A2465, 0xFF922329,
    0xFF5B4534, 0xFFFDE8DC, 0xFF404E55, 0xFF0089A3, 0xFFCB7E98, 0xFFA4E804, 0xFF324E72, 0xFF6A3A4C,
    0xFF83AB58, 0xFF001C1E, 0xFFD1F7CE, 0xFF004B28, 0xFFC8D0F6, 0xFFA3A489, 0xFF806C66, 0xFF222800,
    0xFFBF5650, 0xFFE83000, 0xFF66796D, 0xFFDA007C, 0xFFFF1A59, 0xFF8ADBB4, 0xFF1E0200, 0xFF5B4E51,
    0xFFC895C5, 0xFF320033, 0xFFFF6832, 0xFF66E1D3, 0xFFCFCDAC, 0xFFD0AC94, 0xFF7ED379, 0xFF012C58
};

//////////////////////////////////////////////////////////////////////////

PlotWidget::PlotWidget(QWidget* parent)
    : QWidget(parent)
{
    m_ui.setupUi(this);
    setEnabled(false);

//    setContextMenuPolicy(Qt::CustomContextMenu);
//    connect(this, &QChartView::customContextMenuRequested, this, &PlotWidget::plotContextMenu);

    createPlotWidgets();
}

//////////////////////////////////////////////////////////////////////////

void PlotWidget::init(DB& db)
{
    for (const QMetaObject::Connection& connection: m_uiConnections)
    {
        QObject::disconnect(connection);
    }
    m_uiConnections.clear();

    setEnabled(true);
    m_db = &db;

    m_uiConnections.push_back(connect(m_ui.clearAnnotations, &QPushButton::released, this, &PlotWidget::clearAnnotations));
    m_uiConnections.push_back(connect(m_ui.dateTimeFilter, &DateTimeFilterWidget::filterChanged, this, &PlotWidget::scheduleFastRefresh, Qt::QueuedConnection));
    m_uiConnections.push_back(connect(m_ui.selectSensors, &QPushButton::released, this, &PlotWidget::selectSensors, Qt::QueuedConnection));
    m_uiConnections.push_back(connect(m_ui.exportData, &QPushButton::released, this, &PlotWidget::exportData));

    m_uiConnections.push_back(connect(m_ui.fitMeasurements, &QCheckBox::stateChanged, this, &PlotWidget::scheduleFastRefresh, Qt::QueuedConnection));
    m_uiConnections.push_back(connect(m_ui.minTemperature, &QDoubleSpinBox::editingFinished, this, &PlotWidget::scheduleFastRefresh, Qt::QueuedConnection));
    m_uiConnections.push_back(connect(m_ui.maxTemperature, &QDoubleSpinBox::editingFinished, this, &PlotWidget::scheduleFastRefresh, Qt::QueuedConnection));
    m_uiConnections.push_back(connect(m_ui.minHumidity, &QDoubleSpinBox::editingFinished, this, &PlotWidget::scheduleFastRefresh, Qt::QueuedConnection));
    m_uiConnections.push_back(connect(m_ui.maxHumidity, &QDoubleSpinBox::editingFinished, this, &PlotWidget::scheduleFastRefresh, Qt::QueuedConnection));

    m_uiConnections.push_back(connect(m_ui.useSmoothing, &QCheckBox::stateChanged, this, &PlotWidget::scheduleFastRefresh, Qt::QueuedConnection));

    m_uiConnections.push_back(connect(m_ui.showTemperature, &QCheckBox::stateChanged, this, &PlotWidget::scheduleFastRefresh, Qt::QueuedConnection));
    m_uiConnections.push_back(connect(m_ui.showHumidity, &QCheckBox::stateChanged, this, &PlotWidget::scheduleFastRefresh, Qt::QueuedConnection));

    m_uiConnections.push_back(connect(m_db, &DB::measurementsAdded, this, &PlotWidget::scheduleSlowRefresh, Qt::QueuedConnection));

    loadSettings();

    refresh();
}

//////////////////////////////////////////////////////////////////////////

void PlotWidget::shutdown()
{
    saveSettings();

    setEnabled(false);

    clearAnnotations();

    m_graphs.clear();
    delete m_plot;
    m_plot = nullptr;
    m_axisD = nullptr;
    m_axisT = nullptr;
    m_axisH = nullptr;

    m_db = nullptr;
}

//////////////////////////////////////////////////////////////////////////

void PlotWidget::loadSettings()
{
    m_ui.dateTimeFilter->loadSettings();

    bool showTemperature = m_ui.showTemperature->isChecked();
    bool showHumidity = m_ui.showHumidity->isChecked();
    bool fitMeasurements = m_ui.fitMeasurements->isChecked();
    bool useSmoothing = m_ui.useSmoothing->isChecked();
    double minHumidity = m_ui.minHumidity->value();
    double maxHumidity = m_ui.maxHumidity->value();
    double minTemperature = m_ui.minTemperature->value();
    double maxTemperature = m_ui.maxTemperature->value();
    std::set<DB::SensorId> selectedSensorIds = m_selectedSensorIds;

    m_ui.showTemperature->blockSignals(true);
    m_ui.showHumidity->blockSignals(true);
    m_ui.fitMeasurements->blockSignals(true);
    m_ui.useSmoothing->blockSignals(true);
    m_ui.minHumidity->blockSignals(true);
    m_ui.maxHumidity->blockSignals(true);
    m_ui.minTemperature->blockSignals(true);
    m_ui.maxTemperature->blockSignals(true);

    QSettings settings;
    m_ui.showTemperature->setChecked(settings.value("filter/showTemperature", true).toBool());
    m_ui.showHumidity->setChecked(settings.value("filter/showHumidity", true).toBool());
    m_ui.fitMeasurements->setChecked(settings.value("filter/fitMeasurements", true).toBool());
    m_ui.useSmoothing->setChecked(settings.value("rendering/useSmoothing", true).toBool());
    m_ui.minHumidity->setValue(settings.value("rendering/minHumidity", 0.0).toDouble());
    m_ui.maxHumidity->setValue(settings.value("rendering/maxHumidity", 100.0).toDouble());
    m_ui.minTemperature->setValue(settings.value("rendering/minTemperature", 0.0).toDouble());
    m_ui.maxTemperature->setValue(settings.value("rendering/maxTemperature", 100.0).toDouble());

    m_ui.showTemperature->blockSignals(false);
    m_ui.showHumidity->blockSignals(false);
    m_ui.fitMeasurements->blockSignals(false);
    m_ui.useSmoothing->blockSignals(false);
    m_ui.minHumidity->blockSignals(false);
    m_ui.maxHumidity->blockSignals(false);
    m_ui.minTemperature->blockSignals(false);
    m_ui.maxTemperature->blockSignals(false);

    m_ui.minTemperature->setEnabled(!m_ui.fitMeasurements->isChecked());
    m_ui.maxTemperature->setEnabled(!m_ui.fitMeasurements->isChecked());
    m_ui.minHumidity->setEnabled(!m_ui.fitMeasurements->isChecked());
    m_ui.maxHumidity->setEnabled(!m_ui.fitMeasurements->isChecked());

    m_selectedSensorIds.clear();
    QList<QVariant> ssid = settings.value("filter/selectedSensors", QList<QVariant>()).toList();
    if (!ssid.empty())
    {
        for (QVariant const& v: ssid)
        {
            bool ok = true;
            DB::SensorId id = v.toUInt(&ok);
            if (ok)
            {
                m_selectedSensorIds.insert(id);
            }
        }
    }
    if (m_db && m_selectedSensorIds.empty()) //if no sensors were selected, select all
    {
        for (size_t i = 0; i < m_db->getSensorCount(); i++)
        {
            DB::Sensor const& sensor = m_db->getSensor(i);
            m_selectedSensorIds.insert(sensor.id);
        }
    }

    if (showTemperature != m_ui.showTemperature->isChecked() ||
            showHumidity != m_ui.showHumidity->isChecked() ||
            fitMeasurements != m_ui.fitMeasurements->isChecked() ||
            useSmoothing != m_ui.useSmoothing->isChecked() ||
            minHumidity != m_ui.minHumidity->value() ||
            maxHumidity != m_ui.maxHumidity->value() ||
            minTemperature != m_ui.minTemperature->value() ||
            maxTemperature != m_ui.maxTemperature->value() ||
            selectedSensorIds != m_selectedSensorIds)
    {
        refresh();
    }
}

//////////////////////////////////////////////////////////////////////////

void PlotWidget::saveSettings()
{
    m_ui.dateTimeFilter->saveSettings();

    QSettings settings;
    settings.setValue("filter/showTemperature", m_ui.showTemperature->isChecked());
    settings.setValue("filter/showHumidity", m_ui.showHumidity->isChecked());
    settings.setValue("filter/fitMeasurements", m_ui.fitMeasurements->isChecked());
    settings.setValue("rendering/useSmoothing", m_ui.useSmoothing->isChecked());
    settings.setValue("rendering/minHumidity", m_ui.minHumidity->value());
    settings.setValue("rendering/maxHumidity", m_ui.maxHumidity->value());
    settings.setValue("rendering/minTemperature", m_ui.minTemperature->value());
    settings.setValue("rendering/maxTemperature", m_ui.maxTemperature->value());

    QList<QVariant> ssid;
    for (DB::SensorId id: m_selectedSensorIds)
    {
        ssid.append(id);
    }
    settings.setValue("filter/selectedSensors", ssid);
}

//////////////////////////////////////////////////////////////////////////

DB::Filter PlotWidget::createFilter() const
{
    DB::Filter filter;

    if (!m_selectedSensorIds.empty())
    {
        filter.useSensorFilter = true;
        filter.sensorIds = m_selectedSensorIds;
    }

    //filter.useTimePointFilter = m_ui.dateTimeFilter->isChecked();
    filter.useTimePointFilter = true;
    filter.timePointFilter.min = DB::Clock::from_time_t(m_ui.dateTimeFilter->getFromDateTime().toTime_t());
    filter.timePointFilter.max = DB::Clock::from_time_t(m_ui.dateTimeFilter->getToDateTime().toTime_t());

    return filter;
}

//////////////////////////////////////////////////////////////////////////

void PlotWidget::scheduleFastRefresh()
{
    scheduleRefresh(std::chrono::milliseconds(500));
}

//////////////////////////////////////////////////////////////////////////

void PlotWidget::scheduleSlowRefresh()
{
    scheduleRefresh(std::chrono::seconds(30));
}

//////////////////////////////////////////////////////////////////////////

void PlotWidget::scheduleRefresh(DB::Clock::duration dt)
{
    int duration = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(dt).count());
    if (m_scheduleTimer && m_scheduleTimer->remainingTime() < duration)
    {
        return;
    }
    m_scheduleTimer.reset(new QTimer());
    m_scheduleTimer->setSingleShot(true);
    m_scheduleTimer->setInterval(duration);
    connect(m_scheduleTimer.get(), &QTimer::timeout, this, &PlotWidget::refresh);
    m_scheduleTimer->start();
}

//////////////////////////////////////////////////////////////////////////

void PlotWidget::refresh()
{
    m_scheduleTimer.reset();

    m_useSmoothing = m_ui.useSmoothing->isChecked();
    m_fitMeasurements = m_ui.fitMeasurements->isChecked();

    DB::Filter filter = createFilter();
    applyFilter(filter);
}

//////////////////////////////////////////////////////////////////////////

void PlotWidget::exportData()
{
    ExportPicDialog dialog(*this, this);
    int result = dialog.exec();
    if (result != QDialog::Accepted)
    {
        return;
    }
}

//////////////////////////////////////////////////////////////////////////

void PlotWidget::selectSensors()
{
    QDialog dialog(this);
    Ui::SensorsFilterDialog ui;
    ui.setupUi(&dialog);

    ui.filter->init(*m_db);

    size_t sensorCount = m_db->getSensorCount();
    if (m_selectedSensorIds.empty()) //if no sensors were selected, select all
    {
        for (size_t i = 0; i < sensorCount; i++)
        {
            ui.filter->getSensorModel().setSensorChecked(m_db->getSensor(i).id, true);
        }
    }
    else
    {
        for (DB::SensorId id: m_selectedSensorIds)
        {
            ui.filter->getSensorModel().setSensorChecked(id, true);
        }
    }

    int result = dialog.exec();
    if (result == QDialog::Accepted)
    {
        m_selectedSensorIds.clear();
        for (size_t i = 0; i < sensorCount; i++)
        {
            DB::Sensor const& sensor = m_db->getSensor(i);
            if (ui.filter->getSensorModel().isSensorChecked(sensor.id))
            {
                m_selectedSensorIds.insert(sensor.id);
            }
        }
        //if no sensors were selected, select all
        if (m_selectedSensorIds.empty()) //if no sensors were selected, select all
        {
            for (size_t i = 0; i < sensorCount; i++)
            {
                m_selectedSensorIds.insert(m_db->getSensor(i).id);
            }
        }
    }

    refresh();
}

//////////////////////////////////////////////////////////////////////////

void PlotWidget::clearAnnotations()
{
    m_draggedAnnotation = nullptr;
    m_annotation.reset();
    m_annotations.clear();
    m_ui.clearAnnotations->setEnabled(false);
    if (m_plot)
    {
        m_annotationsLayer->replot();
    }
}

//////////////////////////////////////////////////////////////////////////

void PlotWidget::createPlotWidgets()
{
    clearAnnotations();

    m_graphs.clear();

    QFont font;
    font.setPointSize(8);

    if (!m_plot)
    {
        m_plot = new QCustomPlot(m_ui.plot);
        //m_plot->setOpenGl(true);

        bool ok = m_plot->addLayer("graphs");
        Q_ASSERT(ok);
        m_graphsLayer = m_plot->layer("graphs");
        Q_ASSERT(m_graphsLayer);
        m_graphsLayer->setMode(QCPLayer::lmBuffered);

        ok = m_plot->addLayer("annotations");
        Q_ASSERT(ok);
        m_annotationsLayer = m_plot->layer("annotations");
        Q_ASSERT(m_annotationsLayer);
        m_annotationsLayer->setMode(QCPLayer::lmBuffered);

        m_plot->setCurrentLayer(m_graphsLayer);

        //m_plot->setInteractions(QCP::Interactions(QCP::Interaction::iSelectLegend));
        connect(m_plot, &QCustomPlot::legendClick, this, &PlotWidget::legendSelectionChanged);

        {
            m_axisD = m_plot->xAxis;
            m_axisD->setTickLabelFont(font);
            m_axisD->setLabel("Date");
            QSharedPointer<QCPAxisTickerDateTime> dateTicker(new QCPAxisTickerDateTime);
            dateTicker->setDateTimeFormat("dd-MMM-yy h:mm");
            dateTicker->setTickStepStrategy(QCPAxisTicker::TickStepStrategy::tssReadability);
            dateTicker->setTickCount(10);
            m_axisD->setTicker(dateTicker);
            m_axisD->setTicks(true);
            m_axisD->setVisible(true);
        }
        {
            m_plot->yAxis->setTickLabelFont(font);
            m_plot->yAxis->setLabel(u8"°C");
            m_plot->yAxis->setNumberFormat("gb");
            m_plot->yAxis->ticker()->setTickStepStrategy(QCPAxisTicker::TickStepStrategy::tssReadability);
            m_plot->yAxis->ticker()->setTickCount(20);
            m_plot->yAxis->setTicks(true);
        }
        {
            m_plot->yAxis2->setTickLabelFont(font);
            m_plot->yAxis2->setLabel(u8"%RH");
            m_plot->yAxis2->setNumberFormat("gb");
            m_plot->yAxis2->ticker()->setTickStepStrategy(QCPAxisTicker::TickStepStrategy::tssReadability);
            m_plot->yAxis2->ticker()->setTickCount(20);
            m_plot->yAxis2->setTicks(true);
        }

        {
            m_plot->legend->setVisible(true);
            QFont legendFont;  // start out with MainWindow's font..
            legendFont.setPointSize(9); // and make a bit smaller for legend
            m_plot->legend->setFont(legendFont);
            m_plot->legend->setBrush(QBrush(QColor(255, 255, 255, 230)));
            // by default, the legend is in the inset layout of the main axis rect. So this is how we access it to change legend placement:
            //m_plot->axisRect()->insetLayout()->setInsetAlignment(0, Qt::AlignTop|Qt::AlignRight);

            m_legendLayout = new QCPLayoutGrid;
            m_plot->plotLayout()->addElement(0, 1, m_legendLayout);
            m_legendLayout->addElement(0, 0, new QCPLayoutElement);
            m_legendLayout->addElement(1, 0, m_plot->legend);
            m_legendLayout->addElement(2, 0, new QCPLayoutElement);
            m_plot->plotLayout()->setColumnStretchFactor(1, 0.001);

            m_plot->legend->setSelectableParts(QCPLegend::spItems);
        }

        m_ui.plot->layout()->addWidget(m_plot);

        connect(m_plot, &QCustomPlot::mousePress, this, &PlotWidget::mousePressEvent);
        connect(m_plot, &QCustomPlot::mouseRelease, this, &PlotWidget::mouseReleaseEvent);
        connect(m_plot, &QCustomPlot::mouseMove, this, &PlotWidget::mouseMoveEvent);
    }

    if (m_ui.showTemperature->isChecked())
    {
        m_axisT = m_plot->yAxis;
        m_axisT->setVisible(true);
    }
    else
    {
        m_axisT = nullptr;
        m_plot->yAxis->setVisible(false);
    }

    if (m_ui.showHumidity->isChecked())
    {
        m_axisH = m_plot->yAxis2;
        m_axisH->setVisible(true);
    }
    else
    {
        m_axisH = nullptr;
        m_plot->yAxis2->setVisible(false);
    }
}

//////////////////////////////////////////////////////////////////////////

void PlotWidget::applyFilter(DB::Filter const& filter)
{
    if (!m_db)
    {
        return;
    }
    m_filter = filter;

    createPlotWidgets();

    m_graphs.clear();
    while (m_plot->graphCount() > 0)
    {
        m_plot->removeGraph(m_plot->graph(0));
    }

    uint64_t minTS = std::numeric_limits<uint64_t>::max();
    uint64_t maxTS = std::numeric_limits<uint64_t>::lowest();
    qreal minT = std::numeric_limits<qreal>::max();
    qreal maxT = std::numeric_limits<qreal>::lowest();
    qreal minH = std::numeric_limits<qreal>::max();
    qreal maxH = std::numeric_limits<qreal>::lowest();

    std::vector<DB::Measurement> measurements = m_db->getFilteredMeasurements(m_filter);

    m_ui.resultCount->setText(QString("%1 out of %2 results.").arg(measurements.size()).arg(m_db->getAllMeasurementCount()));
    m_ui.exportData->setEnabled(!measurements.empty());

    std::sort(measurements.begin(), measurements.end(), [](DB::Measurement const& a, DB::Measurement const& b) { return a.timePoint < b.timePoint; });

    for (size_t i = 0; i < m_db->getSensorCount(); i++)
    {
        DB::Sensor const& sensor = m_db->getSensor(i);
        if (m_selectedSensorIds.find(sensor.id) != m_selectedSensorIds.end())
        {
            GraphData& graphData = m_graphs[sensor.id];
            graphData.sensor = sensor;
            graphData.temperatureLpf.setup(1, 1, 0.15f);
            graphData.humidityLpf.setup(1, 1, 0.15f);
            graphData.temperatureKeys.reserve(8192);
            graphData.temperatureValues.reserve(8192);
            graphData.humidityKeys.reserve(8192);
            graphData.humidityValues.reserve(8192);
            graphData.lastIndex = -1;
        }
    }

    for (DB::Measurement const& m : measurements)
    {
        time_t time = DB::Clock::to_time_t(m.timePoint);
        minTS = std::min(minTS, static_cast<uint64_t>(time));
        maxTS = std::max(maxTS, static_cast<uint64_t>(time));

        auto it = m_graphs.find(m.descriptor.sensorId);
        if (it == m_graphs.end())
        {
            continue;
        }
        GraphData& graphData = it->second;

        bool gap = graphData.lastIndex >= 0 && graphData.lastIndex + 1 != m.descriptor.index;
        graphData.lastIndex = m.descriptor.index;

        {
            qreal value = m.descriptor.temperature;
            if (m_useSmoothing)
            {
                graphData.temperatureLpf.process(value);
            }
            graphData.temperatureKeys.push_back(time);
            graphData.temperatureValues.push_back(gap ? qQNaN() : value);
            minT = std::min(minT, value);
            maxT = std::max(maxT, value);
        }

        {
            qreal value = m.descriptor.humidity;
            if (m_useSmoothing)
            {
                graphData.humidityLpf.process(value);
            }
            graphData.humidityKeys.push_back(time);
            graphData.humidityValues.push_back(gap ? qQNaN() : value);
            minH = std::min(minH, value);
            maxH = std::max(maxH, value);
        }
    }

    m_axisD->setRange(m_ui.dateTimeFilter->getFromDateTime().toTime_t(), m_ui.dateTimeFilter->getToDateTime().toTime_t());

    if (m_fitMeasurements)
    {
        constexpr qreal minTemperatureRange = 5.0;
        if (std::abs(maxT - minT) < minTemperatureRange)
        {
            qreal center = (maxT + minT) / 2.0;
            minT = center - minTemperatureRange / 2.0;
            maxT = center + minTemperatureRange / 2.0;
        }
        constexpr qreal minHumidityRange = 10.0;
        if (std::abs(maxH - minH) < minHumidityRange)
        {
            qreal center = (maxH + minH) / 2.0;
            minH = center - minHumidityRange / 2.0;
            maxH = center + minHumidityRange / 2.0;
        }
    }
    else
    {
        maxT = std::max(m_ui.minTemperature->value(), m_ui.maxTemperature->value());
        minT = std::min(m_ui.minTemperature->value(), m_ui.maxTemperature->value());
        maxH = std::max(m_ui.minHumidity->value(), m_ui.maxHumidity->value());
        minH = std::min(m_ui.minHumidity->value(), m_ui.maxHumidity->value());
    }

    if (m_axisT)
    {
        m_axisT->setRange(minT, maxT);
    }
    if (m_axisH)
    {
        m_axisH->setRange(minH, maxH);
    }

    for (auto& pair : m_graphs)
    {
        GraphData& graphData = pair.second;

        QColor color(k_colors[graphData.sensor.id % k_colors.size()]);
        double brightness = (0.2126*color.redF() + 0.7152*color.greenF() + 0.0722*color.blueF());
        if (brightness > 0.8) //make sure the color is not too bright
        {
            color.setHslF(color.hueF(), color.saturationF(), 0.4);
        }

        if (m_axisT)
        {
            QCPGraph* graph = m_plot->addGraph(m_axisD, m_axisT);
            graph->setLayer(m_graphsLayer);
            QPen pen = graph->pen();
            pen.setWidth(2);
            pen.setColor(color);
            graph->setPen(pen);
            graph->setName(QString("%1 °C").arg(graphData.sensor.descriptor.name.c_str()));

            QVector<double> const& keys = graphData.temperatureKeys;
            QVector<double> const& values = graphData.temperatureValues;
            graph->setData(keys, values);
            graphData.temperatureGraph = graph;
        }
        if (m_axisH)
        {
            QCPGraph* graph = m_plot->addGraph(m_axisD, m_axisH);
            graph->setLayer(m_graphsLayer);
            QPen pen = graph->pen();
            pen.setWidth(2);
            pen.setColor(color);
            pen.setStyle(Qt::DotLine);
            graph->setPen(pen);
            graph->setName(QString("%1 %RH").arg(graphData.sensor.descriptor.name.c_str()));

            QVector<double> const& keys = graphData.humidityKeys;
            QVector<double> const& values = graphData.humidityValues;
            graph->setData(keys, values);
            graphData.humidityGraph = graph;
        }
    }

    m_graphsLayer->replot();
    m_plot->replot();
}

//////////////////////////////////////////////////////////////////////////

void PlotWidget::mousePressEvent(QMouseEvent* event)
{
    QCPAbstractItem* item = qobject_cast<QCPAbstractItem*>(m_plot->itemAt(event->pos()));
    for (std::unique_ptr<PlotToolTip>& annotation: m_annotations)
    {
        if (annotation->mousePressed(item, event->localPos()))
        {
            m_draggedAnnotation = annotation.get();
            break;
        }
    }
}

//////////////////////////////////////////////////////////////////////////

void PlotWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_draggedAnnotation)
    {
        m_draggedAnnotation->mouseReleased(event->localPos());
        m_draggedAnnotation = nullptr;
    }
    else
    {
        keepAnnotation();
    }
}

//////////////////////////////////////////////////////////////////////////

void PlotWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (m_draggedAnnotation)
    {
        m_draggedAnnotation->mouseMoved(event->localPos());
    }
    else
    {
        bool overlaps = false;
        QCPAbstractItem* item = qobject_cast<QCPAbstractItem*>(m_plot->itemAt(event->pos()));
        for (std::unique_ptr<PlotToolTip>& annotation: m_annotations)
        {
            if (annotation->mousePressed(item, event->localPos()))
            {
                overlaps = true;
                break;
            }
        }

        if (!overlaps)
        {
            showAnnotation(event->localPos());
        }
        else
        {
            m_annotation.reset();
        }
    }
    m_annotationsLayer->replot();
}

//////////////////////////////////////////////////////////////////////////

void PlotWidget::resizeEvent(QResizeEvent *event)
{
    for (std::unique_ptr<PlotToolTip>& p: m_annotations)
    {
        p->refresh();
    }
    QWidget::resizeEvent(event);
}

//////////////////////////////////////////////////////////////////////////

void PlotWidget::showAnnotation(const QPointF& pos)
{
    double key = 0;
    double value = 0;
    double bestDistance = std::numeric_limits<double>::max();
    auto computeClosestPoint = [&key, &value, &bestDistance](QCPGraph* graph, const QPointF& pos) -> bool
    {
        bool found = false;
        QCPDataRange dataRange = graph->data()->dataRange();
        QCPGraphDataContainer::const_iterator begin = graph->data()->at(dataRange.begin());
        QCPGraphDataContainer::const_iterator end = graph->data()->at(dataRange.end());
        for (QCPGraphDataContainer::const_iterator it = begin; it < end; it++)
        {
            double x, y;
            graph->coordsToPixels(it->key, it->value, x, y);
            double dx = pos.x() - x;
            double dy = pos.y() - y;
            double distance = std::sqrt(dx*dx + dy*dy);
            if (distance < bestDistance && distance < 50.0)
            {
                key = it->key;
                value = it->value;
                bestDistance = distance;
                found = true;
            }
        }
        return found;
    };

    QCPGraph* bestGraph = nullptr;
    const GraphData* bestGraphData = nullptr;
    bool isTemperature = false;
    for (const auto& pair: m_graphs)
    {
        const GraphData& graphData = pair.second;
        QCPGraph* graph = graphData.temperatureGraph;
        if (graph && computeClosestPoint(graph, pos))
        {
            bestGraph = graph;
            bestGraphData = &graphData;
            isTemperature = true;
        }
        graph = graphData.humidityGraph;
        if (graph && computeClosestPoint(graph, pos))
        {
            bestGraph = graph;
            bestGraphData = &graphData;
            isTemperature = false;
        }
    }

    if (bestGraphData)
    {
        createAnnotation(bestGraph, QPointF(), key, value, true, bestGraphData->sensor, isTemperature);
    }
    else
    {
        m_annotation.reset();
    }
}

//////////////////////////////////////////////////////////////////////////

void PlotWidget::keepAnnotation()
{
    if (m_annotation != nullptr)
    {
        m_annotation->setFixed(true);

        PlotToolTip* annotation = m_annotation.get();
        connect(annotation, &PlotToolTip::closeMe, [this, annotation]()
        {
            auto it = std::find_if(m_annotations.begin(), m_annotations.end(), [annotation](std::unique_ptr<PlotToolTip> const& a) { return a.get() == annotation; });
            if (it != m_annotations.end())
            {
                m_annotations.erase(it);
                m_annotationsLayer->replot();
            }
        });

        m_annotations.push_back(std::move(m_annotation));
        m_ui.clearAnnotations->setEnabled(true);

        //m_annotation.reset(new PlotToolTip(m_chart));
    }
    m_annotationsLayer->replot();
}

//////////////////////////////////////////////////////////////////////////

void PlotWidget::createAnnotation(QCPGraph* graph, QPointF point, double key, double value, bool state, DB::Sensor const& sensor, bool temperature)
{
    if (m_annotation == nullptr)
    {
        m_annotation.reset(new PlotToolTip(m_plot, m_annotationsLayer));
    }

    if (state)
    {
        QColor color = graph->pen().color();
        QDateTime dt;
        dt.setSecsSinceEpoch(static_cast<int64_t>(key));
        if (temperature)
        {
            m_annotation->setText(QString("<p style=\"color:%4;\"><b>%1</b></p>%2<br>Temperature: <b>%3 °C</b>")
                               .arg(sensor.descriptor.name.c_str())
                               .arg(dt.toString("dd-MM-yyyy h:mm"))
                               .arg(value, 0, 'f', 1)
                               .arg(color.name()));
        }
        else
        {
            m_annotation->setText(QString("<p style=\"color:%4;\"><b>%1</b></p>%2<br>Humidity: <b>%3 %RH</b>")
                               .arg(sensor.descriptor.name.c_str())
                               .arg(dt.toString("dd-MM-yyyy h:mm"))
                               .arg(value, 0, 'f', 1)
                               .arg(color.name()));
        }
        m_annotation->setAnchor(graph, point, key, value);
    }
    else
    {
        m_annotation.reset();
    }
}

//////////////////////////////////////////////////////////////////////////

void PlotWidget::plotContextMenu(QPoint const& /*position*/)
{
//    QMenu menu;
//    menu.exec(mapToGlobal(position));
}

//////////////////////////////////////////////////////////////////////////

void PlotWidget::legendSelectionChanged(QCPLegend* l, QCPAbstractLegendItem* ai, QMouseEvent* me)
{
    Q_UNUSED(me);

    if (QCPPlottableLegendItem* legendItem = dynamic_cast<QCPPlottableLegendItem*>(ai))
    {
        QCPAbstractPlottable* plottable = legendItem->plottable();
        if (plottable)
        {
            plottable->setVisible(!plottable->visible());

            QFont font = legendItem->font();
            font.setItalic(!plottable->visible());
            legendItem->setFont(font);

            m_plot->replot();
        }
    }
}

//////////////////////////////////////////////////////////////////////////

QSize PlotWidget::getPlotSize() const
{
    return m_plot->size();
}

//////////////////////////////////////////////////////////////////////////

void PlotWidget::saveToPng(const QString& fileName, bool showLegend, bool showAnnotations)
{
    if (m_annotation)
    {
        m_annotation->setVisible(false);
    }

    for (std::unique_ptr<PlotToolTip>& annotation: m_annotations)
    {
        annotation->setVisible(showAnnotations);
    }

    m_plot->legend->setVisible(showLegend);
    m_plot->plotLayout()->take(m_legendLayout);

    m_plot->savePng(fileName);

    m_plot->plotLayout()->addElement(0, 1, m_legendLayout);
    m_plot->legend->setVisible(true);

    if (m_annotation)
    {
        m_annotation->setVisible(true);
    }
    for (std::unique_ptr<PlotToolTip>& annotation: m_annotations)
    {
        annotation->setVisible(true);
    }
}

//////////////////////////////////////////////////////////////////////////

