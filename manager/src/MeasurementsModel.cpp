#include "MeasurementsModel.h"

#include <QWidget>
#include <QIcon>
#include <QDateTime>

#include <cmath>
#include <bitset>
#include <array>
#include <cassert>
#include <iostream>

static std::array<const char*, 10> s_headerNames = {"Id", "Sensor", "Index", "Timestamp", "Temperature", "Humidity", "Battery", "Signal", "Alarms"};

float getBatteryLevel(float vcc)
{
    constexpr float max = 3.2f;
    constexpr float min = 2.4f;
    float level = std::max(std::min(vcc, max) - min, 0.f) / (max - min);
    return level;
}

QIcon getBatteryIcon(float vcc)
{
    static std::array<QIcon, 5> s_batteryIconNames =
    {
        QIcon(":/icons/ui/battery-0.png"),
        QIcon(":/icons/ui/battery-25.png"),
        QIcon(":/icons/ui/battery-50.png"),
        QIcon(":/icons/ui/battery-75.png"),
        QIcon(":/icons/ui/battery-100.png")
    };

    float level = getBatteryLevel(vcc);
    size_t index = static_cast<size_t>(std::floor(level * (s_batteryIconNames.size() - 1) + 0.5f));
    return s_batteryIconNames[index];
}

float getSignalLevel(int8_t dBm)
{
    if (dBm == 0)
    {
        return 0.f;
    }
    constexpr float max = -70.f;
    constexpr float min = -120.f;
    float level = std::max(std::min(static_cast<float>(dBm), max) - min, 0.f) / (max - min);
    return level;
}

QIcon getSignalIcon(int8_t dBm)
{
    static std::array<QIcon, 6> s_signalIconNames =
    {
        QIcon(":/icons/ui/signal-0.png"),
        QIcon(":/icons/ui/signal-20.png"),
        QIcon(":/icons/ui/signal-40.png"),
        QIcon(":/icons/ui/signal-60.png"),
        QIcon(":/icons/ui/signal-80.png"),
        QIcon(":/icons/ui/signal-100.png")
    };

    float level = getSignalLevel(dBm);
    size_t index = static_cast<size_t>(std::floor(level * (s_signalIconNames.size() - 1) + 0.5f));
    return s_signalIconNames[index];
}

//////////////////////////////////////////////////////////////////////////

MeasurementsModel::MeasurementsModel(DB& db)
    : m_db(db)
{
    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setSingleShot(true);

    connect(&db, &DB::measurementsAdded, this, &MeasurementsModel::startAutoRefresh);
    connect(m_refreshTimer, &QTimer::timeout, this, &MeasurementsModel::refresh);

    m_columnsVisible.resize(s_headerNames.size(), true);
    refreshVisibleColumnToRealColumn();
}

//////////////////////////////////////////////////////////////////////////

MeasurementsModel::~MeasurementsModel()
{
}

//////////////////////////////////////////////////////////////////////////

QModelIndex MeasurementsModel::index(int row, int column, QModelIndex const& parent) const
{
    if (row < 0 || column < 0)
    {
        return {};
    }
    if (!hasIndex(row, column, parent))
    {
        return {};
    }
    return createIndex(row, column, nullptr);
}

//////////////////////////////////////////////////////////////////////////

QModelIndex MeasurementsModel::parent(QModelIndex const& /*index*/) const
{
    return QModelIndex();
}

//////////////////////////////////////////////////////////////////////////

int MeasurementsModel::rowCount(QModelIndex const& index) const
{
    if (!index.isValid())
    {
        return static_cast<int>(m_measurements.size());
    }
    return 0;
}

//////////////////////////////////////////////////////////////////////////

int MeasurementsModel::columnCount(QModelIndex const& /*index*/) const
{
    int cc = static_cast<int>(m_columnsVisibleCount);
    return cc;
}

//////////////////////////////////////////////////////////////////////////

