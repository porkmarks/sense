#include "PlotWidget.h"
#include <QSettings>
#include <array>

#include "ExportPicDialog.h"
#include "ExportDataDialog.h"
#include "SensorsModel.h"
#include "SensorsDelegate.h"
#include "ui_SensorsFilterDialog.h"
#include "Utils.h"


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

std::array<const char*, (size_t)PlotWidget::PlotType::Count> k_plotUnits = { "°C", "%RH", "%", "%" };
std::array<Qt::PenStyle, (size_t)PlotWidget::PlotType::Count> k_plotPenStyles = { Qt::SolidLine, Qt::DashLine, Qt::DotLine, Qt::DashDotDotLine };
std::array<const char*, (size_t)PlotWidget::PlotType::Count> k_plotNames = { "Temperature", "Humidity", "Battery", "Signal" };
std::array<qreal, (size_t)PlotWidget::PlotType::Count> k_plotMinRange = { 5.0, 10.0, 0.1, 0.1 };

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

    m_uiConnections.push_back(connect(&db, &DB::userLoggedIn, this, &PlotWidget::setPermissions));

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
    m_uiConnections.push_back(connect(m_ui.showBattery, &QCheckBox::stateChanged, this, &PlotWidget::scheduleFastRefresh, Qt::QueuedConnection));
    m_uiConnections.push_back(connect(m_ui.showSignal, &QCheckBox::stateChanged, this, &PlotWidget::scheduleFastRefresh, Qt::QueuedConnection));

    m_uiConnections.push_back(connect(m_db, &DB::measurementsAdded, this, &PlotWidget::scheduleSlowRefresh, Qt::QueuedConnection));
    m_uiConnections.push_back(connect(m_db, &DB::measurementsChanged, this, &PlotWidget::scheduleFastRefresh, Qt::QueuedConnection));
	m_uiConnections.push_back(connect(m_db, &DB::sensorAdded, this, &PlotWidget::sensorAdded, Qt::QueuedConnection));
	m_uiConnections.push_back(connect(m_db, &DB::sensorRemoved, this, &PlotWidget::sensorRemoved, Qt::QueuedConnection));

    loadSettings();
    refresh();

    setPermissions();
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
    m_axisDate = nullptr;
    for (QCPAxis*& axis : m_plotAxis)
    {
        axis = nullptr;
    }

    m_db = nullptr;
}

//////////////////////////////////////////////////////////////////////////

void PlotWidget::setPermissions()
{
	if (!m_db->isLoggedInAsAdmin())
	{
		m_ui.showBattery->setVisible(false);
		m_ui.showBattery->setChecked(false);
		m_ui.showSignal->setVisible(false);
		m_ui.showSignal->setChecked(false);
	}
    else
    {
		m_ui.showBattery->setVisible(true);
		m_ui.showSignal->setVisible(true);
    }
}

//////////////////////////////////////////////////////////////////////////

