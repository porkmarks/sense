#include "MeasurementsWidget.h"
#include <iostream>
#include <QDateTime>

MeasurementsWidget::MeasurementsWidget(QWidget *parent)
    : QWidget(parent)
{
    m_ui.setupUi(this);

    m_model.setColumnCount(8);
    m_model.setHorizontalHeaderLabels({"Sensor", "Index", "Timestamp", "Temperature", "Humidity", "Battery", "Signal", "Errors"});

    m_ui.list->setModel(&m_model);

    m_ui.list->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_ui.list->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_ui.list->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_ui.list->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_ui.list->header()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_ui.list->header()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    m_ui.list->header()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
    m_ui.list->header()->setSectionResizeMode(7, QHeaderView::Stretch);
}

void MeasurementsWidget::init(Comms& comms)
{
    m_comms = &comms;

    DB::Clock::time_point start = DB::Clock::now();
    for (size_t sid = 0; sid < 4; sid++)
    {
        for (size_t index = 0; index < 1000; index++)
        {
            DB::Measurement m;
            m.sensor_id = sid;
            m.index = index;
            m_db.add_measurement(m);
        }
    }
    std::cout << "Time to add: " << (std::chrono::duration<float>(DB::Clock::now() - start).count()) << "\n";

    DB::Filter filter;
    start = DB::Clock::now();
    for (size_t i = 0; i < 1; i++)
    {
        std::vector<DB::Measurement> result = m_db.get_filtered_measurements(filter);
    }
    std::cout << "Time to filter: " << (std::chrono::duration<float>(DB::Clock::now() - start).count()) << "\n";

    std::cout.flush();

    refreshFromDB();
}

void MeasurementsWidget::refreshFromDB()
{
    DB::Filter filter;
    std::vector<DB::Measurement> result = m_db.get_filtered_measurements(filter);

    m_model.clear();

    std::vector<Comms::Sensor> const& sensors = m_comms->getLastSensors();

    for (DB::Measurement const& m: result)
    {
        QStandardItem* sensorItem = new QStandardItem();
        sensorItem->setIcon(QIcon(":/icons/ui/sensor.png"));
        auto it = std::find_if(sensors.begin(), sensors.end(), [&m](Comms::Sensor const& sensor) { return sensor.id == m.sensor_id; });
        if (it == sensors.end())
        {
            sensorItem->setText("N/A");
        }
        else
        {
            sensorItem->setText(it->name.c_str());
        }

        QStandardItem* indexItem = new QStandardItem();
        indexItem->setData(m.index, Qt::DisplayRole);

        QStandardItem* timestampItem = new QStandardItem();
        QDateTime dt;
        dt.setTime_t(DB::Clock::to_time_t(m.time_point));
        timestampItem->setData(dt, Qt::DisplayRole);

        QStandardItem* temperatureItem = new QStandardItem();
        temperatureItem->setData(m.temperature, Qt::DisplayRole);

        QStandardItem* humidityItem = new QStandardItem();
        humidityItem->setData(m.humidity, Qt::DisplayRole);

        QStandardItem* batteryItem = new QStandardItem();
        batteryItem->setData(m.vcc, Qt::DisplayRole);

        QStandardItem* signalItem = new QStandardItem();
        signalItem->setData(std::min(m.b2s_input_dBm, m.s2b_input_dBm), Qt::DisplayRole);

        QStandardItem* errorsItem = new QStandardItem();
        errorsItem->setData("-", Qt::DisplayRole);

        m_model.appendRow({ sensorItem, indexItem, timestampItem, temperatureItem, humidityItem, batteryItem, signalItem, errorsItem });
    }
}