void MeasurementsModel::refreshVisibleColumnToRealColumn()
{
    m_visibleColumnToRealColumn.clear();
    for (size_t i = 0; i < s_headerNames.size(); i++)
    {
        if (m_columnsVisible[i] == true)
        {
            m_visibleColumnToRealColumn.push_back(static_cast<Column>(i));
        }
    }
    m_columnsVisibleCount = std::count(m_columnsVisible.begin(), m_columnsVisible.end(), true);
}

//////////////////////////////////////////////////////////////////////////

QVariant MeasurementsModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
    {
        return s_headerNames[static_cast<size_t>(m_visibleColumnToRealColumn[static_cast<size_t>(section)])];
    }
    return QAbstractItemModel::headerData(section, orientation, role);
}

//////////////////////////////////////////////////////////////////////////

QVariant MeasurementsModel::data(QModelIndex const& index, int role) const
{
    if (!index.isValid())
    {
        return QVariant();
    }

    size_t indexRow = static_cast<size_t>(index.row());
    if (indexRow >= m_measurements.size())
    {
        return QVariant();
    }

    DB::Measurement const& measurement = m_measurements[indexRow];

    //code to skip over disabled columns
    Column column = m_visibleColumnToRealColumn[static_cast<size_t>(index.column())];

    if (role == UserRole::RealColumnRole)
    {
        return QVariant(static_cast<qlonglong>(column));
    }
    else if (role == UserRole::SortingRole)
    {
        if (column == Column::Timestamp)
        {
            return static_cast<qlonglong>(DB::Clock::to_time_t(measurement.timePoint));
        }
        else if (column == Column::Temperature)
        {
            return measurement.descriptor.temperature;
        }
        else if (column == Column::Humidity)
        {
            return measurement.descriptor.humidity;
        }
        else if (column == Column::Battery)
        {
            return getBatteryLevel(measurement.descriptor.vcc);
        }
        else if (column == Column::Signal)
        {
            return getSignalLevel(measurement.combinedSignalStrength);
        }
//        else if (column == Column::SensorErrors)
//        {
//            return static_cast<uint32_t>(std::bitset<8>(measurement.descriptor.sensorErrors).count());
//        }
        else if (column == Column::Alarms)
        {
            return static_cast<uint32_t>(std::bitset<8>(measurement.alarmTriggers).count());
        }

        return data(index, Qt::DisplayRole);
    }
    else if (role == Qt::DecorationRole)
    {
        if (column == Column::Id)
        {
            static QIcon icon(":/icons/ui/sensor.png");
            return icon;
        }
        else if (column == Column::Temperature)
        {
            static QIcon icon(":/icons/ui/temperature.png");
            return icon;
        }
        else if (column == Column::Humidity)
        {
            static QIcon icon(":/icons/ui/humidity.png");
            return icon;
        }
        else if (column == Column::Battery)
        {
            return getBatteryIcon(measurement.descriptor.vcc);
        }
        else if (column == Column::Signal)
        {
            return getSignalIcon(measurement.combinedSignalStrength);
        }
    }
    else if (role == Qt::DisplayRole)
    {
        if (column == Column::Id)
        {
            return static_cast<qulonglong>(measurement.id);
        }
        else if (column == Column::Sensor)
        {
            int32_t sensorIndex = m_db.findSensorIndexById(measurement.descriptor.sensorId);
            if (sensorIndex < 0)
            {
                return "N/A";
            }
            else
            {
                return m_db.getSensor(static_cast<size_t>(sensorIndex)).descriptor.name.c_str();
            }
        }
        else if (column == Column::Index)
        {
            return measurement.descriptor.index;
        }
        else if (column == Column::Timestamp)
        {
            QDateTime dt;
            dt.setTime_t(DB::Clock::to_time_t(measurement.timePoint));
            return dt.toString("dd-MM-yyyy HH:mm");
        }
        else if (column == Column::Temperature)
        {
            return QString("%1 °C").arg(measurement.descriptor.temperature, 0, 'f', 1);
        }
        else if (column == Column::Humidity)
        {
            return QString("%1 %RH").arg(measurement.descriptor.humidity, 0, 'f', 1);
        }
        else if (column == Column::Battery)
        {
            return QString("%1 %").arg(static_cast<int>(getBatteryLevel(measurement.descriptor.vcc) * 100.f));
            //return QString("%1 %").arg(static_cast<int>(measurement.descriptor.signalStrength.s2b));
        }
        else if (column == Column::Signal)
        {
            return QString("%1 %").arg(static_cast<int>(getSignalLevel(measurement.combinedSignalStrength) * 100.f));
            //return QString("%1 %").arg(static_cast<int>(measurement.descriptor.signalStrength.b2s));
            //return QString("%1 %").arg(std::min(measurement.descriptor.b2s, measurement.descriptor.s2b));
        }
//        else if (column == Column::SensorErrors)
//        {
//            return measurement.descriptor.sensorErrors;
//        }
        else if (column == Column::Alarms)
        {
            return measurement.alarmTriggers;
        }
    }

    return QVariant();
}

