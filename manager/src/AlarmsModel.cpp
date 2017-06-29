#include "AlarmsModel.h"

#include <QWidget>
#include <QIcon>

static std::array<const char*, 7> s_headerNames = {"Name", "Temperature", "Humidity", "Battery", "Errors", "Action"};
enum class Column
{
    Name,
    Temperature,
    Humidity,
    Battery,
    Errors,
    Action
};

//////////////////////////////////////////////////////////////////////////

AlarmsModel::AlarmsModel(Alarms& alarms)
    : QAbstractItemModel()
    , m_alarms(alarms)
{
}

//////////////////////////////////////////////////////////////////////////

AlarmsModel::~AlarmsModel()
{
}

//////////////////////////////////////////////////////////////////////////

QModelIndex AlarmsModel::index(int row, int column, QModelIndex const& parent) const
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

QModelIndex AlarmsModel::parent(QModelIndex const& index) const
{
    return QModelIndex();
}

//////////////////////////////////////////////////////////////////////////

int AlarmsModel::rowCount(QModelIndex const& index) const
{
    if (!index.isValid())
    {
        return m_alarms.getAlarmCount();
    }
    else
    {
        return 0;
    }
}

//////////////////////////////////////////////////////////////////////////

int AlarmsModel::columnCount(QModelIndex const& index) const
{
    return s_headerNames.size();
}

//////////////////////////////////////////////////////////////////////////

QVariant AlarmsModel::headerData(int section, Qt::Orientation orientation, int role) const
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

QVariant AlarmsModel::data(QModelIndex const& index, int role) const
{
    if (!index.isValid())
    {
        return QVariant();
    }

    if (static_cast<size_t>(index.row()) >= m_alarms.getAlarmCount())
    {
        return QVariant();
    }

    Alarms::Alarm const& alarm = m_alarms.getAlarm(index.row());

    Column column = static_cast<Column>(index.column());
    if (role == Qt::DecorationRole)
    {
        if (column == Column::Name)
        {
            return QIcon(":/icons/ui/alarm.png");
        }
    }
    else if (role == Qt::DisplayRole)
    {
        if (column == Column::Temperature)
        {
            if (alarm.temperatureWatch == Alarms::Watch::Bigger)
            {
                return QString("> %1 °").arg(alarm.temperature);
            }
            else if (alarm.temperatureWatch == Alarms::Watch::Smaller)
            {
                return QString("< %1 °").arg(alarm.temperature);
            }
            return QString("<ignored>");
        }
        else if (column == Column::Humidity)
        {
            if (alarm.humidityWatch == Alarms::Watch::Bigger)
            {
                return QString("> %1 %").arg(alarm.humidity);
            }
            else if (alarm.humidityWatch == Alarms::Watch::Smaller)
            {
                return QString("< %1 %").arg(alarm.humidity);
            }
            return QString("<ignored>");
        }
        else if (column == Column::Battery)
        {
            if (alarm.vccWatch)
            {
                return QString("Yes");
            }
            return QString("<ignored>");
        }
        else if (column == Column::Errors)
        {
            std::string str;
            if (alarm.flags && DB::Measurement::Flag::COMMS_ERROR)
            {
                str += "Comms";
            }
            if (alarm.flags && DB::Measurement::Flag::SENSOR_ERROR)
            {
                if (!str.empty())
                {
                    str += ", ";
                }
                str += "Sensor";
            }
            if (str.empty())
            {
                str = "<ignored>";
            }
            return str.c_str();
        }
        else if (column == Column::Action)
        {
            if (alarm.action == Alarms::Action::Email)
            {
                return ("Email " + alarm.emailRecipient).c_str();
            }
            return "<none>";
        }
    }

    return QVariant();
}

//////////////////////////////////////////////////////////////////////////

Qt::ItemFlags AlarmsModel::flags(QModelIndex const& index) const
{
    return Qt::ItemIsEnabled;
}

//////////////////////////////////////////////////////////////////////////

bool AlarmsModel::setData(QModelIndex const& index, QVariant const& value, int role)
{
    return false;
}

//////////////////////////////////////////////////////////////////////////

bool AlarmsModel::setHeaderData(int section, Qt::Orientation orientation, QVariant const& value, int role)
{
    return false;
}

//////////////////////////////////////////////////////////////////////////

bool AlarmsModel::insertColumns(int position, int columns, QModelIndex const& parent)
{
    return false;
}

//////////////////////////////////////////////////////////////////////////

bool AlarmsModel::removeColumns(int position, int columns, QModelIndex const& parent)
{
    return false;
}

//////////////////////////////////////////////////////////////////////////

bool AlarmsModel::insertRows(int position, int rows, QModelIndex const& parent)
{
    return false;
}

//////////////////////////////////////////////////////////////////////////

bool AlarmsModel::removeRows(int position, int rows, QModelIndex const& parent)
{
    return false;
}
