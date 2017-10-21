#include "MeasurementsDelegate.h"
#include "MeasurementsModel.h"

#include <QWidget>
#include <QIcon>
#include <bitset>

constexpr QSize k_iconMargin(4, 2);

//////////////////////////////////////////////////////////////////////////

MeasurementsDelegate::MeasurementsDelegate(QAbstractItemModel& model)
    : m_model(model)
{
}

//////////////////////////////////////////////////////////////////////////

MeasurementsDelegate::~MeasurementsDelegate()
{
}

//////////////////////////////////////////////////////////////////////////

void MeasurementsDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    if (!index.isValid())
    {
        return QStyledItemDelegate::paint(painter, option, index);
    }

    MeasurementsModel::Column column = static_cast<MeasurementsModel::Column>(index.column());
    if (column == MeasurementsModel::Column::Alarms)
    {
        uint8_t triggeredAlarms = m_model.data(index).toUInt();

        QApplication::style()->drawControl(QStyle::CE_ItemViewItem, &option, painter);

        painter->save();

        QPoint pos = option.rect.topLeft() + QPoint(k_iconMargin.width(), k_iconMargin.height());
        int32_t iconSize = option.rect.height();

        if (triggeredAlarms & DB::TriggeredAlarm::Temperature)
        {
            static QIcon icon(":/icons/ui/temperature.png");
            painter->drawPixmap(pos, icon.pixmap(option.rect.size() - k_iconMargin * 2));
            pos.setX(pos.x() + iconSize + k_iconMargin.width());
        }
        if (triggeredAlarms & DB::TriggeredAlarm::Humidity)
        {
            static QIcon icon(":/icons/ui/humidity.png");
            painter->drawPixmap(pos, icon.pixmap(option.rect.size() - k_iconMargin * 2));
            pos.setX(pos.x() + iconSize + k_iconMargin.width());
        }
        if (triggeredAlarms & DB::TriggeredAlarm::LowVcc)
        {
            static QIcon icon(":/icons/ui/battery-0.png");
            painter->drawPixmap(pos, icon.pixmap(option.rect.size() - k_iconMargin * 2));
            pos.setX(pos.x() + iconSize + k_iconMargin.width());
        }
//        if (triggeredAlarms & DB::TriggeredAlarm::SensorErrors)
//        {
//            static QIcon icon(":/icons/ui/sensor-error.png");
//          painter->drawPixmap(option.rect, icon.pixmap(option.rect.size() - QSize(2, 2)));
//            pos.setX(pos.x() + k_iconSize.width() + k_iconMargin);
//        }
        if (triggeredAlarms & DB::TriggeredAlarm::LowSignal)
        {
            static QIcon icon(":/icons/ui/signal-0.png");
            painter->drawPixmap(pos, icon.pixmap(option.rect.size() - k_iconMargin * 2));
            pos.setX(pos.x() + iconSize + k_iconMargin.width());
        }

        painter->restore();
        return;
    }
//    else if (column == MeasurementsModel::Column::SensorErrors)
//    {
//        uint8_t sensorErrors = m_model.data(index).toUInt();

//        QApplication::style()->drawControl(QStyle::CE_ItemViewItem, &option, painter);

//        painter->save();

//        QPoint pos = option.rect.topLeft();

//        if (sensorErrors & DB::SensorErrors::Comms)
//        {
//            static QIcon icon(":/icons/ui/comms-error.png");
//            painter->drawPixmap(QRect(pos, k_iconSize), icon.pixmap(k_iconSize));
//            pos.setX(pos.x() + k_iconSize.width() + k_iconMargin);
//        }
//        if (sensorErrors & DB::SensorErrors::Hardware)
//        {
//            static QIcon icon(":/icons/ui/hardware-error.png");
//            painter->drawPixmap(QRect(pos, k_iconSize), icon.pixmap(k_iconSize));
//            pos.setX(pos.x() + k_iconSize.width() + k_iconMargin);
//        }

//        painter->restore();
//        return;
//    }

    return QStyledItemDelegate::paint(painter, option, index);
}

//////////////////////////////////////////////////////////////////////////

QSize MeasurementsDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    if (!index.isValid())
    {
        return QStyledItemDelegate::sizeHint(option, index);
    }

    MeasurementsModel::Column column = static_cast<MeasurementsModel::Column>(index.column());
    if (column == MeasurementsModel::Column::Alarms)
    {
        uint8_t triggeredAlarms = m_model.data(index).toUInt();

        int32_t width = 0;
        int32_t iconSize = option.rect.height();

        if (triggeredAlarms & DB::TriggeredAlarm::Temperature)
        {
            width += iconSize + k_iconMargin.width();
        }
        if (triggeredAlarms & DB::TriggeredAlarm::Humidity)
        {
            width += iconSize + k_iconMargin.width();
        }
        if (triggeredAlarms & DB::TriggeredAlarm::LowVcc)
        {
            width += iconSize + k_iconMargin.width();
        }
//        if (triggeredAlarms & DB::TriggeredAlarm::SensorErrors)
//        {
//            width += iconSize + k_iconMargin.width();
//        }
        if (triggeredAlarms & DB::TriggeredAlarm::LowSignal)
        {
            width += iconSize + k_iconMargin.width();
        }

        return QSize(width, iconSize);
    }
//    else if (column == MeasurementsModel::Column::SensorErrors)
//    {
//        uint8_t sensorErrors = m_model.data(index).toUInt();

//        int32_t width = 0;
//        if (sensorErrors & DB::SensorErrors::Comms)
//        {
//            width += iconSize + k_iconMargin.width();
//        }
//        if (sensorErrors & DB::SensorErrors::Hardware)
//        {
//            width += k_iconSize.width() + k_iconMargin;
//        }

//        return QSize(width, k_iconSize.height());
//    }

    return QStyledItemDelegate::sizeHint(option, index);
}
