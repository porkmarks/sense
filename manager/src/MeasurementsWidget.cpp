#include "MeasurementsWidget.h"
#include <iostream>
#include <QDateTime>
#include <QFileDialog>
#include <QCalendarWidget>
#include <QMessageBox>
#include <QSettings>

#include "ExportDataDialog.h"
#include "SensorsModel.h"
#include "SensorsDelegate.h"
#include <QSortFilterProxyModel>

#include "ui_SensorsFilterDialog.h"

#include "Logger.h"
#include "PermissionsCheck.h"
#include "MeasurementDetailsDialog.h"

extern Logger s_logger;

//////////////////////////////////////////////////////////////////////////

MeasurementsWidget::MeasurementsWidget(QWidget *parent)
    : QWidget(parent)
{
    m_ui.setupUi(this);
    setEnabled(false);

    connect(m_ui.list, &QTreeView::activated, this, &MeasurementsWidget::configureMeasurement);
}

//////////////////////////////////////////////////////////////////////////

MeasurementsWidget::~MeasurementsWidget()
{
    shutdown();
}

//////////////////////////////////////////////////////////////////////////

void MeasurementsWidget::init(Settings& settings)
{
    for (const QMetaObject::Connection& connection: m_uiConnections)
    {
        QObject::disconnect(connection);
    }
    m_uiConnections.clear();

    setEnabled(true);
	m_settings = &settings;
	m_db = &m_settings->getDB();

    m_model.reset(new MeasurementsModel(*m_db));
    m_sortingModel.setSourceModel(m_model.get());
    m_sortingModel.setSortRole(MeasurementsModel::SortingRole);

    connect(m_model.get(), &MeasurementsModel::rowsAboutToBeInserted, this, &MeasurementsWidget::refreshCounter);
    connect(m_model.get(), &MeasurementsModel::modelReset, this, &MeasurementsWidget::refreshCounter);

    m_delegate.reset(new MeasurementsDelegate(m_sortingModel));

    m_ui.list->setModel(&m_sortingModel);
    m_ui.list->setItemDelegate(m_delegate.get());

    m_ui.list->setUniformRowHeights(true);

//    DB::SensorDescriptor sd;
//    sd.name = "test1";
//    m_db->addSensor(sd);

//    DB::Clock::time_point start = DB::Clock::now();
//    for (size_t index = 0; index < 500; index++)
//    {
//        DB::Measurement m;
//        m.sensorId = m_db->getSensor(m_db->findSensorIndexByName(sd.name)).id;
//        m.index = index;
//        m.timePoint = DB::Clock::now() - std::chrono::seconds(index * 1000);
//        m.vcc = std::min((index / 1000.f) + 2.f, 3.3f);
//        m.temperature = std::min((index / 1000.f) * 55.f, 100.f);
//        m.humidity = std::min((index / 1000.f) * 100.f, 100.f);
//        m_db->addMeasurement(m);
//    }
//    std::cout << "Time to add: " << (std::chrono::duration<float>(DB::Clock::now() - start).count()) << "\n";

//    DB::Filter filter;
//    start = DB::Clock::now();
//    for (size_t i = 0; i < 1; i++)
//    {
//        std::vector<DB::Measurement> result = m_db->getFilteredMeasurements(filter);
//    }
//    std::cout << "Time to filter: " << (std::chrono::duration<float>(DB::Clock::now() - start).count()) << "\n";
//
//    refresh();

    m_uiConnections.push_back(connect(m_ui.dateTimeFilter, &DateTimeFilterWidget::filterChanged, this, &MeasurementsWidget::scheduleFastRefresh, Qt::QueuedConnection));
    m_uiConnections.push_back(connect(m_ui.selectSensors, &QPushButton::released, this, &MeasurementsWidget::selectSensors, Qt::QueuedConnection));
    m_uiConnections.push_back(connect(m_ui.exportData, &QPushButton::released, this, &MeasurementsWidget::exportData));

    m_uiConnections.push_back(connect(m_ui.minTemperature, (&QDoubleSpinBox::editingFinished), this, &MeasurementsWidget::minTemperatureChanged));
    m_uiConnections.push_back(connect(m_ui.maxTemperature, (&QDoubleSpinBox::editingFinished), this, &MeasurementsWidget::maxTemperatureChanged));

    m_uiConnections.push_back(connect(m_ui.minHumidity, (&QDoubleSpinBox::editingFinished), this, &MeasurementsWidget::minHumidityChanged));
    m_uiConnections.push_back(connect(m_ui.maxHumidity, (&QDoubleSpinBox::editingFinished), this, &MeasurementsWidget::maxHumidityChanged));


    m_uiConnections.push_back(connect(m_ui.useTemperature, &QCheckBox::stateChanged, this, &MeasurementsWidget::scheduleFastRefresh, Qt::QueuedConnection));
    m_uiConnections.push_back(connect(m_ui.minTemperature, &QDoubleSpinBox::editingFinished, this, &MeasurementsWidget::scheduleFastRefresh, Qt::QueuedConnection));
    m_uiConnections.push_back(connect(m_ui.maxTemperature, &QDoubleSpinBox::editingFinished, this, &MeasurementsWidget::scheduleFastRefresh, Qt::QueuedConnection));
    m_uiConnections.push_back(connect(m_ui.useHumidity, &QCheckBox::stateChanged, this, &MeasurementsWidget::scheduleFastRefresh, Qt::QueuedConnection));
    m_uiConnections.push_back(connect(m_ui.minHumidity, &QDoubleSpinBox::editingFinished, this, &MeasurementsWidget::scheduleFastRefresh, Qt::QueuedConnection));
    m_uiConnections.push_back(connect(m_ui.maxHumidity, &QDoubleSpinBox::editingFinished, this, &MeasurementsWidget::scheduleFastRefresh, Qt::QueuedConnection));

    m_uiConnections.push_back(connect(m_ui.showTemperature, &QCheckBox::stateChanged, this, &MeasurementsWidget::scheduleFastRefresh, Qt::QueuedConnection));
    m_uiConnections.push_back(connect(m_ui.showHumidity, &QCheckBox::stateChanged, this, &MeasurementsWidget::scheduleFastRefresh, Qt::QueuedConnection));
    m_uiConnections.push_back(connect(m_ui.showBattery, &QCheckBox::stateChanged, this, &MeasurementsWidget::scheduleFastRefresh, Qt::QueuedConnection));
    m_uiConnections.push_back(connect(m_ui.showSignal, &QCheckBox::stateChanged, this, &MeasurementsWidget::scheduleFastRefresh, Qt::QueuedConnection));

//    m_uiConnections.push_back(connect(m_db, &DB::measurementsAdded, this, &MeasurementsWidget::scheduleSlowRefresh, Qt::QueuedConnection));
//    m_uiConnections.push_back(connect(m_db, &DB::measurementsChanged, this, &MeasurementsWidget::scheduleFastRefresh, Qt::QueuedConnection));
    m_uiConnections.push_back(connect(m_db, &DB::sensorAdded, this, &MeasurementsWidget::sensorAdded, Qt::QueuedConnection));
    m_uiConnections.push_back(connect(m_db, &DB::sensorRemoved, this, &MeasurementsWidget::sensorRemoved, Qt::QueuedConnection));

    loadSettings();
}

