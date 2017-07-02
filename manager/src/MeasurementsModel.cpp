#include "MeasurementsModel.h"

#include <QWidget>
#include <QIcon>
#include <bitset>

static std::array<const char*, 9> s_headerNames = {"Sensor", "Index", "Timestamp", "Temperature", "Humidity", "Battery", "Signal", "Sensor Errors", "Alarms"};
enum class Column
{
    Sensor,
    Index,
    Timestamp,
    Temperature,
    Humidity,
    Battery,
    Signal,
    SensorErrors,
    Alarms
};

float getBatteryLevel(float vcc)
{
    constexpr float max = 3.2f;
    constexpr float min = 2.f;
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
    size_t index = std::floor(level * (s_batteryIconNames.size() - 1) + 0.5f);
    return s_batteryIconNames[index];
}

float getSignalLevel(int8_t dBm)
{
    if (dBm == 0)
    {
        return 0.f;
    }
    constexpr float max = -40.f;
    constexpr float min = -110.f;
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
    size_t index = std::floor(level * (s_signalIconNames.size() - 1) + 0.5f);
    return s_signalIconNames[index];
}

//////////////////////////////////////////////////////////////////////////

MeasurementsModel::MeasurementsModel(DB& db)
    : QAbstractItemModel()
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

    if (role == Qt::UserRole + 5) // sorting
    {
        if (column == Column::Temperature)
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
            return getSignalLevel(std::min(measurement.descriptor.b2s, measurement.descriptor.s2b));
        }
        else if (column == Column::SensorErrors)
        {
            return static_cast<uint32_t>(std::bitset<8>(measurement.descriptor.sensorErrors).count());
        }
        else if (column == Column::Alarms)
        {
            return static_cast<uint32_t>(std::bitset<8>(measurement.triggeredAlarms).count());
        }

        return data(index, Qt::DisplayRole);
    }
    else if (role == Qt::DecorationRole)
    {
        if (column == Column::Sensor)
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
            return getSignalIcon(std::min(measurement.descriptor.b2s, measurement.descriptor.s2b));
        }
    }
    else if (role == Qt::DisplayRole)
    {
        if (column == Column::Sensor)
        {
            int32_t sensorIndex = m_db.findSensorIndexById(measurement.descriptor.sensorId);
            if (sensorIndex < 0)
            {
                return "N/A";
            }
            else
            {
                return m_db.getSensor(sensorIndex).descriptor.name.c_str();
            }
        }
        else if (column == Column::Index)
        {
            return measurement.descriptor.index;
        }
        else if (column == Column::Timestamp)
        {
            QDateTime dt;
            dt.setTime_t(DB::Clock::to_time_t(measurement.descriptor.timePoint));
            return dt;
        }
        else if (column == Column::Temperature)
        {
            return QString("%1 °C").arg(measurement.descriptor.temperature, 0, 'f', 1);
        }
        else if (column == Column::Humidity)
        {
            return QString("%1 % RH").arg(measurement.descriptor.humidity, 0, 'f', 1);
        }
        else if (column == Column::Battery)
        {
            return QString("%1 %").arg(static_cast<int>(getBatteryLevel(measurement.descriptor.vcc) * 100.f));
        }
        else if (column == Column::Signal)
        {
            return QString("%1 %").arg(static_cast<int>(getSignalLevel(std::min(measurement.descriptor.b2s, measurement.descriptor.s2b)) * 100.f));
            //return QString("%1 %").arg(std::min(measurement.descriptor.b2s, measurement.descriptor.s2b));
        }
        else if (column == Column::SensorErrors)
        {
            if (measurement.descriptor.sensorErrors == 0)
            {
                return "<none>";
            }
        }
        else if (column == Column::Alarms)
        {
            if (measurement.triggeredAlarms == 0)
            {
                return "<none>";
            }
        }
    }

    return QVariant();
}

//////////////////////////////////////////////////////////////////////////

