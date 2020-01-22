#include "MeasurementsModel.h"

#include <QWidget>
#include <QIcon>
#include <QDateTime>

#include <cmath>
#include <bitset>
#include <array>
#include <cassert>
#include <iostream>
#include "Utils.h"

static std::array<const char*, 10> s_headerNames = {"Id", "Sensor", "Index", "Timestamp", "Received Timestamp", "Temperature", "Humidity", "Battery", "Signal", "Alarms"};

constexpr size_t k_chunkSize = 10000;

//////////////////////////////////////////////////////////////////////////

MeasurementsModel::MeasurementsModel(DB& db)
    : m_db(db)
{
    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setSingleShot(true);
}

//////////////////////////////////////////////////////////////////////////

MeasurementsModel::~MeasurementsModel()
{
}

//////////////////////////////////////////////////////////////////////////

void MeasurementsModel::setActive(bool active)
{
	for (const QMetaObject::Connection& connection : m_connections)
	{
		QObject::disconnect(connection);
	}
    m_connections.clear();

	m_connections.push_back(connect(&m_db, &DB::measurementsAdded, this, &MeasurementsModel::startSlowAutoRefresh));
	m_connections.push_back(connect(&m_db, &DB::measurementsChanged, this, &MeasurementsModel::startFastAutoRefresh));
	m_connections.push_back(connect(m_refreshTimer, &QTimer::timeout, this, &MeasurementsModel::refresh));
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
    return (int)s_headerNames.size();
}

//////////////////////////////////////////////////////////////////////////

QVariant MeasurementsModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (section < 0)
    {
        int a = 0;
        section = 0;
    }
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
    {
        return s_headerNames[static_cast<size_t>(section)];
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
    Column column = static_cast<Column>(index.column());

//     if (role == UserRole::SortingRole)
//     {
//         if (column == Column::Timestamp)
//         {
//             return static_cast<qlonglong>(IClock::to_time_t(measurement.timePoint));
//         }
// 		else if (column == Column::ReceivedTimestamp)
// 		{
// 			return static_cast<qlonglong>(IClock::to_time_t(measurement.receivedTimePoint));
// 		}
//         else if (column == Column::Temperature)
//         {
//             return measurement.descriptor.temperature;
//         }
//         else if (column == Column::Humidity)
//         {
//             return measurement.descriptor.humidity;
//         }
//         else if (column == Column::Battery)
//         {
//             return utils::getBatteryLevel(measurement.descriptor.vcc);
//         }
//         else if (column == Column::Signal)
//         {
// 			return utils::getSignalLevel(std::min(measurement.descriptor.signalStrength.s2b, measurement.descriptor.signalStrength.b2s));
//         }
//         else if (column == Column::Alarms)
//         {
//             return static_cast<uint32_t>(std::bitset<32>(measurement.alarmTriggers.current).count());
//         }
// 
//         return data(index, Qt::DisplayRole);
//     }
    if (role == Qt::DecorationRole)
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
            return utils::getBatteryIcon(m_db.getSensorSettings(), measurement.descriptor.vcc);
        }
        else if (column == Column::Signal)
        {
            return utils::getSignalIcon(m_db.getSensorSettings(), std::min(measurement.descriptor.signalStrength.s2b, measurement.descriptor.signalStrength.b2s));
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
            std::optional<DB::Sensor> sensor = m_db.findSensorById(measurement.descriptor.sensorId);
            if (sensor.has_value())
            {
                return sensor->descriptor.name.c_str();
            }
            else
            {
                return "N/A";
            }
        }
        else if (column == Column::Index)
        {
            return measurement.descriptor.index;
        }
        else if (column == Column::Timestamp)
        {
			return utils::toString<IClock>(measurement.timePoint, m_db.getGeneralSettings().dateTimeFormat);
        }
		else if (column == Column::ReceivedTimestamp)
		{
			return utils::toString<IClock>(measurement.receivedTimePoint, m_db.getGeneralSettings().dateTimeFormat);
		}
        else if (column == Column::Temperature)
        {
            return QString("%1°C").arg(measurement.descriptor.temperature, 0, 'f', 1);
        }
        else if (column == Column::Humidity)
        {
            return QString("%1 %RH").arg(measurement.descriptor.humidity, 0, 'f', 1);
        }
        else if (column == Column::Battery)
        {
            return QString("%1%").arg(static_cast<int>(utils::getBatteryLevel(measurement.descriptor.vcc) * 100.f));
            //return QString("%1 %").arg(static_cast<int>(measurement.descriptor.signalStrength.s2b));
        }
        else if (column == Column::Signal)
        {
            return QString("%1%").arg(static_cast<int>(utils::getSignalLevel(std::min(measurement.descriptor.signalStrength.s2b, measurement.descriptor.signalStrength.b2s)) * 100.f));
            //return QString("%1 %").arg(static_cast<int>(measurement.descriptor.signalStrength.b2s));
            //return QString("%1 %").arg(std::min(measurement.descriptor.b2s, measurement.descriptor.s2b));
        }
        else if (column == Column::Alarms)
        {
            return measurement.alarmTriggers.current;
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
	switch (m_sortColumn)
	{
	case Column::Id: m_filter.sortBy = DB::Filter::SortBy::Id; break;
	case Column::Sensor: m_filter.sortBy = DB::Filter::SortBy::SensorId; break;
	case Column::Index: m_filter.sortBy = DB::Filter::SortBy::Index; break;
	case Column::Timestamp: m_filter.sortBy = DB::Filter::SortBy::Timestamp; break;
	case Column::ReceivedTimestamp: m_filter.sortBy = DB::Filter::SortBy::ReceivedTimestamp; break;
	case Column::Temperature: m_filter.sortBy = DB::Filter::SortBy::Temperature; break;
	case Column::Humidity: m_filter.sortBy = DB::Filter::SortBy::Humidity; break;
	case Column::Battery: m_filter.sortBy = DB::Filter::SortBy::Battery; break;
	case Column::Signal: m_filter.sortBy = DB::Filter::SortBy::Signal; break;
	case Column::Alarms: m_filter.sortBy = DB::Filter::SortBy::Alarms; break;
	default:
		break;
	}
    m_filter.sortOrder = m_sortOrder;
    //auto start = IClock::now();
    m_measurementsStartIndex = 0;
    m_measurementsTotalCount = m_db.getFilteredMeasurementCount(m_filter);
    m_measurements = m_db.getFilteredMeasurements(m_filter, m_measurementsStartIndex, k_chunkSize);
    //std::cout << "XXX: " << std::chrono::duration_cast<std::chrono::microseconds>(IClock::now() - start).count() << "\n";
    endResetModel();

    Q_EMIT layoutChanged();
}

