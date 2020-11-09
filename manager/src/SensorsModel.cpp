#include "SensorsModel.h"

#include <QWidget>
#include <QIcon>
#include <QPainter>
#include <QDateTime>
#include <QAbstractItemModel>
#include <QTimer>

#include <algorithm>
#include <cassert>
#include "Utils.h"

static std::array<const char*, 9> s_headerNames = {"Name", "Serial Number", "Temperature", "Humidity", "Battery", "Signal", "Comms", "Stored", "Alarms"};

constexpr int32_t k_imminentMaxSecond = 5;
constexpr int32_t k_imminentMinSecond = -60;

constexpr int32_t k_alertStoredMeasurementCount = 600; //turn red above this many measurements
constexpr int32_t k_minStoredMeasurementCount = 10; //don't show stored measurements below this number

//////////////////////////////////////////////////////////////////////////

SensorsModel::SensorsModel(DB& db)
    : QAbstractItemModel()
    , m_db(db)
{
    connect(&m_db, &DB::sensorAdded, this, &SensorsModel::sensorAdded);
    connect(&m_db, &DB::sensorBound, this, &SensorsModel::sensorChanged);
    connect(&m_db, &DB::sensorChanged, this, &SensorsModel::sensorChanged);
    connect(&m_db, &DB::sensorDataChanged, this, &SensorsModel::sensorChanged, Qt::QueuedConnection);
    connect(&m_db, &DB::sensorRemoved, this, &SensorsModel::sensorRemoved);

    size_t sensorCount = m_db.getSensorCount();
    for (size_t i = 0; i < sensorCount; i++)
        sensorAdded(m_db.getSensor(i).id);

    m_timer.setSingleShot(false);
    m_timer.setInterval(5000);
    m_timer.start();
    connect(&m_timer, &QTimer::timeout, this, &SensorsModel::refreshDetails);
}

//////////////////////////////////////////////////////////////////////////

SensorsModel::~SensorsModel()
{
}

//////////////////////////////////////////////////////////////////////////

DB& SensorsModel::getDB()
{
    return m_db;
}

//////////////////////////////////////////////////////////////////////////

void SensorsModel::refreshDetails()
{
    for (size_t i = 0; i < m_db.getSensorCount(); i++)
        emit dataChanged(index((int)i, 0), index((int)i, columnCount() - 1));
}

//////////////////////////////////////////////////////////////////////////

size_t SensorsModel::getSensorCount() const
{
    return m_db.getSensorCount();
}

//////////////////////////////////////////////////////////////////////////

DB::Sensor SensorsModel::getSensor(size_t index) const
{
    return m_db.getSensor(index);
}

//////////////////////////////////////////////////////////////////////////

int32_t SensorsModel::getSensorIndex(QModelIndex index) const
{
    size_t indexRow = static_cast<size_t>(index.row());
    if (indexRow >= m_sensors.size())
        return -1;

    SensorData const& sensorData = m_sensors[indexRow];
    return m_db.findSensorIndexById(sensorData.sensorId);
}

//////////////////////////////////////////////////////////////////////////

void SensorsModel::sensorAdded(DB::SensorId id)
{
    if (m_db.findSensorIndexById(id) < 0)
    {
        assert(false);
        return;
    }

    //DB::Sensor sensor = m_db.getSensor(sensorIndex);

    emit beginInsertRows(QModelIndex(), int(m_sensors.size()), int(m_sensors.size()));
    SensorData sd;
    sd.sensorId = id;
    m_sensors.push_back(sd);
    emit endInsertRows();
}

//////////////////////////////////////////////////////////////////////////

void SensorsModel::sensorChanged(DB::SensorId id)
{
    auto it = std::find_if(m_sensors.begin(), m_sensors.end(), [id](SensorData const& sd) { return sd.sensorId == id; });
    if (it == m_sensors.end())
    {
        assert(false);
        return;
    }

    int32_t sensorIndex = std::distance(m_sensors.begin(), it);
    emit dataChanged(index(sensorIndex, 0), index(sensorIndex, columnCount() - 1));
}

//////////////////////////////////////////////////////////////////////////

void SensorsModel::sensorRemoved(DB::SensorId id)
{
    auto it = std::find_if(m_sensors.begin(), m_sensors.end(), [id](SensorData const& sd) { return sd.sensorId == id; });
    if (it == m_sensors.end())
    {
        assert(false);
        return;
    }

    int32_t sensorIndex = std::distance(m_sensors.begin(), it);
    emit beginRemoveRows(QModelIndex(), sensorIndex, sensorIndex);
    m_sensors.erase(it);
    emit endRemoveRows();
}

//////////////////////////////////////////////////////////////////////////

void SensorsModel::bindSensor(std::string const& name)
{

}

//////////////////////////////////////////////////////////////////////////

void SensorsModel::unbindSensor(DB::SensorId id)
{

}

//////////////////////////////////////////////////////////////////////////

void SensorsModel::setShowCheckboxes(bool show)
{
    emit layoutAboutToBeChanged();
    m_showCheckboxes = show;
    emit layoutChanged();
}

