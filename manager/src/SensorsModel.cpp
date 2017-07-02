#include "SensorsModel.h"

#include <QWidget>
#include <QIcon>
#include <QPainter>
#include <QDateTime>
#include <cassert>

static std::array<const char*, 12> s_headerNames = {"Name", "Id", "Address", "Temperature", "Humidity", "Battery", "Signal", "State", "Next Measurement", "Next Comms", "Sensor Errors", "Alarms"};

extern float getBatteryLevel(float vcc);
extern QIcon getBatteryIcon(float vcc);
extern float getSignalLevel(int8_t dBm);
extern QIcon getSignalIcon(int8_t dBm);

//////////////////////////////////////////////////////////////////////////

SensorsModel::SensorsModel(DB& db)
    : QAbstractItemModel()
    , m_db(db)
{
    connect(&m_db, &DB::sensorAdded, this, &SensorsModel::sensorAdded);
    connect(&m_db, &DB::sensorBound, this, &SensorsModel::sensorChanged);
    connect(&m_db, &DB::sensorChanged, this, &SensorsModel::sensorChanged);
    connect(&m_db, &DB::sensorDataChanged, this, &SensorsModel::sensorChanged);
    connect(&m_db, &DB::sensorRemoved, this, &SensorsModel::sensorRemoved);

    size_t sensorCount = m_db.getSensorCount();
    for (size_t i = 0; i < sensorCount; i++)
    {
        sensorAdded(m_db.getSensor(i).id);
    }
}

//////////////////////////////////////////////////////////////////////////

SensorsModel::~SensorsModel()
{
}

//////////////////////////////////////////////////////////////////////////

size_t SensorsModel::getSensorCount() const
{
    return m_db.getSensorCount();
}

//////////////////////////////////////////////////////////////////////////

DB::Sensor const& SensorsModel::getSensor(size_t index) const
{
    return m_db.getSensor(index);
}

//////////////////////////////////////////////////////////////////////////

void SensorsModel::sensorAdded(DB::SensorId id)
{
    int32_t sensorIndex = m_db.findSensorIndexById(id);
    if (sensorIndex < 0)
    {
        assert(false);
        return;
    }

    DB::Sensor const& sensor = m_db.getSensor(sensorIndex);

    emit beginInsertRows(QModelIndex(), m_sensors.size(), m_sensors.size());
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
    {
        return;
    }
    it->isChecked = checked;
}

//////////////////////////////////////////////////////////////////////////

bool SensorsModel::isSensorChecked(DB::SensorId id) const
{
    auto it = std::find_if(m_sensors.begin(), m_sensors.end(), [id](SensorData const& sd) { return sd.sensorId == id; });
    if (it == m_sensors.end())
    {
        return false;
    }
    return it->isChecked;
}

//////////////////////////////////////////////////////////////////////////

QModelIndex SensorsModel::index(int row, int column, QModelIndex const& parent) const
{
    if (row < 0 || column < 0)
    {
        return QModelIndex();
    }
    if (!hasIndex(row, column, parent))
    {
        return QModelIndex();
    }
    return createIndex(row, column, nullptr);
}

//////////////////////////////////////////////////////////////////////////

QModelIndex SensorsModel::parent(QModelIndex const& index) const
{
    return QModelIndex();
}

//////////////////////////////////////////////////////////////////////////

int SensorsModel::rowCount(QModelIndex const& index) const
{
    if (!index.isValid())
    {
        return m_sensors.size();
    }
    else
    {
        return 0;
    }
}

//////////////////////////////////////////////////////////////////////////

int SensorsModel::columnCount(QModelIndex const& index) const
{
    return s_headerNames.size();
}

//////////////////////////////////////////////////////////////////////////

QVariant SensorsModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
    {
        if (static_cast<size_t>(section) < s_headerNames.size())
        {
            return s_headerNames[section];
        }
    }
    return QAbstractItemModel::headerData(section, orientation, role);
}

//////////////////////////////////////////////////////////////////////////

