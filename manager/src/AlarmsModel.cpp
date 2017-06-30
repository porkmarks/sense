#include "AlarmsModel.h"

#include <QWidget>
#include <QIcon>

static std::array<const char*, 8> s_headerNames = {"Name", "Temperature", "Humidity", "Low Battery", "Errors", "Low Signal", "Action"};
enum class Column
{
    Name,
    Temperature,
    Humidity,
    LowBattery,
    Errors,
    LowSignal,
    Action
};

//////////////////////////////////////////////////////////////////////////

AlarmsModel::AlarmsModel(DB& db)
    : QAbstractItemModel()
    , m_db(db)
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
        return m_db.getAlarmCount();
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

    if (static_cast<size_t>(index.row()) >= m_db.getAlarmCount())
    {
        return QVariant();
    }

    DB::Alarm const& alarm = m_db.getAlarm(index.row());

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
            std::string str;
            if (alarm.lowTemperatureWatch)
            {
                str += "< " + std::to_string(alarm.lowTemperature) + " °, ";
            }
            if (alarm.highTemperatureWatch)
            {
                str += "> " + std::to_string(alarm.highTemperature) + " °, ";
            }
            if (str.empty())
            {
                return "<ignored>";
            }
            str.pop_back(); //comma
            return str.c_str();
        }
        else if (column == Column::Humidity)
        {
            std::string str;
            if (alarm.lowHumidityWatch)
            {
                str += "< " + std::to_string(alarm.lowHumidity) + " %, ";
            }
            if (alarm.highHumidityWatch)
            {
                str += "> " + std::to_string(alarm.highHumidity) + " %, ";
            }
            if (str.empty())
            {
                return "<ignored>";
            }
            str.pop_back(); //comma
            return str.c_str();
        }
        else if (column == Column::LowBattery)
        {
            if (alarm.vccWatch)
            {
                return QString("Yes");
            }
            return QString("<ignored>");
        }
        else if (column == Column::LowSignal)
        {
            if (alarm.signalWatch)
            {
                return QString("Yes");
            }
            return QString("<ignored>");
        }
        else if (column == Column::Errors)
        {
            if (alarm.errorFlagsWatch)
            {
                return "Yes";
            }
            return "<ignored>";
        }
        else if (column == Column::Action)
        {
            if (alarm.sendEmailAction)
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