//////////////////////////////////////////////////////////////////////////

void SensorsModel::setSensorChecked(DB::SensorId id, bool checked)
{
    auto it = std::find_if(m_sensors.begin(), m_sensors.end(), [id](SensorData const& sd) { return sd.sensorId == id; });
    if (it == m_sensors.end())
        return;

    it->isChecked = checked;

    int32_t sensorIndex = std::distance(m_sensors.begin(), it);
    emit dataChanged(index(sensorIndex, 0), index(sensorIndex, columnCount() - 1));
}

//////////////////////////////////////////////////////////////////////////

bool SensorsModel::isSensorChecked(DB::SensorId id) const
{
    auto it = std::find_if(m_sensors.begin(), m_sensors.end(), [id](SensorData const& sd) { return sd.sensorId == id; });
    if (it == m_sensors.end())
        return false;

    return it->isChecked;
}

//////////////////////////////////////////////////////////////////////////

QModelIndex SensorsModel::index(int row, int column, QModelIndex const& parent) const
{
    if (row < 0 || column < 0)
        return QModelIndex();

    if (!hasIndex(row, column, parent))
        return QModelIndex();

    return createIndex(row, column, nullptr);
}

//////////////////////////////////////////////////////////////////////////

QModelIndex SensorsModel::parent(QModelIndex const& /*index*/) const
{
    return QModelIndex();
}

//////////////////////////////////////////////////////////////////////////

int SensorsModel::rowCount(QModelIndex const& index) const
{
    if (!index.isValid())
        return static_cast<int>(m_sensors.size());
    else
        return 0;
}

//////////////////////////////////////////////////////////////////////////

int SensorsModel::columnCount(QModelIndex const& /*index*/) const
{
    return static_cast<int>(s_headerNames.size());
}

//////////////////////////////////////////////////////////////////////////

QVariant SensorsModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
    {
        if (static_cast<size_t>(section) < s_headerNames.size())
            return s_headerNames[section];
    }
    return QAbstractItemModel::headerData(section, orientation, role);
}

//////////////////////////////////////////////////////////////////////////

