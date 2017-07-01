#include "MeasurementsWidget.h"
#include <iostream>
#include <QDateTime>

#include "SensorsModel.h"
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

    DB::SensorDescriptor sd;
    sd.name = "test1";
    m_db->addSensor(sd);

    m_model.reset(new MeasurementsModel(*m_db));
    m_ui.list->setModel(m_model.get());
    m_ui.list->setItemDelegate(m_model.get());

    m_ui.list->setUniformRowHeights(true);

    DB::Clock::time_point start = DB::Clock::now();
    for (size_t index = 0; index < 5000000; index++)
    {
        DB::Measurement m;
        m.sensorId = m_db->getSensor(m_db->findSensorIndexByName(sd.name)).id;
        m.index = index;
        m.timePoint = DB::Clock::now() - std::chrono::seconds(index * 1000);
        m.vcc = std::min((index / 1000.f) + 2.f, 3.3f);
        m.temperature = std::min((index / 1000.f) * 55.f, 100.f);
        m.humidity = std::min((index / 1000.f) * 100.f, 100.f);
        m_db->addMeasurement(m);
    }
    std::cout << "Time to add: " << (std::chrono::duration<float>(DB::Clock::now() - start).count()) << "\n";

    DB::Filter filter;
    start = DB::Clock::now();
    for (size_t i = 0; i < 1; i++)
    {
        std::vector<DB::Measurement> result = m_db->getFilteredMeasurements(filter);
    }
    std::cout << "Time to filter: " << (std::chrono::duration<float>(DB::Clock::now() - start).count()) << "\n";

    std::cout.flush();

    refreshFromDB();

    QObject::connect(m_ui.apply, &QPushButton::released, this, &MeasurementsWidget::refreshFromDB);
    QObject::connect(m_ui.minDateTimeNow, &QPushButton::released, this, &MeasurementsWidget::setMinDateTimeNow);
    QObject::connect(m_ui.maxDateTimeNow, &QPushButton::released, this, &MeasurementsWidget::setMaxDateTimeNow);
    QObject::connect(m_ui.thisDay, &QPushButton::released, this, &MeasurementsWidget::setDateTimeThisDay);
    QObject::connect(m_ui.thisWeek, &QPushButton::released, this, &MeasurementsWidget::setDateTimeThisWeek);
    QObject::connect(m_ui.thisMonth, &QPushButton::released, this, &MeasurementsWidget::setDateTimeThisMonth);
    QObject::connect(m_ui.selectSensors, &QPushButton::released, this, &MeasurementsWidget::selectSensors);

    QObject::connect(m_ui.minDateTime, &QDateTimeEdit::dateTimeChanged, this, &MeasurementsWidget::minDateTimeChanged);
    QObject::connect(m_ui.maxDateTime, &QDateTimeEdit::dateTimeChanged, this, &MeasurementsWidget::maxDateTimeChanged);

    QObject::connect(m_ui.minTemperature, (&QDoubleSpinBox::editingFinished), this, &MeasurementsWidget::minTemperatureChanged);
    QObject::connect(m_ui.maxTemperature, (&QDoubleSpinBox::editingFinished), this, &MeasurementsWidget::maxTemperatureChanged);

    QObject::connect(m_ui.minHumidity, (&QDoubleSpinBox::editingFinished), this, &MeasurementsWidget::minHumidityChanged);
    QObject::connect(m_ui.maxHumidity, (&QDoubleSpinBox::editingFinished), this, &MeasurementsWidget::maxHumidityChanged);
//    QObject::connect(m_ui.minTemperature, static_cast<void(QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this, &MeasurementsWidget::minTemperatureChanged);
//    QObject::connect(m_ui.maxTemperature, static_cast<void(QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this, &MeasurementsWidget::maxTemperatureChanged);

//    QObject::connect(m_ui.minHumidity, static_cast<void(QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this, &MeasurementsWidget::minHumidityChanged);
//    QObject::connect(m_ui.maxHumidity, static_cast<void(QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this, &MeasurementsWidget::maxHumidityChanged);
}

//////////////////////////////////////////////////////////////////////////

void MeasurementsWidget::shutdown()
{
    setEnabled(false);
    m_ui.list->setModel(nullptr);
    m_ui.list->setItemDelegate(nullptr);
    m_model.reset();
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
    DB::Filter filter = createFilter();
    m_model->setFilter(filter);

    m_ui.resultCount->setText(QString("%1 results.").arg(m_model->getMeasurementCount()));
}

//////////////////////////////////////////////////////////////////////////

void MeasurementsWidget::setMinDateTimeNow()
{
    DB::Clock::time_point now = DB::Clock::now();
    QDateTime dt;
    dt.setTime_t(DB::Clock::to_time_t(now));
    m_ui.minDateTime->setDateTime(dt);
}

//////////////////////////////////////////////////////////////////////////

void MeasurementsWidget::setMaxDateTimeNow()
{
    DB::Clock::time_point now = DB::Clock::now();
    QDateTime dt;
    dt.setTime_t(DB::Clock::to_time_t(now));
    m_ui.maxDateTime->setDateTime(dt);
}

//////////////////////////////////////////////////////////////////////////

void MeasurementsWidget::setDateTimeThisDay()
{
    DB::Clock::time_point now = DB::Clock::now();
    QDateTime dt;
    dt.setTime_t(DB::Clock::to_time_t(now));
    m_ui.maxDateTime->setDateTime(dt);

    dt.setTime(QTime(0, 0));
    m_ui.minDateTime->setDateTime(dt);
}

//////////////////////////////////////////////////////////////////////////

void MeasurementsWidget::setDateTimeThisWeek()
{
    DB::Clock::time_point now = DB::Clock::now();
    QDateTime dt;
    dt.setTime_t(DB::Clock::to_time_t(now));
    m_ui.maxDateTime->setDateTime(dt);

    dt.setDate(dt.date().addDays(-dt.date().dayOfWeek()));
    dt.setTime(QTime(0, 0));
    m_ui.minDateTime->setDateTime(dt);
}

//////////////////////////////////////////////////////////////////////////

void MeasurementsWidget::setDateTimeThisMonth()
{
    DB::Clock::time_point now = DB::Clock::now();
    QDateTime dt;
    dt.setTime_t(DB::Clock::to_time_t(now));
    m_ui.maxDateTime->setDateTime(dt);

    dt.setDate(dt.date().addDays(-dt.date().daysInMonth()));
    dt.setTime(QTime(0, 0));
    m_ui.minDateTime->setDateTime(dt);
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

void MeasurementsWidget::selectSensors()
{
    QDialog dialog;
    Ui::SensorsFilterDialog ui;
    ui.setupUi(&dialog);

    SensorsModel model(*m_db);
    model.setShowCheckboxes(true);

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

    ui.list->setModel(&model);
    ui.list->setItemDelegate(&model);

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

