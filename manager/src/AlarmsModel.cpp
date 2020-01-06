#include "AlarmsModel.h"

#include <array>
#include <QWidget>
#include <QIcon>
#include "Utils.h"

static std::array<const char*, 8> s_headerNames = {"Id", "Name", "Temperature", "Humidity", "Low Battery", "Low Signal"};

//////////////////////////////////////////////////////////////////////////

AlarmsModel::AlarmsModel(DB& db)
    : m_db(db)
{
}

//////////////////////////////////////////////////////////////////////////

AlarmsModel::~AlarmsModel()
{
}

//////////////////////////////////////////////////////////////////////////

void AlarmsModel::refresh()
{
    beginResetModel();
    endResetModel();

    Q_EMIT layoutChanged();
}

//////////////////////////////////////////////////////////////////////////

QModelIndex AlarmsModel::index(int row, int column, QModelIndex const& parent) const
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

QModelIndex AlarmsModel::parent(QModelIndex const& /*index*/) const
{
    return QModelIndex();
}

//////////////////////////////////////////////////////////////////////////

int AlarmsModel::rowCount(QModelIndex const& index) const
{
    if (!index.isValid())
    {
        return static_cast<int>(m_db.getAlarmCount());
    }
    return 0;
}

//////////////////////////////////////////////////////////////////////////

int AlarmsModel::columnCount(QModelIndex const& /*index*/) const
{
    return static_cast<int>(s_headerNames.size());
}

//////////////////////////////////////////////////////////////////////////

QVariant AlarmsModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
    {
        if (static_cast<size_t>(section) < s_headerNames.size())
        {
            return s_headerNames[static_cast<size_t>(section)];
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

    DB::Alarm const& alarm = m_db.getAlarm(static_cast<size_t>(index.row()));
    DB::AlarmDescriptor const& descriptor = alarm.descriptor;

    Column column = static_cast<Column>(index.column());
    if (role == Qt::CheckStateRole)
    {
        if (column == Column::LowBattery)
        {
            if (descriptor.lowVccWatch)
            {
                return Qt::Checked;
            }
        }
        else if (column == Column::LowSignal)
        {
            if (descriptor.lowSignalWatch)
            {
                return Qt::Checked;
            }
        }
//        else if (column == Column::SensorErrors)
//        {
//            if (descriptor.sensorErrorsWatch)
//            {
//                return Qt::Checked;
//            }
//        }
    }
    else if (role == Qt::DisplayRole)
    {
        if (column == Column::Id)
        {
            return alarm.id;
        }
        else if (column == Column::Name)
        {
            return descriptor.name.c_str();
        }
        else if (column == Column::Temperature)
        {
            QString str;
            if (descriptor.lowTemperatureWatch)
            {
                str += QString("&lt; <font color=\"#%2\">%1°C</font>").arg(descriptor.lowTemperatureHard, 0, 'f', 1).arg(utils::k_lowThresholdHardColor, 0, 16);
                str += QString("<br>&lt; <font color=\"#%2\">%1°C</font>").arg(descriptor.lowTemperatureSoft, 0, 'f', 1).arg(utils::k_lowThresholdSoftColor, 0, 16);
            }
            if (descriptor.highTemperatureWatch)
            {
                if (!str.isEmpty())
                {
                    str += "<br>";
                }
				str += QString("&gt; <font color=\"#%2\">%1°C</font>").arg(descriptor.highTemperatureSoft, 0, 'f', 1).arg(utils::k_highThresholdSoftColor, 0, 16);
				str += QString("<br>&gt; <font color=\"#%2\">%1°C</font>").arg(descriptor.highTemperatureHard, 0, 'f', 1).arg(utils::k_highThresholdHardColor, 0, 16);
            }
            return str;
        }
        else if (column == Column::Humidity)
        {
			QString str;
			if (descriptor.lowHumidityWatch)
			{
				str += QString("&lt; <font color=\"#%2\">%1%</font>").arg(descriptor.lowHumidityHard, 0, 'f', 1).arg(utils::k_lowThresholdHardColor, 0, 16);
				str += QString("<br>&lt; <font color=\"#%2\">%1%</font>").arg(descriptor.lowHumiditySoft, 0, 'f', 1).arg(utils::k_lowThresholdSoftColor, 0, 16);
			}
			if (descriptor.highHumidityWatch)
			{
				if (!str.isEmpty())
				{
					str += "<br>";
				}
				str += QString("&gt; <font color=\"#%2\">%1%</font>").arg(descriptor.highHumiditySoft, 0, 'f', 1).arg(utils::k_highThresholdSoftColor, 0, 16);
				str += QString("<br>&gt; <font color=\"#%2\">%1%</font>").arg(descriptor.highHumidityHard, 0, 'f', 1).arg(utils::k_highThresholdHardColor, 0, 16);
			}
			return str;
        }
    }
    else if (role == Qt::DecorationRole)
    {
        if (column == Column::Id)
        {
            return QIcon(":/icons/ui/alarm.png");
        }
        else if (column == Column::Temperature && (descriptor.lowTemperatureWatch || descriptor.highTemperatureWatch))
        {
            return QIcon(":/icons/ui/temperature.png");
        }
        else if (column == Column::Humidity && (descriptor.lowHumidityWatch || descriptor.highHumidityWatch))
        {
            return QIcon(":/icons/ui/humidity.png");
        }
        else if (column == Column::LowBattery && descriptor.lowVccWatch)
        {
            return QIcon(":/icons/ui/battery-0.png");
        }
        else if (column == Column::LowSignal && descriptor.lowSignalWatch)
        {
            return QIcon(":/icons/ui/signal-0.png");
        }
    }

    return QVariant();
}

//////////////////////////////////////////////////////////////////////////

Qt::ItemFlags AlarmsModel::flags(QModelIndex const& /*index*/) const
{
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

//////////////////////////////////////////////////////////////////////////

bool AlarmsModel::setData(QModelIndex const& /*index*/, QVariant const& /*value*/, int /*role*/)
{
    return false;
}

//////////////////////////////////////////////////////////////////////////

bool AlarmsModel::setHeaderData(int /*section*/, Qt::Orientation /*orientation*/, QVariant const& /*value*/, int /*role*/)
{
    return false;
}

//////////////////////////////////////////////////////////////////////////

bool AlarmsModel::insertColumns(int /*position*/, int /*columns*/, QModelIndex const& /*parent*/)
{
    return false;
}

//////////////////////////////////////////////////////////////////////////

bool AlarmsModel::removeColumns(int /*position*/, int /*columns*/, QModelIndex const& /*parent*/)
{
    return false;
}

//////////////////////////////////////////////////////////////////////////

bool AlarmsModel::insertRows(int /*position*/, int /*rows*/, QModelIndex const& /*parent*/)
{
    return false;
}

//////////////////////////////////////////////////////////////////////////

bool AlarmsModel::removeRows(int /*position*/, int /*rows*/, QModelIndex const& /*parent*/)
{
    return false;
}