//////////////////////////////////////////////////////////////////////////

void MeasurementsWidget::shutdown()
{
    saveSettings();

    setEnabled(false);
    m_ui.list->setModel(nullptr);
    m_ui.list->setItemDelegate(nullptr);
    m_sortingModel.setSourceModel(nullptr);
    m_model.reset();
    m_delegate.reset();
    m_db = nullptr;
}

//////////////////////////////////////////////////////////////////////////

void MeasurementsWidget::loadSettings()
{
    m_ui.dateTimeFilter->loadSettings();

    bool showTemperature = m_ui.showTemperature->isChecked();
    bool showHumidity = m_ui.showHumidity->isChecked();
    bool showBattery = m_ui.showBattery->isChecked();
    bool showSignal = m_ui.showSignal->isChecked();
    bool useTemperature = m_ui.useTemperature->isChecked();
    bool useHumidity = m_ui.useHumidity->isChecked();
    double minHumidity = m_ui.minHumidity->value();
    double maxHumidity = m_ui.maxHumidity->value();
    double minTemperature = m_ui.minTemperature->value();
    double maxTemperature = m_ui.maxTemperature->value();
    std::set<DB::SensorId> selectedSensorIds = m_selectedSensorIds;

    m_ui.showTemperature->blockSignals(true);
    m_ui.showHumidity->blockSignals(true);
    m_ui.showBattery->blockSignals(true);
    m_ui.showSignal->blockSignals(true);
    m_ui.useTemperature->blockSignals(true);
    m_ui.useHumidity->blockSignals(true);
    m_ui.minHumidity->blockSignals(true);
    m_ui.maxHumidity->blockSignals(true);
    m_ui.minTemperature->blockSignals(true);
    m_ui.maxTemperature->blockSignals(true);

    QSettings settings;
    m_ui.showTemperature->setChecked(settings.value("filter/showTemperature", true).toBool());
    m_ui.showHumidity->setChecked(settings.value("filter/showHumidity", true).toBool());
    m_ui.showBattery->setChecked(settings.value("filter/showBattery", true).toBool());
    m_ui.showSignal->setChecked(settings.value("filter/showSignal", true).toBool());
    m_ui.useTemperature->setChecked(settings.value("filter/useTemperature", true).toBool());
    m_ui.useHumidity->setChecked(settings.value("filter/useHumidity", true).toBool());
    m_ui.minHumidity->setValue(settings.value("filter/minHumidity", 0.0).toDouble());
    m_ui.maxHumidity->setValue(settings.value("filter/maxHumidity", 100.0).toDouble());
    m_ui.minTemperature->setValue(settings.value("filter/minTemperature", 0.0).toDouble());
    m_ui.maxTemperature->setValue(settings.value("filter/maxTemperature", 100.0).toDouble());

    m_ui.showTemperature->blockSignals(false);
    m_ui.showHumidity->blockSignals(false);
    m_ui.showBattery->blockSignals(false);
    m_ui.showSignal->blockSignals(false);
    m_ui.useTemperature->blockSignals(false);
    m_ui.useHumidity->blockSignals(false);
    m_ui.minHumidity->blockSignals(false);
    m_ui.maxHumidity->blockSignals(false);
    m_ui.minTemperature->blockSignals(false);
    m_ui.maxTemperature->blockSignals(false);

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
		//validate the selected sensor ids
		for (auto it = m_selectedSensorIds.begin(); it != m_selectedSensorIds.end(); )
		{
			if (m_db->findSensorIndexById(*it) < 0)
			{
				it = m_selectedSensorIds.erase(it);
			}
			else
			{
				++it;
			}
		}
    }

	m_ui.list->header()->restoreState(settings.value("measurements/list/state").toByteArray());

    m_ui.minTemperature->setEnabled(m_ui.useTemperature->isChecked());
    m_ui.maxTemperature->setEnabled(m_ui.useTemperature->isChecked());
    m_ui.minHumidity->setEnabled(m_ui.useHumidity->isChecked());
    m_ui.maxHumidity->setEnabled(m_ui.useHumidity->isChecked());

    if (showTemperature != m_ui.showTemperature->isChecked() ||
            showHumidity != m_ui.showHumidity->isChecked() ||
            showBattery != m_ui.showBattery->isChecked() ||
            showSignal != m_ui.showSignal->isChecked() ||
            useTemperature != m_ui.useTemperature->isChecked() ||
            useHumidity != m_ui.useHumidity->isChecked() ||
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

void MeasurementsWidget::saveSettings()
{
    m_ui.dateTimeFilter->saveSettings();

    QSettings settings;
    settings.setValue("filter/showTemperature", m_ui.showTemperature->isChecked());
    settings.setValue("filter/showHumidity", m_ui.showHumidity->isChecked());
    settings.setValue("filter/showBattery", m_ui.showBattery->isChecked());
    settings.setValue("filter/showSignal", m_ui.showSignal->isChecked());
    settings.setValue("filter/useTemperature", m_ui.useTemperature->isChecked());
    settings.setValue("filter/useHumidity", m_ui.useHumidity->isChecked());
    settings.setValue("filter/minHumidity", m_ui.minHumidity->value());
    settings.setValue("filter/maxHumidity", m_ui.maxHumidity->value());
    settings.setValue("filter/minTemperature", m_ui.minTemperature->value());
    settings.setValue("filter/maxTemperature", m_ui.maxTemperature->value());

    QList<QVariant> ssid;
    for (DB::SensorId id: m_selectedSensorIds)
    {
        ssid.append(id);
    }
    settings.setValue("filter/selectedSensors", ssid);
	settings.setValue("measurements/list/state", m_ui.list->header()->saveState());
}

//////////////////////////////////////////////////////////////////////////

DB::Filter MeasurementsWidget::createFilter() const
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

    filter.useTemperatureFilter = m_ui.useTemperature->isChecked();
    filter.temperatureFilter.min = static_cast<float>(m_ui.minTemperature->value());
    filter.temperatureFilter.max = static_cast<float>(m_ui.maxTemperature->value());

    filter.useHumidityFilter = m_ui.useHumidity->isChecked();
    filter.humidityFilter.min = static_cast<float>(m_ui.minHumidity->value());
    filter.humidityFilter.max = static_cast<float>(m_ui.maxHumidity->value());

    return filter;
}

