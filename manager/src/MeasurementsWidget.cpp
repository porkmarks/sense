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

extern Logger s_logger;

//////////////////////////////////////////////////////////////////////////

MeasurementsWidget::MeasurementsWidget(QWidget *parent)
    : QWidget(parent)
{
    m_ui.setupUi(this);
    setEnabled(false);
}

//////////////////////////////////////////////////////////////////////////

MeasurementsWidget::~MeasurementsWidget()
{
    shutdown();
}

//////////////////////////////////////////////////////////////////////////

void MeasurementsWidget::init(DB& db)
{
    for (const QMetaObject::Connection& connection: m_uiConnections)
    {
        QObject::disconnect(connection);
    }
    m_uiConnections.clear();

    setEnabled(true);
    m_db = &db;

    m_model.reset(new MeasurementsModel(*m_db));
    m_sortingModel.setSourceModel(m_model.get());
    m_sortingModel.setSortRole(MeasurementsModel::SortingRole);

    m_delegate.reset(new MeasurementsDelegate(m_sortingModel));

    m_ui.list->setModel(&m_sortingModel);
    m_ui.list->setItemDelegate(m_delegate.get());

    m_ui.list->setUniformRowHeights(true);

    connect(m_ui.list->header(), &QHeaderView::sectionResized, [this]()
    {
        QSettings settings;
        settings.setValue("measurements/list/state", m_ui.list->header()->saveState());
    });

    {
        QSettings settings;
        m_ui.list->header()->restoreState(settings.value("measurements/list/state").toByteArray());
    }

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

    refresh();

    m_uiConnections.push_back(connect(m_ui.dateTimeFilter, &DateTimeFilterWidget::filterChanged, this, &MeasurementsWidget::refresh, Qt::QueuedConnection));
    m_uiConnections.push_back(connect(m_ui.selectSensors, &QPushButton::released, this, &MeasurementsWidget::selectSensors, Qt::QueuedConnection));
    m_uiConnections.push_back(connect(m_ui.exportData, &QPushButton::released, this, &MeasurementsWidget::exportData));

    m_uiConnections.push_back(connect(m_ui.minTemperature, (&QDoubleSpinBox::editingFinished), this, &MeasurementsWidget::minTemperatureChanged));
    m_uiConnections.push_back(connect(m_ui.maxTemperature, (&QDoubleSpinBox::editingFinished), this, &MeasurementsWidget::maxTemperatureChanged));

    m_uiConnections.push_back(connect(m_ui.minHumidity, (&QDoubleSpinBox::editingFinished), this, &MeasurementsWidget::minHumidityChanged));
    m_uiConnections.push_back(connect(m_ui.maxHumidity, (&QDoubleSpinBox::editingFinished), this, &MeasurementsWidget::maxHumidityChanged));


    m_uiConnections.push_back(connect(m_ui.useTemperature, &QCheckBox::stateChanged, this, &MeasurementsWidget::refresh, Qt::QueuedConnection));
    m_uiConnections.push_back(connect(m_ui.minTemperature, &QDoubleSpinBox::editingFinished, this, &MeasurementsWidget::refresh, Qt::QueuedConnection));
    m_uiConnections.push_back(connect(m_ui.maxTemperature, &QDoubleSpinBox::editingFinished, this, &MeasurementsWidget::refresh, Qt::QueuedConnection));
    m_uiConnections.push_back(connect(m_ui.useHumidity, &QCheckBox::stateChanged, this, &MeasurementsWidget::refresh, Qt::QueuedConnection));
    m_uiConnections.push_back(connect(m_ui.minHumidity, &QDoubleSpinBox::editingFinished, this, &MeasurementsWidget::refresh, Qt::QueuedConnection));
    m_uiConnections.push_back(connect(m_ui.maxHumidity, &QDoubleSpinBox::editingFinished, this, &MeasurementsWidget::refresh, Qt::QueuedConnection));

    m_uiConnections.push_back(connect(m_ui.showTemperature, &QCheckBox::stateChanged, this, &MeasurementsWidget::refresh, Qt::QueuedConnection));
    m_uiConnections.push_back(connect(m_ui.showHumidity, &QCheckBox::stateChanged, this, &MeasurementsWidget::refresh, Qt::QueuedConnection));
    m_uiConnections.push_back(connect(m_ui.showBattery, &QCheckBox::stateChanged, this, &MeasurementsWidget::refresh, Qt::QueuedConnection));
    m_uiConnections.push_back(connect(m_ui.showSignal, &QCheckBox::stateChanged, this, &MeasurementsWidget::refresh, Qt::QueuedConnection));
}

