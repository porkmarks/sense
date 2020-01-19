#include "SensorsDelegate.h"
#include "SensorsModel.h"

#include <QWidget>
#include <QIcon>
#include <QPainter>
#include <QDateTime>
#include <QApplication>

#include <cassert>

constexpr QSize k_iconMargin(4, 2);

//////////////////////////////////////////////////////////////////////////

SensorsDelegate::SensorsDelegate(QAbstractItemModel& model)
    : m_model(model)
{
}

//////////////////////////////////////////////////////////////////////////

SensorsDelegate::~SensorsDelegate()
{
}

//////////////////////////////////////////////////////////////////////////

void SensorsDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    if (!index.isValid())
    {
        return QStyledItemDelegate::paint(painter, option, index);
    }

    SensorsModel::Column column = static_cast<SensorsModel::Column>(index.column());
    if (column == SensorsModel::Column::Alarms)
    {
        int alarmTriggers = m_model.data(index).toInt();

        QApplication::style()->drawControl(QStyle::CE_ItemViewItem, &option, painter);

        painter->save();

        QPoint pos = option.rect.topLeft() + QPoint(k_iconMargin.width(), k_iconMargin.height());
        int32_t iconSize = option.rect.height();

        if (alarmTriggers == -1)
        {
//             static QIcon icon(":/icons/ui/question.png");
//             painter->drawPixmap(pos, icon.pixmap(option.rect.size() - k_iconMargin * 2));
//             pos.setX(pos.x() + iconSize + k_iconMargin.width());
        }
        else
        {
            if (alarmTriggers & DB::AlarmTrigger::MeasurementLowTemperatureMask)
            {
                static QIcon icon(":/icons/ui/temperature.png");
                painter->drawPixmap(pos, icon.pixmap(option.rect.size() - k_iconMargin * 2));
                pos.setX(pos.x() + iconSize + k_iconMargin.width());
            }
			if (alarmTriggers & DB::AlarmTrigger::MeasurementHighTemperatureMask)
			{
				static QIcon icon(":/icons/ui/temperature.png");
				painter->drawPixmap(pos, icon.pixmap(option.rect.size() - k_iconMargin * 2));
				pos.setX(pos.x() + iconSize + k_iconMargin.width());
			}
            if (alarmTriggers & DB::AlarmTrigger::MeasurementLowHumidityMask)
            {
                static QIcon icon(":/icons/ui/humidity.png");
                painter->drawPixmap(pos, icon.pixmap(option.rect.size() - k_iconMargin * 2));
                pos.setX(pos.x() + iconSize + k_iconMargin.width());
            }
			if (alarmTriggers & DB::AlarmTrigger::MeasurementHighHumidityMask)
			{
				static QIcon icon(":/icons/ui/humidity.png");
				painter->drawPixmap(pos, icon.pixmap(option.rect.size() - k_iconMargin * 2));
				pos.setX(pos.x() + iconSize + k_iconMargin.width());
			}
            if (alarmTriggers & DB::AlarmTrigger::MeasurementLowVcc)
            {
                static QIcon icon(":/icons/ui/battery-0.png");
                painter->drawPixmap(pos, icon.pixmap(option.rect.size() - k_iconMargin * 2));
                pos.setX(pos.x() + iconSize + k_iconMargin.width());
            }
            if (alarmTriggers & DB::AlarmTrigger::MeasurementLowSignal)
            {
                static QIcon icon(":/icons/ui/signal-0.png");
                painter->drawPixmap(pos, icon.pixmap(option.rect.size() - k_iconMargin * 2));
                pos.setX(pos.x() + iconSize + k_iconMargin.width());
            }
        }

        painter->restore();
        return;
    }

    return QStyledItemDelegate::paint(painter, option, index);
}

//////////////////////////////////////////////////////////////////////////

QSize SensorsDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    if (!index.isValid())
    {
        return QStyledItemDelegate::sizeHint(option, index);
    }

    SensorsModel::Column column = static_cast<SensorsModel::Column>(index.column());
    if (column == SensorsModel::Column::Alarms)
    {
        int alarmTriggers = m_model.data(index).toInt();

        int32_t width = 0;
        int32_t iconSize = option.rect.height();

        if (alarmTriggers == -1)
        {
            width += iconSize + k_iconMargin.width();
        }
        else
        {
            if (alarmTriggers & DB::AlarmTrigger::MeasurementHighTemperatureMask)
            {
                width += iconSize + k_iconMargin.width();
            }
			if (alarmTriggers & DB::AlarmTrigger::MeasurementLowTemperatureMask)
			{
				width += iconSize + k_iconMargin.width();
			}
            if (alarmTriggers & DB::AlarmTrigger::MeasurementHighHumidityMask)
            {
                width += iconSize + k_iconMargin.width();
            }
			if (alarmTriggers & DB::AlarmTrigger::MeasurementLowHumidityMask)
			{
				width += iconSize + k_iconMargin.width();
			}
            if (alarmTriggers & DB::AlarmTrigger::MeasurementLowVcc)
            {
                width += iconSize + k_iconMargin.width();
            }
            if (alarmTriggers & DB::AlarmTrigger::MeasurementLowSignal)
            {
                width += iconSize + k_iconMargin.width();
            }
        }

        return QSize(width, iconSize);
    }

    return QStyledItemDelegate::sizeHint(option, index);
}