//////////////////////////////////////////////////////////////////////////

void MeasurementsWidget::sensorAdded(DB::SensorId id)
{
    m_selectedSensorIds.insert(id);
}

//////////////////////////////////////////////////////////////////////////

void MeasurementsWidget::sensorRemoved(DB::SensorId id)
{
    m_selectedSensorIds.erase(id);
}

//////////////////////////////////////////////////////////////////////////

void MeasurementsWidget::scheduleFastRefresh()
{
    scheduleRefresh(std::chrono::milliseconds(100));
}

//////////////////////////////////////////////////////////////////////////

void MeasurementsWidget::scheduleSlowRefresh()
{
    scheduleRefresh(std::chrono::seconds(30));
}

//////////////////////////////////////////////////////////////////////////

void MeasurementsWidget::scheduleRefresh(DB::Clock::duration dt)
{
    int duration = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(dt).count());
    if (m_scheduleTimer && m_scheduleTimer->remainingTime() < duration)
    {
        return;
    }
    m_scheduleTimer.reset(new QTimer());
    m_scheduleTimer->setSingleShot(true);
    m_scheduleTimer->setInterval(duration);
    connect(m_scheduleTimer.get(), &QTimer::timeout, this, &MeasurementsWidget::refresh);
    m_scheduleTimer->start();
}