QVariant SensorsModel::data(QModelIndex const& index, int role) const
{
    if (!index.isValid())
        return QVariant();

    size_t indexRow = static_cast<size_t>(index.row());
    if (indexRow >= m_sensors.size())
        return QVariant();

    SensorData const& sensorData = m_sensors[indexRow];
    std::optional<DB::Sensor> sensor = m_db.findSensorById(sensorData.sensorId);
    if (!sensor.has_value())
        return QVariant();

    Column column = static_cast<Column>(index.column());
    if (role == Qt::CheckStateRole && m_showCheckboxes)
    {
        if (column == Column::Name)
            return sensorData.isChecked ? Qt::Checked : Qt::Unchecked;
    }
    else if (role == Qt::BackgroundRole)
    {
        if (column == Column::NextComms)
        {
            if (sensor->state == DB::Sensor::State::Sleeping)
                return QVariant(QColor(150, 150, 150));
            else
            {
                if (sensor->blackout)
					return QVariant(QColor(255, 150, 150));

                if (sensor->lastCommsTimePoint.time_since_epoch().count() != 0)
                {
                    auto p = utils::computeRelativeTimePointString(sensor->lastCommsTimePoint + m_db.computeActualCommsPeriod(m_db.getLastSensorTimeConfig().descriptor));
                    if (p.second < k_imminentMaxSecond)
                        return QVariant(QColor(255, 255, 150));
                }
            }
        }
        else if (column == Column::Stored)
        {
            if (sensor->estimatedStoredMeasurementCount > k_alertStoredMeasurementCount)
                return QVariant(QColor(255, 150, 150));
        }
    }
    else if (role == Qt::DecorationRole)
    {
        if (column == Column::Name)
        {
            if (sensor->state == DB::Sensor::State::Active)
                return QIcon(":/icons/ui/sensor.png");

            if (sensor->state == DB::Sensor::State::Sleeping)
                return QIcon(":/icons/ui/sleeping.png");

            if (sensor->state == DB::Sensor::State::Unbound)
                return QIcon(":/icons/ui/unbound.png");
        }
        else if (column == Column::NextComms)
        {
            if (sensor->lastCommsTimePoint.time_since_epoch().count() == 0)
                return QIcon(":/icons/ui/question.png");
        }
        else if (column == Column::Stored)
        {
//            if (sensor->storedMeasurementCount < 0)
//                return QIcon(":/icons/ui/question.png");
        }
        else
        {
            if (sensor->isRTMeasurementValid)
            {
                if (column == Column::Battery)
                    return utils::getBatteryIcon(m_db.getSensorSettings(), sensor->rtMeasurementVcc);
                else if (column == Column::Signal)
                    return utils::getSignalIcon(m_db.getSensorSettings(), std::min(sensor->averageSignalStrength.s2b, sensor->averageSignalStrength.b2s));
                else if (column == Column::Temperature)
                    return QIcon(":/icons/ui/temperature.png");
                else if (column == Column::Humidity)
                    return QIcon(":/icons/ui/humidity.png");
            }
            else
            {
                if (column == Column::Battery ||
                    column == Column::Signal ||
                    column == Column::Temperature ||
                    column == Column::Humidity)
                    return QIcon(":/icons/ui/question.png");
            }
        }
    }
    else if (role == Qt::DisplayRole)
    {
        if (column == Column::Name)
            return sensor->descriptor.name.c_str();
        else if (column == Column::SerialNumber)
            return QString("%1").arg(sensor->serialNumber, 8, 16, QChar('0'));
        else if (column == Column::NextComms)
        {
            if (sensor->state == DB::Sensor::State::Sleeping)
                return "Sleeping...";
            else
            {
                if (sensor->lastCommsTimePoint.time_since_epoch().count() != 0)
                {
                    auto p = utils::computeRelativeTimePointString(sensor->lastCommsTimePoint + m_db.computeActualCommsPeriod(m_db.getLastSensorTimeConfig().descriptor));
                    std::string str = p.first;
                    str = (p.second > 0) ? "In " + str : str + " ago";
                    if (p.second < k_imminentMaxSecond && p.second > k_imminentMinSecond)
                        return "Imminent";

                    return str.c_str();
                }
            }
        }
        else if (column == Column::Stored)
        {
            return sensor->estimatedStoredMeasurementCount < k_minStoredMeasurementCount ? 0 : sensor->estimatedStoredMeasurementCount;
        }
        else if (column == Column::Alarms)
        {
            uint32_t triggers = 0;
            Result<DB::Measurement> res = m_db.getMostRecentMeasurementForSensor(sensor->id);
            triggers |= (res == success) ? res.payload().alarmTriggers.current : 0;
            if (sensor->blackout)
                triggers |= DB::AlarmTrigger::SensorBlackout;

            return triggers;
        }
        else
        {
            if (sensor->isRTMeasurementValid)
            {
                if (column == Column::Temperature)
                    return QString("%1°C").arg(sensor->rtMeasurementTemperature, 0, 'f', 1);
                else if (column == Column::Humidity)
                    return QString("%1 %RH").arg(sensor->rtMeasurementHumidity, 0, 'f', 1);
                else if (column == Column::Battery)
                    return QString("%1%").arg(static_cast<int>(utils::getBatteryLevel(sensor->rtMeasurementVcc) * 100.f));
                else if (column == Column::Signal)
                {
                    int average = static_cast<int>(utils::getSignalLevel(std::min(sensor->averageSignalStrength.s2b, sensor->averageSignalStrength.b2s)) * 100.f);
                    return QString("%1%").arg(average);
                }
            }
        }
    }
    else if (role == Qt::ToolTipRole)
    {
        if (column == Column::Alarms)
        {
			uint32_t triggers = 0;
			Result<DB::Measurement> res = m_db.getMostRecentMeasurementForSensor(sensor->id);
			triggers |= (res == success) ? res.payload().alarmTriggers.current : 0;
			if (sensor->blackout)
				triggers |= DB::AlarmTrigger::SensorBlackout;

			return QVariant(utils::sensorTriggersToString(triggers, true).c_str());
        }
    }

    return QVariant();
}

//////////////////////////////////////////////////////////////////////////

Qt::ItemFlags SensorsModel::flags(QModelIndex const& index) const
{
    Column column = static_cast<Column>(index.column());

    Qt::ItemFlags flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    if (m_showCheckboxes && column == Column::Name)
        flags |= Qt::ItemIsUserCheckable | Qt::ItemIsEditable;

    return flags;
}

//////////////////////////////////////////////////////////////////////////

bool SensorsModel::setData(QModelIndex const& index, QVariant const& value, int role)
{
    if (!index.isValid())
        return false;

    if (static_cast<size_t>(index.row()) >= m_sensors.size())
        return false;

    SensorData& sensorData = m_sensors[index.row()];

    Column column = static_cast<Column>(index.column());
    if (role == Qt::CheckStateRole && column == Column::Name)
    {
        sensorData.isChecked = value.toInt() == Qt::Checked;
        emit sensorCheckedChanged(sensorData.sensorId);
        return true;
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////

bool SensorsModel::setHeaderData(int /*section*/, Qt::Orientation /*orientation*/, QVariant const& /*value*/, int /*role*/)
{
    return false;
}

//////////////////////////////////////////////////////////////////////////

bool SensorsModel::insertColumns(int /*position*/, int /*columns*/, QModelIndex const& /*parent*/)
{
    return false;
}

//////////////////////////////////////////////////////////////////////////

bool SensorsModel::removeColumns(int /*position*/, int /*columns*/, QModelIndex const& /*parent*/)
{
    return false;
}

//////////////////////////////////////////////////////////////////////////

bool SensorsModel::insertRows(int /*position*/, int /*rows*/, QModelIndex const& /*parent*/)
{
    return false;
}

//////////////////////////////////////////////////////////////////////////

bool SensorsModel::removeRows(int /*position*/, int /*rows*/, QModelIndex const& /*parent*/)
{
    return false;
}


