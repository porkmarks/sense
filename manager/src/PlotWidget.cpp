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
std::array<float, (size_t)PlotWidget::PlotType::Count> k_plotPenWidths = { 2.f, 2.f, 1.f, 1.f };
std::array<Qt::PenStyle, (size_t)PlotWidget::PlotType::Count> k_plotPenStyles = { Qt::SolidLine, Qt::DashLine, Qt::DotLine, Qt::DashDotDotLine };
std::array<const char*, (size_t)PlotWidget::PlotType::Count> k_plotNames = { "Temperature", "Humidity", "Battery", "Signal" };
std::array<double, (size_t)PlotWidget::PlotType::Count> k_plotMinRange = { 5.0, 10.0, 0.1, 0.1 };

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
    setEnabled(true);
    m_db = &db;

    connect(m_db, &DB::userLoggedIn, this, &PlotWidget::setPermissions);

    loadSettings();

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

    m_ui.showTemperature->blockSignals(true);
    m_ui.showHumidity->blockSignals(true);
    m_ui.showBattery->blockSignals(true);
    m_ui.showSignal->blockSignals(true);
    m_ui.fitVertically->blockSignals(true);
    m_ui.fitHorizontally->blockSignals(true);
    m_ui.useSmoothing->blockSignals(true);
    m_ui.minHumidity->blockSignals(true);
    m_ui.maxHumidity->blockSignals(true);
    m_ui.minTemperature->blockSignals(true);
    m_ui.maxTemperature->blockSignals(true);

    QSettings settings;
    m_ui.showTemperature->setChecked(settings.value("filter/showTemperature", true).toBool());
    m_ui.showHumidity->setChecked(settings.value("filter/showHumidity", true).toBool());
    //not loading these on purpose
//     m_ui.showBattery->setChecked(settings.value("filter/showBattery", false).toBool());
//     m_ui.showSignal->setChecked(settings.value("filter/showSignal", false).toBool());
    m_ui.fitVertically->setChecked(settings.value("filter/fitVertically", true).toBool());
	m_ui.fitHorizontally->setChecked(settings.value("filter/fitHorizontally", true).toBool());
    m_ui.useSmoothing->setChecked(settings.value("rendering/useSmoothing", true).toBool());
    m_ui.minHumidity->setValue(settings.value("rendering/minHumidity", 0.0).toDouble());
    m_ui.maxHumidity->setValue(settings.value("rendering/maxHumidity", 100.0).toDouble());
    m_ui.minTemperature->setValue(settings.value("rendering/minTemperature", 0.0).toDouble());
    m_ui.maxTemperature->setValue(settings.value("rendering/maxTemperature", 100.0).toDouble());

    m_ui.showTemperature->blockSignals(false);
    m_ui.showHumidity->blockSignals(false);
    m_ui.showBattery->blockSignals(false);
    m_ui.showSignal->blockSignals(false);
    m_ui.fitVertically->blockSignals(false);
	m_ui.fitHorizontally->blockSignals(false);
    m_ui.useSmoothing->blockSignals(false);
    m_ui.minHumidity->blockSignals(false);
    m_ui.maxHumidity->blockSignals(false);
    m_ui.minTemperature->blockSignals(false);
    m_ui.maxTemperature->blockSignals(false);

    m_ui.minTemperature->setEnabled(!m_ui.fitVertically->isChecked());
    m_ui.maxTemperature->setEnabled(!m_ui.fitVertically->isChecked());
    m_ui.minHumidity->setEnabled(!m_ui.fitVertically->isChecked());
    m_ui.maxHumidity->setEnabled(!m_ui.fitVertically->isChecked());

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
            DB::Sensor sensor = m_db->getSensor(i);
            m_selectedSensorIds.insert(sensor.id);
        }
    }
}

//////////////////////////////////////////////////////////////////////////