//////////////////////////////////////////////////////////////////////////

void MeasurementsWidget::refresh()
{
    m_scheduleTimer.reset();

    DB::Clock::time_point start = DB::Clock::now();

    m_ui.list->header()->setSectionHidden((int)MeasurementsModel::Column::Temperature, !m_ui.showTemperature->isChecked());
    m_ui.list->header()->setSectionHidden((int)MeasurementsModel::Column::Humidity, !m_ui.showHumidity->isChecked());
    m_ui.list->header()->setSectionHidden((int)MeasurementsModel::Column::Battery, !m_ui.showBattery->isChecked());
    m_ui.list->header()->setSectionHidden((int)MeasurementsModel::Column::Signal, !m_ui.showSignal->isChecked());

    DB::Filter filter = createFilter();
    m_model->setFilter(filter);

    refreshCounter();

    m_ui.exportData->setEnabled(m_model->getMeasurementCount() > 0);

	std::cout << (QString("Refreshed measurements: %3ms\n")
				  .arg(std::chrono::duration_cast<std::chrono::milliseconds>(DB::Clock::now() - start).count())).toStdString();
}

//////////////////////////////////////////////////////////////////////////

void MeasurementsWidget::refreshCounter()
{
	m_ui.resultCount->setText(QString("%1 out of %2 results.").arg(m_model->getMeasurementCount()).arg(m_db->getAllMeasurementCount()));
}

