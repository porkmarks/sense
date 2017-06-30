#include "SensorsModel.h"

#include <QWidget>
#include <QIcon>

static std::array<const char*, 12> s_headerNames = {"Name", "Id", "Address", "Temperature", "Humidity", "Battery", "Signal", "State", "Next Measurement", "Next Comms", "Errors", "Alarms"};
enum class Column
{
    Name,
    Id,
    Address,
    Temperature,
    Humidity,
    Battery,
    Signal,
    State,
    NextMeasurement,
    NextComms,
    Errors,
    Alarms
};

extern QIcon getBatteryIcon(float vcc);
extern QIcon getSignalIcon(int8_t dBm);

//////////////////////////////////////////////////////////////////////////

SensorsModel::SensorsModel(Comms& comms, DB& db)
    : QAbstractItemModel()
    , m_comms(comms)
    , m_db(db)
{
    QObject::connect(&m_comms, &Comms::sensorAdded, this, &SensorsModel::sensorAdded);

    std::vector<Comms::Sensor> const& sensors = m_comms.getLastSensors();
    for (Comms::Sensor const& sensor: sensors)
    {
        sensorAdded(sensor);
    }
}

//////////////////////////////////////////////////////////////////////////

SensorsModel::~SensorsModel()
{
}

//////////////////////////////////////////////////////////////////////////

void SensorsModel::sensorAdded(Comms::Sensor const& sensor)
{
    auto it = std::find_if(m_sensors.begin(), m_sensors.end(), [&sensor](SensorData const& sd) { return sd.sensor.id == sensor.id; });
    if (it != m_sensors.end())
    {
        it->sensor = sensor;
        int idx = std::distance(m_sensors.begin(), it);
        emit dataChanged(index(idx, 0), index(idx, columnCount()));
        return;
    }

    emit beginInsertRows(QModelIndex(), m_sensors.size(), m_sensors.size());
    SensorData sd;
    sd.sensor = sensor;
    m_sensors.push_back(sd);
    emit endInsertRows();
}

//////////////////////////////////////////////////////////////////////////

void SensorsModel::bindSensor(std::string const& name)
{

}

//////////////////////////////////////////////////////////////////////////

void SensorsModel::unbindSensor(Comms::Sensor_Id id)
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

void SensorsModel::setSensorChecked(Comms::Sensor_Id id, bool checked)
{
    auto it = std::find_if(m_sensors.begin(), m_sensors.end(), [id](SensorData const& sd) { return sd.sensor.id == id; });
    if (it == m_sensors.end())
    {
        return;
    }
    it->isChecked = checked;
}

//////////////////////////////////////////////////////////////////////////

bool SensorsModel::isSensorChecked(Comms::Sensor_Id id) const
{
    auto it = std::find_if(m_sensors.begin(), m_sensors.end(), [id](SensorData const& sd) { return sd.sensor.id == id; });
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
    Comms::Sensor const& sensor = sensorData.sensor;

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
        else if (column == Column::Id)
        {
        }
        else if (column == Column::Address)
        {
        }
        else if (column == Column::State)
        {
        }
        else if (column == Column::NextMeasurement)
        {
        }
        else if (column == Column::NextComms)
        {
        }
        else
        {
            DB::Measurement measurement;
            if (m_db.getLastMeasurementForSensor(sensor.id, measurement))
            {
                if (column == Column::Battery)
                {
                    return getBatteryIcon(measurement.vcc);
                }
                else if (column == Column::Signal)
                {
                    return getSignalIcon(std::min(measurement.b2s, measurement.s2b));
                }
            }
            else
            {
                if (column == Column::Temperature ||
                    column == Column::Humidity ||
                    column == Column::Errors)
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
            return sensor.name.c_str();
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
        }
        else if (column == Column::NextMeasurement)
        {
        }
        else if (column == Column::NextComms)
        {
        }
        else if (column == Column::Alarms)
        {
            uint8_t triggeredAlarm = m_db.computeTriggeredAlarm(sensor.id);
            std::string str;
            if (triggeredAlarm && DB::TriggeredAlarm::Temperature)
            {
                str += "T, ";
            }
            if (triggeredAlarm && DB::TriggeredAlarm::Humidity)
            {
                str += "H, ";
            }
            if (triggeredAlarm && DB::TriggeredAlarm::Vcc)
            {
                str += "B, ";
            }
            if (triggeredAlarm && DB::TriggeredAlarm::ErrorFlags)
            {
                str += "E, ";
            }
            if (triggeredAlarm && DB::TriggeredAlarm::Signal)
            {
                str += "S, ";
            }
            if (str.empty())
            {
                return "<none>";
            }
            str.pop_back();//the comma
            return str.c_str();
        }
        else
        {
            DB::Measurement measurement;
            if (m_db.getLastMeasurementForSensor(sensor.id, measurement))
            {
                if (column == Column::Temperature)
                {
                    return measurement.temperature;
                }
                else if (column == Column::Humidity)
                {
                    return measurement.humidity;
                }
                else if (column == Column::Errors)
                {
                    std::string str;
                    if (measurement.errorFlags && DB::ErrorFlag::CommsError)
                    {
                        str += "C, ";
                    }
                    if (measurement.errorFlags && DB::ErrorFlag::SensorError)
                    {
                        str += "S, ";
                    }
                    if (str.empty())
                    {
                        return "<none>";
                    }
                    str.pop_back(); //comma
                    return str.c_str();
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
