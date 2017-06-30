#include "MeasurementsModel.h"

#include <QWidget>
#include <QIcon>

static std::array<const char*, 9> s_headerNames = {"Sensor", "Index", "Timestamp", "Temperature", "Humidity", "Battery", "Signal", "Errors", "Alarms"};
enum class Column
{
    Sensor,
    Index,
    Timestamp,
    Temperature,
    Humidity,
    Battery,
    Signal,
    Errors,
    Alarms
};

static std::array<const char*, 5> s_batteryIconNames = { "battery-0.png", "battery-25.png", "battery-50.png", "battery-75.png", "battery-100.png" };

QIcon getBatteryIcon(float vcc)
{
    constexpr float max = 3.2f;
    constexpr float min = 2.f;
    float percentage = std::max(std::min(vcc, max) - min, 0.f) / (max - min);
    size_t index = std::floor(percentage * s_batteryIconNames.size() + 0.5f);
    return QIcon(QString(":/icons/ui/") + s_batteryIconNames[index]);
}

static std::array<const char*, 5> s_signalIconNames = { "signal-0.png", "signal-25.png", "signal-50.png", "signal-75.png", "signal-100.png" };

QIcon getSignalIcon(int8_t dBm)
{
    constexpr float max = -50.f;
    constexpr float min = -110.f;
    float percentage = std::max(std::min(static_cast<float>(dBm), max) - min, 0.f) / (max - min);
    size_t index = std::floor(percentage * s_signalIconNames.size() + 0.5f);
    return QIcon(QString(":/icons/ui/") + s_signalIconNames[index]);
}

//////////////////////////////////////////////////////////////////////////

MeasurementsModel::MeasurementsModel(Comms& comms, DB& db)
    : QAbstractItemModel()
    , m_comms(comms)
    , m_db(db)
{
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
        return QModelIndex();
    }
    if (!hasIndex(row, column, parent))
    {
        return QModelIndex();
    }
    return createIndex(row, column, nullptr);
}

//////////////////////////////////////////////////////////////////////////

QModelIndex MeasurementsModel::parent(QModelIndex const& index) const
{
    return QModelIndex();
}

//////////////////////////////////////////////////////////////////////////

int MeasurementsModel::rowCount(QModelIndex const& index) const
{
    if (!index.isValid())
    {
        return m_measurements.size();
    }
    else
    {
        return 0;
    }
}

//////////////////////////////////////////////////////////////////////////

int MeasurementsModel::columnCount(QModelIndex const& index) const
{
    return s_headerNames.size();
}

//////////////////////////////////////////////////////////////////////////

QVariant MeasurementsModel::headerData(int section, Qt::Orientation orientation, int role) const
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

QVariant MeasurementsModel::data(QModelIndex const& index, int role) const
{
    if (!index.isValid())
    {
        return QVariant();
    }

    if (static_cast<size_t>(index.row()) >= m_measurements.size())
    {
        return QVariant();
    }

    DB::Measurement const& measurement = m_measurements[index.row()];

    Column column = static_cast<Column>(index.column());
    if (role == Qt::DecorationRole)
    {
        if (column == Column::Sensor)
        {
            return QIcon(":/icons/ui/sensor.png");
        }
        else if (column == Column::Battery)
        {
            return getBatteryIcon(measurement.vcc);
        }
        else if (column == Column::Signal)
        {
            return getBatteryIcon(std::min(measurement.b2s, measurement.s2b));
        }
    }
    else if (role == Qt::DisplayRole)
    {
        if (column == Column::Sensor)
        {
            std::vector<Comms::Sensor> const& sensors = m_comms.getLastSensors();
            auto it = std::find_if(sensors.begin(), sensors.end(), [&measurement](Comms::Sensor const& sensor) { return sensor.id == measurement.sensorId; });
            if (it == sensors.end())
            {
                return "N/A";
            }
            else
            {
                return it->name.c_str();
            }
        }
        else if (column == Column::Index)
        {
            return measurement.index;
        }
        else if (column == Column::Timestamp)
        {
            QDateTime dt;
            dt.setTime_t(DB::Clock::to_time_t(measurement.timePoint));
            return dt;
        }
        else if (column == Column::Temperature)
        {
            return measurement.temperature;
        }
        else if (column == Column::Humidity)
        {
            return measurement.humidity;
        }
        else if (column == Column::Signal)
        {
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
            str.pop_back(); //the comma
            return str.c_str();
        }
        else if (column == Column::Alarms)
        {
            uint8_t triggeredAlarm = m_db.computeTriggeredAlarm(measurement);
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
            if (str.empty())
            {
                return "<none>";
            }
            str.pop_back();//the comma
            return str.c_str();
        }
    }

    return QVariant();
}

//////////////////////////////////////////////////////////////////////////

Qt::ItemFlags MeasurementsModel::flags(QModelIndex const& index) const
{
    return Qt::ItemIsEnabled;
}

//////////////////////////////////////////////////////////////////////////

void MeasurementsModel::setFilter(DB::Filter const& filter)
{
    beginResetModel();
    m_filter = filter;
    m_measurements = m_db.getFilteredMeasurements(m_filter);
    endResetModel();

    Q_EMIT layoutChanged();
}

//////////////////////////////////////////////////////////////////////////

size_t MeasurementsModel::getMeasurementCount() const
{
    return m_measurements.size();
}

//////////////////////////////////////////////////////////////////////////

bool MeasurementsModel::setData(QModelIndex const& index, QVariant const& value, int role)
{
    return false;
}

//////////////////////////////////////////////////////////////////////////

bool MeasurementsModel::setHeaderData(int section, Qt::Orientation orientation, QVariant const& value, int role)
{
    return false;
}

//////////////////////////////////////////////////////////////////////////

bool MeasurementsModel::insertColumns(int position, int columns, QModelIndex const& parent)
{
    return false;
}

//////////////////////////////////////////////////////////////////////////

bool MeasurementsModel::removeColumns(int position, int columns, QModelIndex const& parent)
{
    return false;
}

//////////////////////////////////////////////////////////////////////////

bool MeasurementsModel::insertRows(int position, int rows, QModelIndex const& parent)
{
    return false;
}

//////////////////////////////////////////////////////////////////////////

bool MeasurementsModel::removeRows(int position, int rows, QModelIndex const& parent)
{
    return false;
}