void PlotWidget::loadSettings()
{
    m_ui.dateTimeFilter->loadSettings();

    bool showTemperature = m_ui.showTemperature->isChecked();
    bool showHumidity = m_ui.showHumidity->isChecked();
    bool showBattery = m_ui.showBattery->isChecked();
    bool showSignal = m_ui.showSignal->isChecked();
    bool fitMeasurements = m_ui.fitMeasurements->isChecked();
    bool useSmoothing = m_ui.useSmoothing->isChecked();
    double minHumidity = m_ui.minHumidity->value();
    double maxHumidity = m_ui.maxHumidity->value();
    double minTemperature = m_ui.minTemperature->value();
    double maxTemperature = m_ui.maxTemperature->value();
    std::set<DB::SensorId> selectedSensorIds = m_selectedSensorIds;

    m_ui.showTemperature->blockSignals(true);
    m_ui.showHumidity->blockSignals(true);
    m_ui.showBattery->blockSignals(true);
    m_ui.showSignal->blockSignals(true);
    m_ui.fitMeasurements->blockSignals(true);
    m_ui.useSmoothing->blockSignals(true);
    m_ui.minHumidity->blockSignals(true);
    m_ui.maxHumidity->blockSignals(true);
    m_ui.minTemperature->blockSignals(true);
    m_ui.maxTemperature->blockSignals(true);

    QSettings settings;
    m_ui.showTemperature->setChecked(settings.value("filter/showTemperature", true).toBool());
    m_ui.showHumidity->setChecked(settings.value("filter/showHumidity", true).toBool());
    m_ui.showBattery->setChecked(settings.value("filter/showBattery", false).toBool());
    m_ui.showSignal->setChecked(settings.value("filter/showSignal", false).toBool());
    m_ui.fitMeasurements->setChecked(settings.value("filter/fitMeasurements", true).toBool());
    m_ui.useSmoothing->setChecked(settings.value("rendering/useSmoothing", true).toBool());
    m_ui.minHumidity->setValue(settings.value("rendering/minHumidity", 0.0).toDouble());
    m_ui.maxHumidity->setValue(settings.value("rendering/maxHumidity", 100.0).toDouble());
    m_ui.minTemperature->setValue(settings.value("rendering/minTemperature", 0.0).toDouble());
    m_ui.maxTemperature->setValue(settings.value("rendering/maxTemperature", 100.0).toDouble());

    m_ui.showTemperature->blockSignals(false);
    m_ui.showHumidity->blockSignals(false);
    m_ui.showBattery->blockSignals(false);
    m_ui.showSignal->blockSignals(false);
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
            showBattery != m_ui.showBattery->isChecked() ||
            showSignal != m_ui.showSignal->isChecked() ||
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
    settings.setValue("filter/showBattery", m_ui.showBattery->isChecked());
    settings.setValue("filter/showSignal", m_ui.showSignal->isChecked());
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

void PlotWidget::sensorAdded(DB::SensorId id)
{
	m_selectedSensorIds.insert(id);
}

//////////////////////////////////////////////////////////////////////////

void PlotWidget::sensorRemoved(DB::SensorId id)
{
	m_selectedSensorIds.erase(id);
}

//////////////////////////////////////////////////////////////////////////

void PlotWidget::scheduleFastRefresh()
{
    scheduleRefresh(std::chrono::milliseconds(100));
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
    m_draggedAnnotationIndex = -1;
    m_annotation.toolTip.reset();
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
            m_axisDate = m_plot->xAxis;
            m_axisDate->setTickLabelFont(font);
            m_axisDate->setLabel("Date");
            QSharedPointer<QCPAxisTickerDateTime> dateTicker(new QCPAxisTickerDateTime);
            dateTicker->setDateTimeFormat("dd-MMM-yy h:mm");
            dateTicker->setTickStepStrategy(QCPAxisTicker::TickStepStrategy::tssReadability);
            dateTicker->setTickCount(10);
            m_axisDate->setTicker(dateTicker);
            m_axisDate->setTicks(true);
            m_axisDate->setVisible(true);
        }
        {
            m_plot->yAxis->setTickLabelFont(font);
            m_plot->yAxis->setLabel("Temperature (°C)");
            m_plot->yAxis->setNumberFormat("gb");
            m_plot->yAxis->ticker()->setTickStepStrategy(QCPAxisTicker::TickStepStrategy::tssReadability);
            m_plot->yAxis->ticker()->setTickCount(20);
            m_plot->yAxis->setTicks(true);
			m_plotAxis[(size_t)PlotType::Temperature] = m_plot->yAxis;
        }
        {
            m_plot->yAxis2->setTickLabelFont(font);
            m_plot->yAxis2->setLabel("Humidity (%RH)");
            m_plot->yAxis2->setNumberFormat("gb");
            m_plot->yAxis2->ticker()->setTickStepStrategy(QCPAxisTicker::TickStepStrategy::tssReadability);
            m_plot->yAxis2->ticker()->setTickCount(20);
            m_plot->yAxis2->setTicks(true);
            m_plotAxis[(size_t)PlotType::Humidity] = m_plot->yAxis2;
        }

        {
            QCPAxis* axis = m_plot->axisRect()->addAxis(QCPAxis::atLeft);
            axis->setTickLabelFont(font);
			axis->setLabel("Battery (%)");
			axis->setNumberFormat("gb");
			axis->ticker()->setTickStepStrategy(QCPAxisTicker::TickStepStrategy::tssReadability);
			axis->ticker()->setTickCount(20);
			axis->setTicks(true);
            m_plotAxis[(size_t)PlotType::Battery] = axis;
        }
		{
            QCPAxis* axis = m_plot->axisRect()->addAxis(QCPAxis::atRight);
			axis->setTickLabelFont(font);
			axis->setLabel("Signal (%)");
			axis->setNumberFormat("gb");
			axis->ticker()->setTickStepStrategy(QCPAxisTicker::TickStepStrategy::tssReadability);
			axis->ticker()->setTickCount(20);
			axis->setTicks(true);
            m_plotAxis[(size_t)PlotType::Signal] = axis;
		}

        {
            m_plot->legend->setVisible(true);
            QFont legendFont;  // start out with MainWindow's font..
            legendFont.setPointSize(7); // and make a bit smaller for legend
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

    m_plotAxis[(size_t)PlotType::Temperature]->setVisible(m_ui.showTemperature->isChecked());
    m_plotAxis[(size_t)PlotType::Humidity]->setVisible(m_ui.showHumidity->isChecked());
	m_plotAxis[(size_t)PlotType::Battery]->setVisible(m_ui.showBattery->isChecked());
	m_plotAxis[(size_t)PlotType::Signal]->setVisible(m_ui.showSignal->isChecked());
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

    std::array<std::pair<qreal, qreal>, (size_t)PlotType::Count> plotMinMax;
    for (size_t plotIndex = 0; plotIndex < plotMinMax.size(); plotIndex++)
    {
		plotMinMax[plotIndex].first = std::numeric_limits<qreal>::max();
		plotMinMax[plotIndex].second = std::numeric_limits<qreal>::lowest();
    }

    std::vector<DB::Measurement> measurements = m_db->getFilteredMeasurements(m_filter);

    m_ui.resultCount->setText(QString("%1 out of %2 results.").arg(measurements.size()).arg(m_db->getAllMeasurementCount()));
    m_ui.exportData->setEnabled(!measurements.empty());

    std::sort(measurements.begin(), measurements.end(), [](DB::Measurement const& a, DB::Measurement const& b) { return a.timePoint < b.timePoint; });

	float measurementFrequency = 1.f;
	if (!measurements.empty())
	{
		DB::SensorTimeConfig const& stc = m_db->findSensorTimeConfigForMeasurementIndex(measurements.front().descriptor.index);
        measurementFrequency = 1.f / std::chrono::duration<float>(stc.descriptor.measurementPeriod).count();
	}
    float maxCutoffFrequency = measurementFrequency / 3.f;
    std::array<float, (size_t)PlotType::Count> plotCutoffFrequencies = 
    { 
        std::min(maxCutoffFrequency, 1.f / (60.f * 10.f)), 
        std::min(maxCutoffFrequency, 1.f / (60.f * 10.f)), 
        std::min(maxCutoffFrequency, 1.f / (60.f * 60.f)), 
        std::min(maxCutoffFrequency, 1.f / (60.f * 2.f)) 
    };

    for (size_t i = 0; i < m_db->getSensorCount(); i++)
    {
        DB::Sensor const& sensor = m_db->getSensor(i);
        if (m_selectedSensorIds.find(sensor.id) != m_selectedSensorIds.end())
        {
            GraphData& graphData = m_graphs[sensor.id];
            graphData.sensor = sensor;
			for (size_t plotIndex = 0; plotIndex < graphData.plots.size(); plotIndex++)
			{
				Plot& plot = graphData.plots[plotIndex];
				plot.lpf.setup(1, measurementFrequency, plotCutoffFrequencies[plotIndex]);
				plot.keys.reserve(8192);
				plot.values.reserve(8192);
			}
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

        for (size_t plotIndex = 0; plotIndex < graphData.plots.size(); plotIndex++)
        {
            Plot& plot = graphData.plots[plotIndex];
            qreal value = 0;
            switch ((PlotType)plotIndex)
            {
            case PlotType::Temperature: value = m.descriptor.temperature; break;
            case PlotType::Humidity: value = m.descriptor.humidity; break;
            case PlotType::Battery: value = utils::getBatteryLevel(m.descriptor.vcc) * 100.0; break;
            case PlotType::Signal: value = utils::getSignalLevel(std::min(m.descriptor.signalStrength.b2s, m.descriptor.signalStrength.s2b)) * 100.0; break;
            }
            if (m_useSmoothing)
            {
                plot.lpf.process(value);
            }
            plot.keys.push_back(time);
            plot.values.push_back(gap ? qQNaN() : value);
            auto& minMax = plotMinMax[plotIndex];
            minMax.first = std::min(minMax.first, value);
            minMax.second = std::max(minMax.second, value);
        }
    }

    m_axisDate->setRange(m_ui.dateTimeFilter->getFromDateTime().toTime_t(), m_ui.dateTimeFilter->getToDateTime().toTime_t());

    if (m_fitMeasurements)
    {
		for (size_t plotIndex = 0; plotIndex < plotMinMax.size(); plotIndex++)
		{
            auto& minMax = plotMinMax[plotIndex];
			if (std::abs(minMax.second - minMax.first) < k_plotMinRange[plotIndex])
			{
				qreal center = (minMax.first + minMax.second) / 2.0;
                minMax.first = center - k_plotMinRange[plotIndex] / 2.0;
                minMax.second = center + k_plotMinRange[plotIndex] / 2.0;
			}
		}
    }
    else
    {
		{
			auto& minMax = plotMinMax[(size_t)PlotType::Temperature];
			minMax.first = std::min(m_ui.minTemperature->value(), m_ui.maxTemperature->value());
			minMax.second = std::max(m_ui.minTemperature->value(), m_ui.maxTemperature->value());
		}
		{
			auto& minMax = plotMinMax[(size_t)PlotType::Humidity];
			minMax.first = std::min(m_ui.minHumidity->value(), m_ui.maxHumidity->value());
			minMax.second = std::max(m_ui.minHumidity->value(), m_ui.maxHumidity->value());
		}
		{
			auto& minMax = plotMinMax[(size_t)PlotType::Battery];
			minMax.first = 0.0;
			minMax.second = 100.0;
		}
		{
			auto& minMax = plotMinMax[(size_t)PlotType::Signal];
			minMax.first = 0.0;
			minMax.second = 100.0;
		}
    }

    for (size_t plotIndex = 0; plotIndex < plotMinMax.size(); plotIndex++)
    {
        QCPAxis* axis = m_plotAxis[plotIndex];
        if (axis->visible())
		{
			auto& minMax = plotMinMax[plotIndex];
			axis->setRange(minMax.first, minMax.second);
		}
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

        for (size_t plotIndex = 0; plotIndex < graphData.plots.size(); plotIndex++)
        {
            Plot& plot = graphData.plots[plotIndex];
            QCPAxis* axis = m_plotAxis[plotIndex];

            if (axis->visible())
            {
                QCPGraph* graph = m_plot->addGraph(m_axisDate, axis);
                graph->setLayer(m_graphsLayer);
                QPen pen = graph->pen();
                pen.setColor(color);
				pen.setStyle(k_plotPenStyles[plotIndex]);
                graph->setPen(pen);
                graph->setName(QString("%1 %2 (%3)").arg(graphData.sensor.descriptor.name.c_str()).arg(k_plotNames[plotIndex]).arg(k_plotUnits[plotIndex]));
                graph->setData(plot.keys, plot.values);
                plot.graph = graph;
            }
        }
    }

	{
		//clear invalid annotations
		std::vector<Annotation> annotations = std::move(m_annotations);
		for (Annotation& annotation : annotations)
		{
			QCPGraph* graph = findAnnotationGraph(annotation);
			if (graph)
			{
				annotation.toolTip->refresh(graph);
				if (annotation.toolTip->isOpen())
				{
					m_annotations.push_back(std::move(annotation));
				}
			}
		}
		m_draggedAnnotationIndex = -1;
		m_annotation.toolTip.reset();
		m_ui.clearAnnotations->setEnabled(!m_annotations.empty());
		m_annotationsLayer->replot();
	}

    m_graphsLayer->replot();
    m_plot->replot();
}

//////////////////////////////////////////////////////////////////////////

void PlotWidget::mousePressEvent(QMouseEvent* event)
{
    QCPAbstractItem* item = qobject_cast<QCPAbstractItem*>(m_plot->itemAt(event->pos()));
    int32_t index = 0;
    for (const Annotation& annotation: m_annotations)
    {
        if (annotation.toolTip->mousePressed(item, event->localPos()))
        {
            m_draggedAnnotationIndex = index;
            break;
        }
        index++;
    }
}

//////////////////////////////////////////////////////////////////////////

void PlotWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_draggedAnnotationIndex >= 0)
    {
        if ((size_t)m_draggedAnnotationIndex >= m_annotations.size())
        {
            Q_ASSERT(false);
        }
        else
		{
            const Annotation& annotation = m_annotations[m_draggedAnnotationIndex];
            QCPGraph* graph = findAnnotationGraph(annotation);
            if (graph)
			{
				annotation.toolTip->mouseReleased(graph, event->localPos());
			}
		}
		m_draggedAnnotationIndex = -1;
    }
    else
    {
        keepAnnotation();
    }
}

