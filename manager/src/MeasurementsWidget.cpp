#include "MeasurementsWidget.h"
#include <iostream>
#include <QDateTime>
#include <QFileDialog>
#include <QMessageBox>

#include "SensorsModel.h"
#include "SensorsDelegate.h"
#include <QSortFilterProxyModel>

#include "ui_SensorsFilterDialog.h"
#include "ui_ExportDataDialog.h"

#include <fstream>
#include <iostream>
#include <iomanip>
#include <cstring>

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

    refreshFromDB();

    connect(m_ui.apply, &QPushButton::released, this, &MeasurementsWidget::refreshFromDB);
    connect(m_ui.minDateTimeNow, &QPushButton::released, this, &MeasurementsWidget::setMinDateTimeNow);
    connect(m_ui.maxDateTimeNow, &QPushButton::released, this, &MeasurementsWidget::setMaxDateTimeNow);
    connect(m_ui.thisDay, &QPushButton::released, this, &MeasurementsWidget::setDateTimeThisDay);
    connect(m_ui.thisWeek, &QPushButton::released, this, &MeasurementsWidget::setDateTimeThisWeek);
    connect(m_ui.thisMonth, &QPushButton::released, this, &MeasurementsWidget::setDateTimeThisMonth);
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

    filter.useTimePointFilter = m_ui.dateTimeFilter->isChecked();
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

void MeasurementsWidget::exportData()
{
    QDialog dialog;
    Ui::ExportDataDialog ui;
    ui.setupUi(&dialog);

    int result = dialog.exec();
    if (result != QDialog::Accepted)
    {
        return;
    }

    std::string separator = ui.separator->text().toUtf8().data();
    QString extension = "Export Files (*.csv)";

    QString fileName = QFileDialog::getSaveFileName(this, "Select export file", QString(), extension);
    if (fileName.isEmpty())
    {
        return;
    }

    std::ofstream file(fileName.toUtf8().data());
    if (!file.is_open())
    {
        QMessageBox::critical(this, "Error", QString("Cannot open file '%1' for writing:\n%2").arg(fileName).arg(std::strerror(errno)));
        return;
    }

    int dateTimeFormat = ui.dateFormat->currentIndex();
    bool exportSensorName = ui.exportSensorName->isChecked();
    bool exportTimePoint = ui.exportTimePoint->isChecked();
    bool exportIndex = ui.exportIndex->isChecked();
    bool exportTemperature = ui.exportTemperature->isChecked();
    bool exportHumidity = ui.exportHumidity->isChecked();
    int unitsFormat = ui.units->currentIndex();
    int decimalPlaces = ui.decimalPlaces->value();

    //header
    if (exportSensorName)
    {
        file << "Sensor Name";
        file << separator;
    }
    if (exportTimePoint)
    {
        file << "Date/Time";
        file << separator;
    }
    if (exportIndex)
    {
        file << "Index";
        file << separator;
    }
    if (exportTemperature)
    {
        file << "Temperature";
        file << separator;
    }
    if (unitsFormat == 2) //separate column
    {
        file << "Temperature Unit";
        file << separator;
    }
    if (exportHumidity)
    {
        file << "Humidity";
        file << separator;
    }
    if (unitsFormat == 2) //separate column
    {
        file << "Humidity Unit";
        file << separator;
    }
    file << std::endl;

    //data
    size_t size = m_model->getMeasurementCount();
    for (size_t i = 0; i < size; i++)
    {
        DB::Measurement const& m = m_model->getMeasurement(i);
        if (exportSensorName)
        {
            int32_t sensorIndex = m_db->findSensorIndexById(m.descriptor.sensorId);
            if (sensorIndex < 0)
            {
                file << "N/A";
            }
            else
            {
                file << m_db->getSensor(sensorIndex).descriptor.name;
            }
            file << separator;
        }
        if (exportTimePoint)
        {
            char buf[128];
            time_t t = DB::Clock::to_time_t(m.descriptor.timePoint);
            if (dateTimeFormat == 0)
            {
                strftime(buf, 128, "%Y/%m/%d %H:%M:%S", localtime(&t));
            }
            else if (dateTimeFormat == 1)
            {
                strftime(buf, 128, "%Y-%m-%d %H:%M:%S", localtime(&t));
            }
            else if (dateTimeFormat == 2)
            {
                strftime(buf, 128, "%d-%m-%Y %H:%M:%S", localtime(&t));
            }
            else
            {
                strftime(buf, 128, "%m/%d/%y %H:%M:%S", localtime(&t));
            }

            file << buf;
            file << separator;
        }
        if (exportIndex)
        {
            file << m.descriptor.index;
            file << separator;
        }
        if (exportTemperature)
        {
            file << std::fixed << std::setprecision(decimalPlaces) << m.descriptor.temperature;
            if (unitsFormat == 1) //embedded
            {
                file << u8" °C";
            }
            file << separator;
        }
        if (unitsFormat == 2) //separate column
        {
            file << u8"°C";
            file << separator;
        }
        if (exportHumidity)
        {
            file << std::fixed << std::setprecision(decimalPlaces) << m.descriptor.humidity;
            if (unitsFormat == 1) //embedded
            {
                file << " % RH";
            }
            file << separator;
        }
        if (unitsFormat == 2) //separate column
        {
            file << "% RH";
            file << separator;
        }
        file << std::endl;
    }

    file.close();

    QMessageBox::information(this, "Success", QString("Data was exported to file '%1'").arg(fileName));
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

    ui.list->setModel(&model);
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