QVariant SensorsModel::data(QModelIndex const& index, int role) const
{
    if (!index.isValid())
    {
        return QVariant();
    }

    if (static_cast<size_t>(index.row()) >= m_sensors.size())
    {
        return QVariant();
    }

    SensorData const& sensorData = m_sensors[index.row()];
    int32_t dbSensorIndex = m_db.findSensorIndexById(sensorData.sensorId);
    assert(dbSensorIndex >= 0);
    if (dbSensorIndex < 0)
    {
        return QVariant();
    }
    DB::Sensor const& sensor = m_db.getSensor(dbSensorIndex);

    Column column = static_cast<Column>(index.column());
    if (role == Qt::CheckStateRole && m_showCheckboxes)
    {
        if (column == Column::Name)
        {
            return sensorData.isChecked ? Qt::Checked : Qt::Unchecked;
        }
    }
    else if (role == Qt::DecorationRole)
    {
        if (column == Column::Name)
        {
            return QIcon(":/icons/ui/sensor.png");
        }
        else if (column == Column::NextMeasurement)
        {
            if (sensor.nextMeasurementTimePoint.time_since_epoch().count() == 0)
            {
                return QIcon(":/icons/ui/question.png");
            }
        }
        else if (column == Column::NextComms)
        {
            if (sensor.nextCommsTimePoint.time_since_epoch().count() == 0)
            {
                return QIcon(":/icons/ui/question.png");
            }
        }
        else
        {
            if (sensor.isLastMeasurementValid)
            {
                if (column == Column::Battery)
                {
                    return getBatteryIcon(sensor.lastMeasurement.descriptor.vcc);
                }
                else if (column == Column::Signal)
                {
                    return getSignalIcon(std::min(sensor.lastMeasurement.descriptor.b2s, sensor.lastMeasurement.descriptor.s2b));
                }
                else if (column == Column::Temperature)
                {
                    return QIcon(":/icons/ui/temperature.png");
                }
                else if (column == Column::Humidity)
                {
                    return QIcon(":/icons/ui/humidity.png");
                }
            }
            else
            {
                if (column == Column::Battery ||
                    column == Column::Signal ||
                    column == Column::Temperature ||
                    column == Column::Humidity)
                {
                    return QIcon(":/icons/ui/question.png");
                }
            }
        }
    }
    else if (role == Qt::DisplayRole)
    {
        if (column == Column::Name)
        {
            return sensor.descriptor.name.c_str();
        }
        else if (column == Column::Id)
        {
            return sensor.id;
        }
        else if (column == Column::Address)
        {
            return sensor.address;
        }
        else if (column == Column::State)
        {
            if (sensor.state == DB::Sensor::State::Active)
            {
                return "Active";
            }
            if (sensor.state == DB::Sensor::State::Sleeping)
            {
                return "Sleeping";
            }
            if (sensor.state == DB::Sensor::State::Unbound)
            {
                return "Not bound";
            }
        }
        else if (column == Column::NextMeasurement)
        {
            if (sensor.nextMeasurementTimePoint.time_since_epoch().count() != 0)
            {
                time_t next = DB::Clock::to_time_t(sensor.nextMeasurementTimePoint);
                QDateTime dt;
                dt.setTime_t(next);
                return dt;
            }
        }
        else if (column == Column::NextComms)
        {
            if (sensor.nextCommsTimePoint.time_since_epoch().count() != 0)
            {
                time_t next = DB::Clock::to_time_t(sensor.nextCommsTimePoint);
                QDateTime dt;
                dt.setTime_t(next);
                return dt;
            }
        }
        else if (column == Column::Alarms)
        {
            return sensor.isLastMeasurementValid ? sensor.lastMeasurement.triggeredAlarms : -1;
        }
        else if (column == Column::SensorErrors)
        {
            return sensor.isLastMeasurementValid ? sensor.lastMeasurement.descriptor.sensorErrors : -1;
        }
        else
        {
            if (sensor.isLastMeasurementValid)
            {
                if (column == Column::Temperature)
                {
                    return QString("%1 °C").arg(sensor.lastMeasurement.descriptor.temperature, 0, 'f', 1);
                }
                else if (column == Column::Humidity)
                {
                    return QString("%1 % RH").arg(sensor.lastMeasurement.descriptor.humidity, 0, 'f', 1);
                }
                else if (column == Column::Battery)
                {
                    return QString("%1 %").arg(static_cast<int>(getBatteryLevel(sensor.lastMeasurement.descriptor.vcc) * 100.f));
                }
                else if (column == Column::Signal)
                {
                    return QString("%1 %").arg(static_cast<int>(getSignalLevel(std::min(sensor.lastMeasurement.descriptor.b2s, sensor.lastMeasurement.descriptor.s2b)) * 100.f));
                }
            }
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
    {
        flags |= Qt::ItemIsUserCheckable | Qt::ItemIsEditable;
    }
    return flags;
}

//////////////////////////////////////////////////////////////////////////

bool SensorsModel::setData(QModelIndex const& index, QVariant const& value, int role)
{
    if (!index.isValid())
    {
        return false;
    }

    if (static_cast<size_t>(index.row()) >= m_sensors.size())
    {
        return false;
    }

    SensorData& sensorData = m_sensors[index.row()];

    Column column = static_cast<Column>(index.column());
    if (role == Qt::CheckStateRole && column == Column::Name)
    {
        sensorData.isChecked = value.toInt() == Qt::Checked;
        return true;
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////

bool SensorsModel::setHeaderData(int section, Qt::Orientation orientation, QVariant const& value, int role)
{

    return false;
}

//////////////////////////////////////////////////////////////////////////

bool SensorsModel::insertColumns(int position, int columns, QModelIndex const& parent)
{
    return false;
}

//////////////////////////////////////////////////////////////////////////

bool SensorsModel::removeColumns(int position, int columns, QModelIndex const& parent)
{
    return false;
}

//////////////////////////////////////////////////////////////////////////

bool SensorsModel::insertRows(int position, int rows, QModelIndex const& parent)
{
    return false;
}

//////////////////////////////////////////////////////////////////////////

bool SensorsModel::removeRows(int position, int rows, QModelIndex const& parent)
{
    return false;
}