//////////////////////////////////////////////////////////////////////////

DB::Filter const& MeasurementsModel::getFilter() const
{
    return m_filter;
}

//////////////////////////////////////////////////////////////////////////

void MeasurementsModel::refresh()
{
    beginResetModel();
    m_measurementsStartIndex = 0;
    m_measurementsTotalCount = m_db.getFilteredMeasurementCount(m_filter);
    m_measurements = m_db.getFilteredMeasurements(m_filter, m_measurementsStartIndex, k_chunkSize);
    endResetModel();

    Q_EMIT layoutChanged();
}

//////////////////////////////////////////////////////////////////////////

void MeasurementsModel::startSlowAutoRefresh()
{
	startAutoRefresh(std::chrono::milliseconds(10000));
}

//////////////////////////////////////////////////////////////////////////

void MeasurementsModel::startFastAutoRefresh()
{
    startAutoRefresh(std::chrono::milliseconds(3000));
}

//////////////////////////////////////////////////////////////////////////

void MeasurementsModel::startAutoRefresh(IClock::duration timer)
{
	m_refreshTimer->start(std::chrono::duration_cast<std::chrono::milliseconds>(timer).count());
}

//////////////////////////////////////////////////////////////////////////

size_t MeasurementsModel::getMeasurementCount() const
{
    return m_measurementsTotalCount;
}

//////////////////////////////////////////////////////////////////////////

DB::Measurement const& MeasurementsModel::getMeasurement(size_t index)
{
    while (index >= m_measurements.size())
    {
        fetchMore(QModelIndex());
    }
    return m_measurements[index];
}

//////////////////////////////////////////////////////////////////////////

Result<DB::Measurement> MeasurementsModel::getMeasurement(QModelIndex index) const
{
	if (!index.isValid())
	{
		return Error("Invalid Index");
	}

	size_t indexRow = static_cast<size_t>(index.row());
	if (indexRow >= m_measurements.size())
	{
        return Error("Invalid Index");
	}

	return m_measurements[indexRow];
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

//////////////////////////////////////////////////////////////////////////

void MeasurementsModel::sort(int column, Qt::SortOrder order)
{
	Column sortColumn = static_cast<Column>(column);
	DB::Filter::SortOrder sortOrder = order == Qt::AscendingOrder ? DB::Filter::SortOrder::Ascending : DB::Filter::SortOrder::Descending;

    if (m_sortColumn != sortColumn || m_sortOrder != sortOrder)
    {
        m_sortColumn = sortColumn;
        m_sortOrder = sortOrder;
        setFilter(getFilter());
    }
}

//////////////////////////////////////////////////////////////////////////

void MeasurementsModel::fetchMore(const QModelIndex& parent)
{
	beginInsertRows(QModelIndex(), int(m_measurementsStartIndex), int(m_measurementsStartIndex) + k_chunkSize);

    m_measurementsStartIndex = m_measurements.size();
    std::vector<DB::Measurement> measurements = m_db.getFilteredMeasurements(m_filter, m_measurementsStartIndex, k_chunkSize);
    m_measurements.resize(m_measurementsStartIndex + measurements.size());
    std::move(measurements.begin(), measurements.end(), m_measurements.begin() + m_measurementsStartIndex);

	endInsertRows();
}

//////////////////////////////////////////////////////////////////////////

bool MeasurementsModel::canFetchMore(const QModelIndex& parent) const
{
    return m_measurements.size() < m_measurementsTotalCount;
}