//////////////////////////////////////////////////////////////////////////

void MeasurementsWidget::shutdown()
{
    setEnabled(false);
    m_ui.list->setModel(nullptr);
    m_ui.list->setItemDelegate(nullptr);
    m_sortingModel.setSourceModel(nullptr);
    m_model.reset();
    m_delegate.reset();
    m_db = nullptr;
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

void MeasurementsWidget::refresh()
{
    DB::Clock::time_point start = DB::Clock::now();

    m_model->setColumnVisibility(MeasurementsModel::Column::Temperature, m_ui.showTemperature->isChecked());
    m_model->setColumnVisibility(MeasurementsModel::Column::Humidity, m_ui.showHumidity->isChecked());
    m_model->setColumnVisibility(MeasurementsModel::Column::Battery, m_ui.showBattery->isChecked());
    m_model->setColumnVisibility(MeasurementsModel::Column::Signal, m_ui.showSignal->isChecked());

    DB::Filter filter = createFilter();
    m_model->setFilter(filter);

    s_logger.logVerbose(QString("Refreshed %1/%2 measurements: %3ms")
                        .arg(m_model->getMeasurementCount())
                        .arg(m_db->getAllMeasurementCount())
                        .arg(std::chrono::duration_cast<std::chrono::milliseconds>(DB::Clock::now() - start).count()));

    m_ui.resultCount->setText(QString("%1 out of %2 results.").arg(m_model->getMeasurementCount()).arg(m_db->getAllMeasurementCount()));

    m_ui.exportData->setEnabled(m_model->getMeasurementCount() > 0);
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
    ExportDataDialog dialog(*m_db, *m_model);
    int result = dialog.exec();
    if (result != QDialog::Accepted)
    {
        return;
    }
}

//////////////////////////////////////////////////////////////////////////

void MeasurementsWidget::selectSensors()
{
    QDialog dialog;
    Ui::SensorsFilterDialog ui;
    ui.setupUi(&dialog);

    SensorsModel model(*m_db);
    model.setShowCheckboxes(true);

    QSortFilterProxyModel sortingModel;
    sortingModel.setSourceModel(&model);

    SensorsDelegate delegate(sortingModel);

    size_t sensorCount = m_db->getSensorCount();
    if (m_selectedSensorIds.empty())
    {
        for (size_t i = 0; i < sensorCount; i++)
        {
            model.setSensorChecked(m_db->getSensor(i).id, true);
        }
    }
    else
    {
        for (DB::SensorId id: m_selectedSensorIds)
        {
            model.setSensorChecked(id, true);
        }
    }

    auto updateSelectionCheckboxes = [this, &model, &ui, sensorCount]()
    {
        bool allSensorsChecked = true;
        bool anySensorsChecked = false;
        for (size_t i = 0; i < sensorCount; i++)
        {
            bool isChecked = model.isSensorChecked(m_db->getSensor(i).id);
            allSensorsChecked &= isChecked;
            anySensorsChecked |= isChecked;
        }
        ui.selectAll->blockSignals(true);
        ui.selectAll->setCheckState(allSensorsChecked ? Qt::Checked : (anySensorsChecked ? Qt::PartiallyChecked : Qt::Unchecked));
        ui.selectAll->blockSignals(false);
    };
    updateSelectionCheckboxes();

    connect(&model, &SensorsModel::sensorCheckedChanged, updateSelectionCheckboxes);
    connect(ui.selectAll, &QCheckBox::stateChanged, [sensorCount, &model, &ui, this]()
    {
        if (ui.selectAll->checkState() != Qt::PartiallyChecked)
        {
            for (size_t i = 0; i < sensorCount; i++)
            {
                model.setSensorChecked(m_db->getSensor(i).id, ui.selectAll->isChecked());
            }
        }
    });


    ui.list->setModel(&sortingModel);
    ui.list->setItemDelegate(&delegate);

    for (int i = 0; i < model.columnCount(); i++)
    {
        ui.list->header()->setSectionResizeMode(i, QHeaderView::ResizeToContents);
    }
    ui.list->header()->setStretchLastSection(true);

    int result = dialog.exec();
    if (result == QDialog::Accepted)
    {
        m_selectedSensorIds.clear();
        for (size_t i = 0; i < sensorCount; i++)
        {
            DB::Sensor const& sensor = m_db->getSensor(i);
            if (model.isSensorChecked(sensor.id))
            {
                m_selectedSensorIds.push_back(sensor.id);
            }
        }
    }

    refresh();
}

//////////////////////////////////////////////////////////////////////////

