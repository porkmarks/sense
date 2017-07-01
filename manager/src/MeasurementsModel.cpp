#include "MeasurementsModel.h"

#include <QWidget>
#include <QIcon>

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

static std::array<const char*, 5> s_batteryIconNames = { "battery-0.png", "battery-25.png", "battery-50.png", "battery-75.png", "battery-100.png" };

QIcon getBatteryIcon(float vcc)
{
    constexpr float max = 3.2f;
    constexpr float min = 2.f;
    float percentage = std::max(std::min(vcc, max) - min, 0.f) / (max - min);
    size_t index = std::floor(percentage * (s_batteryIconNames.size() - 1) + 0.5f);
    return QIcon(QString(":/icons/ui/") + s_batteryIconNames[index]);
}

static std::array<const char*, 6> s_signalIconNames = { "signal-0.png", "signal-20.png", "signal-40.png", "signal-60.png", "signal-80.png", "signal-100.png" };

QIcon getSignalIcon(int8_t dBm)
{
    constexpr float max = -50.f;
    constexpr float min = -110.f;
    float percentage = std::max(std::min(static_cast<float>(dBm), max) - min, 0.f) / (max - min);
    size_t index = std::floor(percentage * (s_signalIconNames.size() - 1) + 0.5f);
    return QIcon(QString(":/icons/ui/") + s_signalIconNames[index]);
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
    if (role == Qt::DecorationRole)
    {
        if (column == Column::Sensor)
        {
            return QIcon(":/icons/ui/sensor.png");
        }
        else if (column == Column::Temperature)
        {
            return QIcon(":/icons/ui/temperature.png");
        }
        else if (column == Column::Humidity)
        {
            return QIcon(":/icons/ui/humidity.png");
        }
        else if (column == Column::Battery)
        {
            return getBatteryIcon(measurement.vcc);
        }
        else if (column == Column::Signal)
        {
            return getSignalIcon(std::min(measurement.b2s, measurement.s2b));
        }
    }
    else if (role == Qt::DisplayRole)
    {
        if (column == Column::Sensor)
        {
            int32_t sensorIndex = m_db.findSensorIndexById(measurement.sensorId);
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
            return QString("%1 °C").arg(measurement.temperature);
        }
        else if (column == Column::Humidity)
        {
            return QString("%1 % RH").arg(measurement.humidity);
        }
        else if (column == Column::SensorErrors)
        {
            if (measurement.sensorErrors == 0)
            {
                return "<none>";
            }
        }
        else if (column == Column::Alarms)
        {
            uint8_t triggeredAlarms = m_db.computeTriggeredAlarm(measurement);
            if (triggeredAlarms == 0)
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
        uint8_t triggeredAlarms = m_db.computeTriggeredAlarm(measurement);
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
            static QIcon icon(":/icons/ui/sensor.png");
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
        if (measurement.sensorErrors == 0)
        {
            return QStyledItemDelegate::paint(painter, option, index);
        }

        QApplication::style()->drawControl(QStyle::CE_ItemViewItem, &option, painter);

        painter->save();

        QPoint pos = option.rect.topLeft();

        if (measurement.sensorErrors & DB::SensorErrors::CommsError)
        {
            static QIcon icon(":/icons/ui/signal-0.png");
            painter->drawPixmap(QRect(pos, k_iconSize), icon.pixmap(k_iconSize));
            pos.setX(pos.x() + k_iconSize.width() + k_iconMargin);
        }
        if (measurement.sensorErrors & DB::SensorErrors::SensorError)
        {
            static QIcon icon(":/icons/ui/sensor.png");
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
        uint8_t triggeredAlarms = m_db.computeTriggeredAlarm(measurement);
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
        if (measurement.sensorErrors == 0)
        {
            return QStyledItemDelegate::sizeHint(option, index);
        }

        int32_t width = 0;
        if (measurement.sensorErrors & DB::SensorErrors::CommsError)
        {
            width += k_iconSize.width() + k_iconMargin;
        }
        if (measurement.sensorErrors & DB::SensorErrors::SensorError)
        {
            width += k_iconSize.width() + k_iconMargin;
        }

        return QSize(width, k_iconSize.height());
    }

    return QStyledItemDelegate::sizeHint(option, index);
}