//////////////////////////////////////////////////////////////////////////

void MeasurementsWidget::minTemperatureChanged()
{
    double value = m_ui.minTemperature->value();
    if (value >= m_ui.maxTemperature->value())
    {
        value = std::min(value + 1.0, m_ui.maxTemperature->maximum());
        m_ui.maxTemperature->setValue(value);
    }
}

//////////////////////////////////////////////////////////////////////////

void MeasurementsWidget::maxTemperatureChanged()
{
    double value = m_ui.maxTemperature->value();
    if (value <= m_ui.minTemperature->value())
    {
        value = std::max(value - 1.0, m_ui.minTemperature->minimum());
        m_ui.minTemperature->setValue(value);
    }
}

//////////////////////////////////////////////////////////////////////////

void MeasurementsWidget::minHumidityChanged()
{
    double value = m_ui.minHumidity->value();
    if (value >= m_ui.maxHumidity->value())
    {
        value = std::min(value + 1.0, m_ui.maxHumidity->maximum());
        m_ui.maxHumidity->setValue(value);
    }
}

//////////////////////////////////////////////////////////////////////////

void MeasurementsWidget::maxHumidityChanged()
{
    double value = m_ui.maxHumidity->value();
    if (value <= m_ui.minHumidity->value())
    {
        value = std::max(value - 1.0, m_ui.minHumidity->minimum());
        m_ui.minHumidity->setValue(value);
    }
}

//////////////////////////////////////////////////////////////////////////

void MeasurementsWidget::exportData()
{
    ExportDataDialog dialog(*m_db, *m_model, this);
    int result = dialog.exec();
    if (result != QDialog::Accepted)
    {
        return;
    }
}

//////////////////////////////////////////////////////////////////////////

void MeasurementsWidget::selectSensors()
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

void MeasurementsWidget::configureMeasurement(QModelIndex const& index)
{
	if (!hasPermissionOrCanLoginAsAdmin(*m_settings, Settings::UserDescriptor::PermissionChangeMeasurements, this))
	{
        QMessageBox::critical(this, "Error", "You don't have permission to change measurements.");
		return;
	}

	QModelIndex mi = m_sortingModel.mapToSource(index);
	Result<DB::Measurement> result = m_model->getMeasurement(mi);
	if (result != success)
	{
		QMessageBox::critical(this, "Error", "Invalid measurement selected.");
		return;
	}

    DB::Measurement measurement = result.payload();

	MeasurementDetailsDialog dialog(*m_db, this);
	dialog.setMeasurement(measurement);

	do
	{
		int result = dialog.exec();
		if (result == QDialog::Accepted)
		{
			measurement = dialog.getMeasurement();
			Result<void> result = m_db->setMeasurement(measurement.id, measurement.descriptor);
			if (result != success)
			{
				QMessageBox::critical(this, "Error", QString("Cannot change measurement '%1': %2").arg(measurement.id).arg(result.error().what().c_str()));
				continue;
			}
		}
		break;
	} while (true);
}

//////////////////////////////////////////////////////////////////////////

