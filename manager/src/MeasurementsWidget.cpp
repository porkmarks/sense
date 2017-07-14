#include "MeasurementsWidget.h"
#include <iostream>
#include <QDateTime>
#include <QFileDialog>
#include <QMessageBox>

#include "ExportDataDialog.h"
#include "SensorsModel.h"
#include "SensorsDelegate.h"
#include <QSortFilterProxyModel>

#include "ui_SensorsFilterDialog.h"

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
    setEnabled(true);
    m_db = &db;

    m_model.reset(new MeasurementsModel(*m_db));
    m_sortingModel.setSourceModel(m_model.get());
    m_sortingModel.setSortRole(Qt::UserRole + 5);

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

    std::cout.flush();

    setDateTimePreset(m_ui.dateTimePreset->currentIndex());

    refreshFromDB();

    connect(m_ui.apply, &QPushButton::released, this, &MeasurementsWidget::refreshFromDB);
    connect(m_ui.dateTimePreset, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &MeasurementsWidget::setDateTimePreset);
    connect(m_ui.selectSensors, &QPushButton::released, this, &MeasurementsWidget::selectSensors);
    connect(m_ui.exportData, &QPushButton::released, this, &MeasurementsWidget::exportData);

    connect(m_ui.minDateTime, &QDateTimeEdit::dateTimeChanged, this, &MeasurementsWidget::minDateTimeChanged);
    connect(m_ui.maxDateTime, &QDateTimeEdit::dateTimeChanged, this, &MeasurementsWidget::maxDateTimeChanged);

    connect(m_ui.minTemperature, (&QDoubleSpinBox::editingFinished), this, &MeasurementsWidget::minTemperatureChanged);
    connect(m_ui.maxTemperature, (&QDoubleSpinBox::editingFinished), this, &MeasurementsWidget::maxTemperatureChanged);

    connect(m_ui.minHumidity, (&QDoubleSpinBox::editingFinished), this, &MeasurementsWidget::minHumidityChanged);
    connect(m_ui.maxHumidity, (&QDoubleSpinBox::editingFinished), this, &MeasurementsWidget::maxHumidityChanged);
//    connect(m_ui.minTemperature, static_cast<void(QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this, &MeasurementsWidget::minTemperatureChanged);
//    connect(m_ui.maxTemperature, static_cast<void(QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this, &MeasurementsWidget::maxTemperatureChanged);

//    connect(m_ui.minHumidity, static_cast<void(QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this, &MeasurementsWidget::minHumidityChanged);
//    connect(m_ui.maxHumidity, static_cast<void(QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this, &MeasurementsWidget::maxHumidityChanged);
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
    filter.timePointFilter.min = DB::Clock::from_time_t(m_ui.minDateTime->dateTime().toTime_t());
    filter.timePointFilter.max = DB::Clock::from_time_t(m_ui.maxDateTime->dateTime().toTime_t());

    filter.useTemperatureFilter = m_ui.useTemperature->isChecked();
    filter.temperatureFilter.min = static_cast<float>(m_ui.minTemperature->value());
    filter.temperatureFilter.max = static_cast<float>(m_ui.maxTemperature->value());

    filter.useHumidityFilter = m_ui.useHumidity->isChecked();
    filter.humidityFilter.min = static_cast<float>(m_ui.minHumidity->value());
    filter.humidityFilter.max = static_cast<float>(m_ui.maxHumidity->value());

    return filter;
}

//////////////////////////////////////////////////////////////////////////

void MeasurementsWidget::refreshFromDB()
{
    //in case the date changed, reapply it
    if (m_ui.useDateTimePreset)
    {
        setDateTimePreset(m_ui.dateTimePreset->currentIndex());
    }

    DB::Filter filter = createFilter();
    m_model->setFilter(filter);

    m_ui.resultCount->setText(QString("%1 out of %2 results.").arg(m_model->getMeasurementCount()).arg(m_db->getAllMeasurementCount()));

    for (int i = 0; i < m_model->columnCount(QModelIndex()); i++)
    {
        m_ui.list->resizeColumnToContents(i);
    }

    m_ui.exportData->setEnabled(m_model->getMeasurementCount() > 0);
}

//////////////////////////////////////////////////////////////////////////

void MeasurementsWidget::setMinDateTimeNow()
{
    QDateTime dt = QDateTime::currentDateTime();
    m_ui.minDateTime->setDateTime(dt);
}