Qt::ItemFlags MeasurementsModel::flags(QModelIndex const& index) const
{
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
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

constexpr QSize k_iconSize(16, 16);
constexpr int32_t k_iconMargin = 4;

//////////////////////////////////////////////////////////////////////////

void MeasurementsModel::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    if (!index.isValid() || static_cast<size_t>(index.row()) >= m_measurements.size())
    {
        return QStyledItemDelegate::paint(painter, option, index);
    }

    DB::Measurement const& measurement = m_measurements[index.row()];

    Column column = static_cast<Column>(index.column());
    if (column == Column::Alarms)
    {
        uint8_t triggeredAlarms = measurement.triggeredAlarms;
        if (triggeredAlarms == 0)
        {
            return QStyledItemDelegate::paint(painter, option, index);
        }

        QApplication::style()->drawControl(QStyle::CE_ItemViewItem, &option, painter);

        painter->save();

        QPoint pos = option.rect.topLeft();

        if (triggeredAlarms & DB::TriggeredAlarm::Temperature)
        {
            static QIcon icon(":/icons/ui/temperature.png");
            painter->drawPixmap(QRect(pos, k_iconSize), icon.pixmap(k_iconSize));
            pos.setX(pos.x() + k_iconSize.width() + k_iconMargin);
        }
        if (triggeredAlarms & DB::TriggeredAlarm::Humidity)
        {
            static QIcon icon(":/icons/ui/humidity.png");
            painter->drawPixmap(QRect(pos, k_iconSize), icon.pixmap(k_iconSize));
            pos.setX(pos.x() + k_iconSize.width() + k_iconMargin);
        }
        if (triggeredAlarms & DB::TriggeredAlarm::LowVcc)
        {
            static QIcon icon(":/icons/ui/battery-0.png");
            painter->drawPixmap(QRect(pos, k_iconSize), icon.pixmap(k_iconSize));
            pos.setX(pos.x() + k_iconSize.width() + k_iconMargin);
        }
        if (triggeredAlarms & DB::TriggeredAlarm::SensorErrors)
        {
            static QIcon icon(":/icons/ui/sensor-error.png");
            painter->drawPixmap(QRect(pos, k_iconSize), icon.pixmap(k_iconSize));
            pos.setX(pos.x() + k_iconSize.width() + k_iconMargin);
        }
        if (triggeredAlarms & DB::TriggeredAlarm::LowSignal)
        {
            static QIcon icon(":/icons/ui/signal-0.png");
            painter->drawPixmap(QRect(pos, k_iconSize), icon.pixmap(k_iconSize));
            pos.setX(pos.x() + k_iconSize.width() + k_iconMargin);
        }

        painter->restore();
        return;
    }
    else if (column == Column::SensorErrors)
    {
        uint8_t sensorErrors = measurement.descriptor.sensorErrors;
        if (sensorErrors == 0)
        {
            return QStyledItemDelegate::paint(painter, option, index);
        }

        QApplication::style()->drawControl(QStyle::CE_ItemViewItem, &option, painter);

        painter->save();

        QPoint pos = option.rect.topLeft();

        if (sensorErrors & DB::SensorErrors::Comms)
        {
            static QIcon icon(":/icons/ui/comms-error.png");
            painter->drawPixmap(QRect(pos, k_iconSize), icon.pixmap(k_iconSize));
            pos.setX(pos.x() + k_iconSize.width() + k_iconMargin);
        }
        if (sensorErrors & DB::SensorErrors::Hardware)
        {
            static QIcon icon(":/icons/ui/hardware-error.png");
            painter->drawPixmap(QRect(pos, k_iconSize), icon.pixmap(k_iconSize));
            pos.setX(pos.x() + k_iconSize.width() + k_iconMargin);
        }

        painter->restore();
        return;
    }

    return QStyledItemDelegate::paint(painter, option, index);
}

//////////////////////////////////////////////////////////////////////////

QSize MeasurementsModel::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    if (!index.isValid() || static_cast<size_t>(index.row()) >= m_measurements.size())
    {
        return QStyledItemDelegate::sizeHint(option, index);
    }

    DB::Measurement const& measurement = m_measurements[index.row()];

    Column column = static_cast<Column>(index.column());
    if (column == Column::Alarms)
    {
        uint8_t triggeredAlarms = measurement.triggeredAlarms;
        if (triggeredAlarms == 0)
        {
            return QStyledItemDelegate::sizeHint(option, index);
        }

        int32_t width = 0;
        if (triggeredAlarms & DB::TriggeredAlarm::Temperature)
        {
            width += k_iconSize.width() + k_iconMargin;
        }
        if (triggeredAlarms & DB::TriggeredAlarm::Humidity)
        {
            width += k_iconSize.width() + k_iconMargin;
        }
        if (triggeredAlarms & DB::TriggeredAlarm::LowVcc)
        {
            width += k_iconSize.width() + k_iconMargin;
        }
        if (triggeredAlarms & DB::TriggeredAlarm::SensorErrors)
        {
            width += k_iconSize.width() + k_iconMargin;
        }
        if (triggeredAlarms & DB::TriggeredAlarm::LowSignal)
        {
            width += k_iconSize.width() + k_iconMargin;
        }

        return QSize(width, k_iconSize.height());
    }
    else if (column == Column::SensorErrors)
    {
        uint8_t sensorErrors = measurement.descriptor.sensorErrors;
        if (sensorErrors == 0)
        {
            return QStyledItemDelegate::sizeHint(option, index);
        }

        int32_t width = 0;
        if (sensorErrors & DB::SensorErrors::Comms)
        {
            width += k_iconSize.width() + k_iconMargin;
        }
        if (sensorErrors & DB::SensorErrors::Hardware)
        {
            width += k_iconSize.width() + k_iconMargin;
        }

        return QSize(width, k_iconSize.height());
    }

    return QStyledItemDelegate::sizeHint(option, index);
}