//////////////////////////////////////////////////////////////////////////

Qt::ItemFlags MeasurementsModel::flags(QModelIndex const& /*index*/) const
{
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

//////////////////////////////////////////////////////////////////////////

void MeasurementsModel::setFilter(DB::Filter const& filter)
{
    beginResetModel();
    m_filter = filter;
    //auto start = DB::Clock::now();
    m_measurements = m_db.getFilteredMeasurements(m_filter);
    //std::cout << "XXX: " << std::chrono::duration_cast<std::chrono::microseconds>(DB::Clock::now() - start).count() << "\n";
    endResetModel();

    Q_EMIT layoutChanged();
}

//////////////////////////////////////////////////////////////////////////

void MeasurementsModel::setColumnVisibility(Column column, bool visibility)
{
    m_columnsVisible[static_cast<size_t>(column)] = visibility;
    refreshVisibleColumnToRealColumn();
}

//////////////////////////////////////////////////////////////////////////

void MeasurementsModel::refresh()
{
    beginResetModel();
    m_measurements = m_db.getFilteredMeasurements(m_filter);
    endResetModel();

    Q_EMIT layoutChanged();
}

//////////////////////////////////////////////////////////////////////////

void MeasurementsModel::startAutoRefresh(DB::SensorId sensorId)
{
    //see if the sensor is in the filter
    if (m_filter.useSensorFilter)
    {
        if (m_filter.sensorIds.find(sensorId) == m_filter.sensorIds.end())
        {
            return;
        }
    }

    m_refreshTimer->start(5000);
}

//////////////////////////////////////////////////////////////////////////

size_t MeasurementsModel::getMeasurementCount() const
{
    return m_measurements.size();
}

//////////////////////////////////////////////////////////////////////////

DB::Measurement const& MeasurementsModel::getMeasurement(size_t index) const
{
    return m_measurements[index];
}

//////////////////////////////////////////////////////////////////////////

bool MeasurementsModel::setData(QModelIndex const& /*index*/, QVariant const& /*value*/, int /*role*/)
{
    return false;
}

//////////////////////////////////////////////////////////////////////////

bool MeasurementsModel::setHeaderData(int /*section*/, Qt::Orientation /*orientation*/, QVariant const& /*value*/, int /*role*/)
{
    return false;
}

//////////////////////////////////////////////////////////////////////////

bool MeasurementsModel::insertColumns(int /*position*/, int /*columns*/, QModelIndex const& /*parent*/)
{
    return false;
}

//////////////////////////////////////////////////////////////////////////

bool MeasurementsModel::removeColumns(int /*position*/, int /*columns*/, QModelIndex const& /*parent*/)
{
    return false;
}

//////////////////////////////////////////////////////////////////////////

bool MeasurementsModel::insertRows(int /*position*/, int /*rows*/, QModelIndex const& /*parent*/)
{
    return false;
}

//////////////////////////////////////////////////////////////////////////

bool MeasurementsModel::removeRows(int /*position*/, int /*rows*/, QModelIndex const& /*parent*/)
{
    return false;
}