//////////////////////////////////////////////////////////////////////////

void PlotWidget::mouseMoveEvent(QMouseEvent* event)
{
	if (m_draggedAnnotationIndex >= 0)
	{
		if ((size_t)m_draggedAnnotationIndex >= m_annotations.size())
		{
			Q_ASSERT(false);
			m_draggedAnnotationIndex = -1;
		}
		else
		{
			const Annotation& annotation = m_annotations[m_draggedAnnotationIndex];
			QCPGraph* graph = findAnnotationGraph(annotation);
			if (graph)
			{
				annotation.toolTip->mouseMoved(graph, event->localPos());
			}
		}
	}
    else
    {
        bool overlaps = false;
        QCPAbstractItem* item = qobject_cast<QCPAbstractItem*>(m_plot->itemAt(event->pos()));
        for (const Annotation& annotation: m_annotations)
        {
			QCPGraph* graph = findAnnotationGraph(annotation);
            if (graph && annotation.toolTip->mouseMoved(graph, event->localPos()))
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
            m_annotation = Annotation();
        }
    }
    m_annotationsLayer->replot();
}

//////////////////////////////////////////////////////////////////////////

void PlotWidget::resizeEvent(QResizeEvent *event)
{
	for (const Annotation& annotation : m_annotations)
	{
		QCPGraph* graph = findAnnotationGraph(annotation);
		if (graph)
		{
            annotation.toolTip->refresh(graph);
		}
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

    const GraphData* bestGraphData = nullptr;
    PlotType plotType = PlotType::Temperature;
    for (const auto& pair: m_graphs)
    {
        const GraphData& graphData = pair.second;
        for (size_t plotIndex = 0; plotIndex < graphData.plots.size(); plotIndex++)
        {
            const Plot& plot = graphData.plots[plotIndex];
            QCPGraph* graph = plot.graph;
            if (graph && computeClosestPoint(graph, pos))
            {
                bestGraphData = &graphData;
                plotType = (PlotType)plotIndex;
            }
        }
    }

    if (bestGraphData)
    {
        createAnnotation(bestGraphData->sensor.id, QPointF(), key, value, bestGraphData->sensor, plotType);
    }
    else
    {
        m_annotation = Annotation();
    }
}

//////////////////////////////////////////////////////////////////////////

QCPGraph* PlotWidget::findAnnotationGraph(const Annotation& annotation) const
{
    auto it = m_graphs.find(annotation.sensorId);
    if (it == m_graphs.end())
    {
        return nullptr;
    }
    const GraphData& gd = it->second;
    return gd.plots[(size_t)annotation.plotType].graph;
}

//////////////////////////////////////////////////////////////////////////

void PlotWidget::keepAnnotation()
{
    if (m_annotation.toolTip != nullptr)
    {
        m_annotation.toolTip->setFixed(true);

        PlotToolTip* toolTip = m_annotation.toolTip.get();
        connect(toolTip, &PlotToolTip::closeMe, [this, toolTip]()
        {
            auto it = std::find_if(m_annotations.begin(), m_annotations.end(), [toolTip](Annotation const& a) { return a.toolTip.get() == toolTip; });
            if (it != m_annotations.end())
            {
                m_annotations.erase(it);
                m_annotationsLayer->replot();
            }
        });

        m_annotations.push_back(std::move(m_annotation));
        m_ui.clearAnnotations->setEnabled(true);

        m_annotation = Annotation();
    }
    m_annotationsLayer->replot();
}

//////////////////////////////////////////////////////////////////////////

void PlotWidget::createAnnotation(DB::SensorId sensorId, QPointF point, double key, double value, DB::Sensor const& sensor, PlotType plotType)
{
    if (m_annotation.toolTip == nullptr)
    {
        m_annotation.toolTip.reset(new PlotToolTip(m_plot, m_annotationsLayer));
    }
    m_annotation.sensorId = sensorId;
    m_annotation.plotType = plotType;

	QCPGraph* graph = findAnnotationGraph(m_annotation);
    if (!graph)
    {
        return;
    }

    QColor color = graph->pen().color();
    QDateTime dt = QDateTime::fromSecsSinceEpoch(static_cast<int64_t>(key));
	QString dateTimeFormatStr = utils::getQDateTimeFormatString(m_db->getGeneralSettings().dateTimeFormat);
    m_annotation.toolTip->setText(graph, QString("<p style=\"color:%1;\"><b>%2</b></p>%3<br>%4: <b>%5%6</b>")
                                    .arg(color.name())
                                    .arg(sensor.descriptor.name.c_str())
                                    .arg(dt.toString(dateTimeFormatStr))
                                    .arg(k_plotNames[(size_t)plotType])
                                    .arg(value, 0, 'f', 1)
                                    .arg(k_plotUnits[(size_t)plotType]));
	m_annotation.toolTip->setAnchor(graph, point, key, value);
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
    if (m_annotation.toolTip)
    {
        m_annotation.toolTip->setVisible(false);
    }

    for (const Annotation& annotation: m_annotations)
    {
        annotation.toolTip->setVisible(showAnnotations);
    }

    if (!showLegend)
	{
        m_plot->legend->setVisible(false);
		m_plot->plotLayout()->take(m_legendLayout);
	}

    m_plot->savePng(fileName);

    if (!showLegend)
	{
		m_plot->plotLayout()->addElement(0, 1, m_legendLayout);
        m_plot->legend->setVisible(true);
	}

    if (m_annotation.toolTip)
    {
        m_annotation.toolTip->setVisible(true);
    }
    for (const Annotation& annotation: m_annotations)
    {
        annotation.toolTip->setVisible(true);
    }
}

//////////////////////////////////////////////////////////////////////////

