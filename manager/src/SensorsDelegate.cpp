#include "SensorsDelegate.h"
#include "SensorsModel.h"

#include <QWidget>
#include <QIcon>
#include <QPainter>
#include <QDateTime>
#include <QApplication>

#include <cassert>
#include <bitset>

constexpr QSize k_iconMargin(4, 2);
constexpr QSize k_miniPlotSize(64, 32);

//////////////////////////////////////////////////////////////////////////

SensorsDelegate::SensorsDelegate(QSortFilterProxyModel& sortingModel, SensorsModel& sensorsModel)
    : m_sortingModel(sortingModel)
    , m_sensorsModel(sensorsModel)
{
}

//////////////////////////////////////////////////////////////////////////

SensorsDelegate::~SensorsDelegate()
{
}

//////////////////////////////////////////////////////////////////////////

void SensorsDelegate::refreshMiniPlot(QImage& image, DB& db, DB::Sensor const& sensor, bool temperature) const
{
	image = QImage(k_miniPlotSize, QImage::Format_RGBA8888);
	QPainter p2;
	p2.begin(&image);
	p2.setRenderHints(QPainter::Antialiasing);

	image.fill(QColor(200, 200, 200, 100));

	DB::Filter filter;
	filter.useSensorFilter = true;
	filter.sensorIds.insert(sensor.id);
	filter.sortBy = DB::Filter::SortBy::Timestamp;
	filter.sortOrder = DB::Filter::SortOrder::Descending;
	std::vector<DB::Measurement> measurements = db.getFilteredMeasurements(filter, 0, size_t(k_miniPlotSize.width()));
	if (!measurements.empty())
	{
		std::sort(measurements.begin(), measurements.end(), [](DB::Measurement const& a, DB::Measurement const& b) { return a.timePoint < b.timePoint; });

		float minValue = std::numeric_limits<float>::max();
		float maxValue = std::numeric_limits<float>::lowest();
		for (DB::Measurement const& m : measurements)
		{
			minValue = std::min(minValue, temperature ? m.descriptor.temperature : m.descriptor.humidity);
			maxValue = std::max(maxValue, temperature ? m.descriptor.temperature : m.descriptor.humidity);
		}
		float center = (maxValue + minValue) / 2.f;
		float range = std::max(maxValue - minValue, 20.f);
		minValue = center - range / 2.f;
		maxValue = center + range / 2.f;

		int x = 0;
		p2.setPen(QPen(Qt::blue));
		std::vector<QPointF> points;
		points.reserve(measurements.size());
		for (DB::Measurement const& m : measurements)
		{
			float nvalue = 1.f - ((temperature ? m.descriptor.temperature : m.descriptor.humidity) - minValue) / range;
			QPointF p(float(x), nvalue * k_miniPlotSize.height());
			points.push_back(p);
			x++;
		}
		p2.drawPolyline(points.data(), int(points.size()));
	}
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
        int alarmTriggers = m_sortingModel.data(index).toInt();

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
			if (alarmTriggers & DB::AlarmTrigger::SensorBlackout)
			{
				static QIcon icon(":/icons/ui/sensor-blackout.png");
				painter->drawPixmap(pos, icon.pixmap(option.rect.size() - k_iconMargin * 2));
				pos.setX(pos.x() + iconSize + k_iconMargin.width());
			}
			if (alarmTriggers & DB::AlarmTrigger::BaseStationDisconnected)
			{
				static QIcon icon(":/icons/ui/station-disconnected.png");
				painter->drawPixmap(pos, icon.pixmap(option.rect.size() - k_iconMargin * 2));
				pos.setX(pos.x() + iconSize + k_iconMargin.width());
			}
        }

        painter->restore();
        return;
    }
	else if (column == SensorsModel::Column::Temperature)
	{
		if (option.state & QStyle::State_Selected)
		{
			painter->fillRect(option.rect, option.palette.highlight());
		}
		else
		{
			painter->fillRect(option.rect, (index.row() & 0) ? option.palette.base() : option.palette.alternateBase());
		}

		int32_t sensorIndex = m_sensorsModel.getSensorIndex(m_sortingModel.mapToSource(index));
		if (sensorIndex >= 0)
		{
			DB& db = m_sensorsModel.getDB();
			DB::Sensor sensor = db.getSensor(size_t(sensorIndex));

			MiniPlot& miniPlot = m_miniPlots[sensor.id];
			if (IClock::rtNow() - miniPlot.temperatureLastRefreshedTimePoint > std::chrono::seconds(60))
			{
				miniPlot.temperatureLastRefreshedTimePoint = IClock::rtNow();
				refreshMiniPlot(miniPlot.temperatureImage, db, sensor, true);
			}

			painter->save();
			painter->setClipRect(option.rect);
			QSize size = sizeHint(option, index);
			QPoint pos = option.rect.topLeft() + QPoint(size.width(), (option.rect.height() - k_miniPlotSize.height()) / 2) - QPoint(k_miniPlotSize.width(), 0);
            painter->drawImage(pos, miniPlot.temperatureImage);
			painter->restore();
		}
		QStyledItemDelegate::paint(painter, option, index);
	}
	else if (column == SensorsModel::Column::Humidity)
	{
		if (option.state & QStyle::State_Selected)
		{
			painter->fillRect(option.rect, option.palette.highlight());
		}
		else
		{
			painter->fillRect(option.rect, (index.row() & 0) ? option.palette.base() : option.palette.alternateBase());
		}

        int32_t sensorIndex = m_sensorsModel.getSensorIndex(m_sortingModel.mapToSource(index));
        if (sensorIndex >= 0)
		{
			DB& db = m_sensorsModel.getDB();
			DB::Sensor sensor = db.getSensor(size_t(sensorIndex));

			MiniPlot& miniPlot = m_miniPlots[sensor.id];
			if (IClock::rtNow() - miniPlot.humidityLastRefreshedTimePoint > std::chrono::seconds(60))
			{
				miniPlot.humidityLastRefreshedTimePoint = IClock::rtNow();
				refreshMiniPlot(miniPlot.humidityImage, db, sensor, false);
			}

			painter->save();
			painter->setClipRect(option.rect);
			QSize size = sizeHint(option, index);
			QPoint pos = option.rect.topLeft() + QPoint(size.width(), (option.rect.height() - k_miniPlotSize.height()) / 2) - QPoint(k_miniPlotSize.width(), 0);
			painter->drawImage(pos, miniPlot.humidityImage);
			painter->restore();
		}
		QStyledItemDelegate::paint(painter, option, index);
	}

    return QStyledItemDelegate::paint(painter, option, index);
}

//////////////////////////////////////////////////////////////////////////

QSize SensorsDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QSize sizeHint = QStyledItemDelegate::sizeHint(option, index);

    if (!index.isValid())
    {
        return sizeHint;
    }

    SensorsModel::Column column = static_cast<SensorsModel::Column>(index.column());
    if (column == SensorsModel::Column::Alarms)
    {
        int alarmTriggers = m_sortingModel.data(index).toInt();

        int32_t width = 0;
        int32_t iconSize = option.rect.height();

        if (alarmTriggers == -1)
        {
            width += iconSize + k_iconMargin.width();
        }
        else
        {
            width += (int32_t)std::bitset<64>(alarmTriggers).count();
        }

        return QSize(width, iconSize);
    }
    else if (column == SensorsModel::Column::Temperature)
    {
        sizeHint += QSize(k_iconMargin.width() + k_miniPlotSize.width(), 0);
        sizeHint = sizeHint.expandedTo(k_miniPlotSize);
    }
	else if (column == SensorsModel::Column::Humidity)
	{
		sizeHint += QSize(k_iconMargin.width() + k_miniPlotSize.width(), 0);
        sizeHint = sizeHint.expandedTo(k_miniPlotSize);
	}

    return sizeHint;
}