void PlotWidget::saveSettings()
{
    m_ui.dateTimeFilter->saveSettings();

    QSettings settings;
    settings.setValue("filter/showTemperature", m_ui.showTemperature->isChecked());
    settings.setValue("filter/showHumidity", m_ui.showHumidity->isChecked());
	//not saving these on purpose
//     settings.setValue("filter/showBattery", m_ui.showBattery->isChecked());
//     settings.setValue("filter/showSignal", m_ui.showSignal->isChecked());
    settings.setValue("filter/fitVertically", m_ui.fitVertically->isChecked());
	settings.setValue("filter/fitHorizontally", m_ui.fitHorizontally->isChecked());
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

void PlotWidget::setActive(bool active)
{
	for (const QMetaObject::Connection& connection : m_uiConnections)
	{
		QObject::disconnect(connection);
	}
	m_uiConnections.clear();

	if (active)
	{
		m_uiConnections.push_back(connect(m_ui.clearAnnotations, &QPushButton::released, this, &PlotWidget::clearAnnotations));
		m_uiConnections.push_back(connect(m_ui.dateTimeFilter, &DateTimeFilterWidget::filterChanged, this, &PlotWidget::scheduleFastRefresh, Qt::QueuedConnection));
		m_uiConnections.push_back(connect(m_ui.selectSensors, &QPushButton::released, this, &PlotWidget::selectSensors, Qt::QueuedConnection));
		m_uiConnections.push_back(connect(m_ui.exportData, &QPushButton::released, this, &PlotWidget::exportData));

		m_uiConnections.push_back(connect(m_ui.fitVertically, &QCheckBox::stateChanged, this, &PlotWidget::scheduleFastRefresh, Qt::QueuedConnection));
		m_uiConnections.push_back(connect(m_ui.fitHorizontally, &QCheckBox::stateChanged, this, &PlotWidget::scheduleFastRefresh, Qt::QueuedConnection));
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
        m_uiConnections.push_back(connect(m_db, &DB::measurementsChanged, this, &PlotWidget::scheduleMediumRefresh, Qt::QueuedConnection));
        m_uiConnections.push_back(connect(m_db, &DB::sensorAdded, this, &PlotWidget::sensorAdded, Qt::QueuedConnection));
        m_uiConnections.push_back(connect(m_db, &DB::sensorRemoved, this, &PlotWidget::sensorRemoved, Qt::QueuedConnection));
		
        loadSettings();

        refresh();
	}
	else
	{
		saveSettings();
	}
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

    DB::SensorTimeConfig timeConfig = m_db->getLastSensorTimeConfig();

    //filter.useTimePointFilter = m_ui.dateTimeFilter->isChecked();
    filter.useTimePointFilter = true;
    filter.timePointFilter.min = IClock::from_time_t(m_ui.dateTimeFilter->getFromDateTime().toTime_t()) - timeConfig.descriptor.measurementPeriod;
    filter.timePointFilter.max = IClock::from_time_t(m_ui.dateTimeFilter->getToDateTime().toTime_t()) + timeConfig.descriptor.measurementPeriod;

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

void PlotWidget::scheduleMediumRefresh()
{
	scheduleRefresh(std::chrono::seconds(5));
}

//////////////////////////////////////////////////////////////////////////

void PlotWidget::scheduleSlowRefresh()
{
    scheduleRefresh(std::chrono::seconds(30));
}

//////////////////////////////////////////////////////////////////////////

void PlotWidget::scheduleRefresh(IClock::duration dt)
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
            DB::Sensor sensor = m_db->getSensor(i);
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
    if (!m_db || m_isApplyingFilter)
    {
        return;
    }
    m_isApplyingFilter = true;
    utils::epilogue epi([this] { m_isApplyingFilter = false; });

    m_filter = filter;

    createPlotWidgets();

    m_graphs.clear();
    while (m_plot->graphCount() > 0)
    {
        m_plot->removeGraph(m_plot->graph(0));
    }
    for (QCPAbstractItem* item: m_indicatorItems)
	{
        m_plot->removeItem(item);
	}
    m_indicatorItems.clear();

    uint64_t minTS = std::numeric_limits<uint64_t>::max();
    uint64_t maxTS = std::numeric_limits<uint64_t>::lowest();

    std::array<std::pair<double, double>, (size_t)PlotType::Count> plotMinMax;
    for (size_t plotIndex = 0; plotIndex < plotMinMax.size(); plotIndex++)
    {
		plotMinMax[plotIndex].first = std::numeric_limits<double>::max();
		plotMinMax[plotIndex].second = std::numeric_limits<double>::lowest();
    }

    size_t totalCount = m_db->getFilteredMeasurementCount(m_filter);

    m_ui.resultCount->setText(QString("%1 out of %2 results.").arg(totalCount).arg(m_db->getAllMeasurementCount()));
    m_ui.exportData->setEnabled(totalCount > 0);

    for (size_t i = 0; i < m_db->getSensorCount(); i++)
    {
        DB::Sensor sensor = m_db->getSensor(i);
        if (m_selectedSensorIds.find(sensor.id) != m_selectedSensorIds.end())
        {
            GraphData& graphData = m_graphs[sensor.id];
            graphData.sensor = sensor;
			for (size_t plotIndex = 0; plotIndex < graphData.plots.size(); plotIndex++)
			{
				Plot& plot = graphData.plots[plotIndex];
				plot.keys.reserve(8192);
				plot.values.reserve(8192);
                plot.alarmIndicators.clear();
			}
            graphData.lastIndex = -1;
        }
    }

    size_t chunkSize = 50000;

    QProgressDialog progressDialog(QProgressDialog("Plotting...", "Abort", 0, (int)(totalCount / chunkSize), this));
	progressDialog.setWindowModality(Qt::WindowModal);
	progressDialog.setMinimumDuration(200);

    for (size_t measurementsIndex = 0; measurementsIndex < totalCount; measurementsIndex += chunkSize)
	{
        std::vector<DB::Measurement> measurements = m_db->getFilteredMeasurements(filter, measurementsIndex, chunkSize);
		std::sort(measurements.begin(), measurements.end(), [](DB::Measurement const& a, DB::Measurement const& b) { return a.timePoint < b.timePoint; });

        progressDialog.setValue(int(measurementsIndex / chunkSize));
        if (progressDialog.wasCanceled())
        {
            break;
        }

		for (DB::Measurement const& m : measurements)
		{
			time_t time = IClock::to_time_t(m.timePoint);
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
				uint32_t alarmTriggerMask = 0;
				double value = 0;
				switch ((PlotType)plotIndex)
				{
				case PlotType::Temperature: value = m.descriptor.temperature; alarmTriggerMask = DB::AlarmTrigger::MeasurementTemperatureMask; break;
				case PlotType::Humidity: value = m.descriptor.humidity; alarmTriggerMask = DB::AlarmTrigger::MeasurementHumidityMask; break;
				case PlotType::Battery: value = utils::getBatteryLevel(m.descriptor.vcc) * 100.0; alarmTriggerMask = DB::AlarmTrigger::MeasurementLowVcc; break;
				case PlotType::Signal: value = utils::getSignalLevel(std::min(m.descriptor.signalStrength.b2s, m.descriptor.signalStrength.s2b)) * 100.0; alarmTriggerMask = DB::AlarmTrigger::MeasurementLowSignal; break;
				}
				if (m_ui.useSmoothing->isChecked())
				{
					if (!plot.oldValue.has_value() || gap)
					{
						plot.oldValue = value;
					}
					value = *plot.oldValue * 0.8 + value * 0.2;
					plot.oldValue = value;
				}
				plot.keys.push_back(time);
				plot.values.push_back(gap ? qQNaN() : value);
				if ((m.alarmTriggers.added & alarmTriggerMask) || (m.alarmTriggers.removed & alarmTriggerMask))
				{
					plot.alarmIndicators.push_back({ double(time), value, m.alarmTriggers & alarmTriggerMask });
				}
				auto& minMax = plotMinMax[plotIndex];
				minMax.first = std::min(minMax.first, value);
				minMax.second = std::max(minMax.second, value);
			}
		}
	}

	if (m_ui.fitHorizontally->isChecked())
    {
        uint64_t d = maxTS - minTS;
		if (d < 10)
		{
			uint64_t center = (maxTS + minTS) / 2;
			minTS = center - 10 / 2;
			maxTS = center + 10 / 2;
		}
        else
        {
            //make sure the fitting doesn't show more than the date time range
            minTS = std::max<uint64_t>(minTS, m_ui.dateTimeFilter->getFromDateTime().toTime_t());
            maxTS = std::min<uint64_t>(maxTS, m_ui.dateTimeFilter->getToDateTime().toTime_t());
        }
		m_axisDate->setRange(minTS, maxTS);
    }
    else
	{
		m_axisDate->setRange(m_ui.dateTimeFilter->getFromDateTime().toTime_t(), m_ui.dateTimeFilter->getToDateTime().toTime_t());
	}

    if (m_ui.fitVertically->isChecked())
    {
		for (size_t plotIndex = 0; plotIndex < plotMinMax.size(); plotIndex++)
		{
            auto& minMax = plotMinMax[plotIndex];
			if (std::abs(minMax.second - minMax.first) < k_plotMinRange[plotIndex])
			{
                double center = (minMax.first + minMax.second) / 2.0;
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
        if (brightness > 0.6) //make sure the color is not too bright
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
				pen.setWidthF(k_plotPenWidths[plotIndex]);
                graph->setPen(pen);
                graph->setName(QString("%1 %2").arg(graphData.sensor.descriptor.name.c_str()).arg(k_plotNames[plotIndex]));
                graph->setData(plot.keys, plot.values);
                plot.graph = graph;

                for (Plot::AlarmIndicator const& ai : plot.alarmIndicators)
                {
					// add the phase tracer (red circle) which sticks to the graph data (and gets updated in bracketDataSlot by timer event):
					QCPItemTracer* item = new QCPItemTracer(m_plot);
					item->setGraph(graph);
					item->setGraphKey(ai.key);
					item->setInterpolating(true);
					item->setStyle(QCPItemTracer::tsCircle);
                    QColor color = utils::getDominatingTriggerColor(ai.triggers.added ? ai.triggers.added : ai.triggers.current);
					item->setPen(QPen(color));
					item->setBrush(color);
					item->setSize(7);
                    m_indicatorItems.push_back(item);
                }
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

	double delta = m_axisDate->pixelToCoord(2) - m_axisDate->pixelToCoord(1);

    const GraphData* bestGraphData = nullptr;
    PlotType plotType = PlotType::Temperature;
    for (const auto& pair: m_graphs)
    {
        const GraphData& graphData = pair.second;
        for (size_t plotIndex = 0; plotIndex < graphData.plots.size(); plotIndex++)
        {
            const Plot& plot = graphData.plots[plotIndex];
            QCPGraph* graph = plot.graph;
            if (!graph)
            {
                continue;
            }
                
            if (computeClosestPoint(graph, pos))
            {
                bestGraphData = &graphData;
                plotType = (PlotType)plotIndex;
            }
        }
    }

    if (bestGraphData)
    {
		const Plot& plot = bestGraphData->plots[(size_t)plotType];

		//now snap to the closest alarm indicator
		double bestDistance = std::numeric_limits<double>::max();
		std::optional<Plot::AlarmIndicator> bestAlarmIndicator;
		for (Plot::AlarmIndicator const& ai : plot.alarmIndicators)
		{
			double d = std::abs(ai.key - key);
			if (d < delta && d < bestDistance)
			{
				bestAlarmIndicator = ai;
				bestDistance = d;
			}
		}
		if (bestAlarmIndicator.has_value())
		{
			key = bestAlarmIndicator->key;
			value = bestAlarmIndicator->value;
		}

        createAnnotation(bestGraphData->sensor.id, QPointF(), key, value, bestGraphData->sensor, plotType);
    }
    else
    {
        m_annotation = Annotation();
    }
}

//////////////////////////////////////////////////////////////////////////

const PlotWidget::Plot* PlotWidget::findAnnotationPlot(const Annotation& annotation) const
{
	auto it = m_graphs.find(annotation.sensorId);
	if (it == m_graphs.end())
	{
		return nullptr;
	}
	const GraphData& gd = it->second;
	return &gd.plots[(size_t)annotation.plotType];
}

//////////////////////////////////////////////////////////////////////////

QCPGraph* PlotWidget::findAnnotationGraph(const Annotation& annotation) const
{
    const Plot* plot = findAnnotationPlot(annotation);
    if (!plot)
    {
        return nullptr;
    }
    return plot->graph;
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

	const Plot* plot = findAnnotationPlot(m_annotation);
    if (!plot)
    {
        return;
    }

    QString alarmStr;
    for (Plot::AlarmIndicator const& ai : plot->alarmIndicators)
    {
        if (std::abs(ai.key - key) < 0.5)
        {
            if (ai.triggers.added)
			{
                alarmStr = QString("<p>Triggered: <span style=\"color:#%1;\"><b>%2</b></span></p>")
                    .arg(utils::getDominatingTriggerColor(ai.triggers.added) & 0xFFFFFF, 6, 16, QChar('0'))
                    .arg(utils::getDominatingTriggerName(ai.triggers.added));
			}
            else
            {
				alarmStr = QString("<p>Recovered to: <span style=\"color:#%1;\"><b>%2</b></span></p>")
                    .arg(utils::getDominatingTriggerColor(ai.triggers.current) & 0xFFFFFF, 6, 16, QChar('0'))
                    .arg(utils::getDominatingTriggerName(ai.triggers.current));
            }
            break;
        }
    }

	m_annotation.toolTip->setAnchor(plot->graph, point, key, value);

    QColor color = k_colors[sensor.id % k_colors.size()];
	double brightness = (0.2126 * color.redF() + 0.7152 * color.greenF() + 0.0722 * color.blueF());
	if (brightness > 0.6) //make sure the color is not too bright
	{
		color.setHslF(color.hueF(), color.saturationF(), 0.4);
	}

    QDateTime dt = QDateTime::fromSecsSinceEpoch(static_cast<int64_t>(key));
	QString dateTimeFormatStr = utils::getQDateTimeFormatString(m_db->getGeneralSettings().dateTimeFormat);
    m_annotation.toolTip->setText(plot->graph, QString("<span style=\"color:#%1;\"><b>%2</b></span><br>%3<br>%4: <b>%5%6</b>%7")
                                  .arg(color.rgba() & 0xFFFFFF, 6, 16, QChar('0'))
                                  .arg(sensor.descriptor.name.c_str())
                                  .arg(dt.toString(dateTimeFormatStr))
                                  .arg(k_plotNames[(size_t)plotType])
                                  .arg(value, 0, 'f', 1)
                                  .arg(k_plotUnits[(size_t)plotType])
                                  .arg(alarmStr));
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