//////////////////////////////////////////////////////////////////////////

void MeasurementsWidget::setMaxDateTimeNow()
{
    QDateTime dt = QDateTime::currentDateTime();
    m_ui.maxDateTime->setDateTime(dt);
}

//////////////////////////////////////////////////////////////////////////

void MeasurementsWidget::setDateTimePreset(int preset)
{
    switch (preset)
    {
    case 0: setDateTimePresetToday(); break;
    case 1: setDateTimePresetYesterday(); break;
    case 2: setDateTimePresetThisWeek(); break;
    case 3: setDateTimePresetLastWeek(); break;
    case 4: setDateTimePresetThisMonth(); break;
    case 5: setDateTimePresetLastMonth(); break;
    }
}

//////////////////////////////////////////////////////////////////////////

void MeasurementsWidget::setDateTimePresetToday()
{
    QDateTime dt = QDateTime::currentDateTime();

    dt.setTime(QTime(23, 59, 59, 999));
    m_ui.maxDateTime->setDateTime(dt);

    dt.setTime(QTime(0, 0));
    m_ui.minDateTime->setDateTime(dt);
}

//////////////////////////////////////////////////////////////////////////

void MeasurementsWidget::setDateTimePresetYesterday()
{
    QDateTime dt = QDateTime::currentDateTime();

    dt.setDate(dt.date().addDays(-1));

    dt.setTime(QTime(23, 59, 59, 999));
    m_ui.maxDateTime->setDateTime(dt);

    dt.setTime(QTime(0, 0));
    m_ui.minDateTime->setDateTime(dt);
}

//////////////////////////////////////////////////////////////////////////

void MeasurementsWidget::setDateTimePresetThisWeek()
{
    QDateTime dt = QDateTime::currentDateTime();

    dt.setDate(dt.date().addDays(-dt.date().dayOfWeek()));
    dt.setTime(QTime(0, 0));
    m_ui.minDateTime->setDateTime(dt);

    dt.setDate(dt.date().addDays(6));
    dt.setTime(QTime(23, 59, 59, 999));
    m_ui.maxDateTime->setDateTime(dt);
}

//////////////////////////////////////////////////////////////////////////

void MeasurementsWidget::setDateTimePresetLastWeek()
{
    QDateTime dt = QDateTime::currentDateTime();

    dt.setDate(dt.date().addDays(-dt.date().dayOfWeek()).addDays(-7));
    dt.setTime(QTime(0, 0));
    m_ui.minDateTime->setDateTime(dt);

    dt.setDate(dt.date().addDays(6));
    dt.setTime(QTime(23, 59, 59, 999));
    m_ui.maxDateTime->setDateTime(dt);
}

//////////////////////////////////////////////////////////////////////////

void MeasurementsWidget::setDateTimePresetThisMonth()
{
    QDate date = QDate::currentDate();
    date = QDate(date.year(), date.month(), 1);

    QDateTime dt(date, QTime(0, 0));
    m_ui.minDateTime->setDateTime(dt);

    dt = QDateTime(date.addMonths(1).addDays(-1), QTime(23, 59, 59, 999));
    m_ui.maxDateTime->setDateTime(dt);
}

//////////////////////////////////////////////////////////////////////////

void MeasurementsWidget::setDateTimePresetLastMonth()
{
    QDate date = QDate::currentDate();
    date = QDate(date.year(), date.month(), 1).addMonths(-1);

    QDateTime dt(date, QTime(0, 0));
    m_ui.minDateTime->setDateTime(dt);

    dt = QDateTime(date.addMonths(1).addDays(-1), QTime(23, 59, 59, 999));
    m_ui.maxDateTime->setDateTime(dt);
}

//////////////////////////////////////////////////////////////////////////

void MeasurementsWidget::minDateTimeChanged(QDateTime const& value)
{
    if (value >= m_ui.maxDateTime->dateTime())
    {
        QDateTime dt = value;
        dt.setTime(dt.time().addSecs(3600));
        m_ui.maxDateTime->setDateTime(dt);
    }
}

//////////////////////////////////////////////////////////////////////////

void MeasurementsWidget::maxDateTimeChanged(QDateTime const& value)
{
    if (value <= m_ui.minDateTime->dateTime())
    {
        QDateTime dt = value;
        dt.setTime(dt.time().addSecs(-3600));
        m_ui.minDateTime->setDateTime(dt);
    }
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
}

//////////////////////////////////////////////////////////////////////////

