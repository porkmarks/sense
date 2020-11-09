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

void SensorsDelegate::refreshMiniPlot(QPixmap& temperature, QPixmap& humidity, DB& db, DB::Sensor const& sensor) const
{
	DB::Filter filter;
	filter.useSensorFilter = true;
	filter.sensorIds.insert(sensor.id);
	filter.sortBy = DB::Filter::SortBy::Timestamp;
	filter.sortOrder = DB::Filter::SortOrder::Descending;
	std::vector<DB::Measurement> measurements = db.getFilteredMeasurements(filter, 0, size_t(k_miniPlotSize.width()));
	if (!measurements.empty())
	{
		std::sort(measurements.begin(), measurements.end(), [](DB::Measurement const& a, DB::Measurement const& b) { return a.timePoint < b.timePoint; });

		{
			temperature = QPixmap(k_miniPlotSize);
			temperature.fill(QColor(200, 200, 200, 100));

			QPainter p2;
			p2.begin(&temperature);
			p2.setRenderHints(QPainter::Antialiasing);

			float minValue = std::numeric_limits<float>::max();
			float maxValue = std::numeric_limits<float>::lowest();
			for (DB::Measurement const& m : measurements)
			{
				minValue = std::min(minValue, m.descriptor.temperature);
				maxValue = std::max(maxValue, m.descriptor.temperature);
			}
			float center = (maxValue + minValue) / 2.f;
			float range = std::max(maxValue - minValue, 20.f);
			minValue = center - range / 2.f;
			maxValue = center + range / 2.f;

			Q_ASSERT(measurements.size() <= (k_miniPlotSize.width()));
			size_t x = size_t(k_miniPlotSize.width()) - measurements.size(); //right align
			p2.setPen(QPen(QColor(211, 47, 47)));
			std::vector<QPointF> points;
			points.reserve(measurements.size());
			for (DB::Measurement const& m : measurements)
			{
				float nvalue = 1.f - (m.descriptor.temperature - minValue) / range;
				QPointF p(float(x), nvalue * k_miniPlotSize.height());
				points.push_back(p);
				x++;
			}
			p2.drawPolyline(points.data(), int(points.size()));
		}
		{
			humidity = QPixmap(k_miniPlotSize);
			humidity.fill(QColor(200, 200, 200, 100));

			QPainter p2;
			p2.begin(&humidity);
			p2.setRenderHints(QPainter::Antialiasing);

			float minValue = std::numeric_limits<float>::max();
			float maxValue = std::numeric_limits<float>::lowest();
			for (DB::Measurement const& m : measurements)
			{
				minValue = std::min(minValue, m.descriptor.humidity);
				maxValue = std::max(maxValue, m.descriptor.humidity);
			}
			float center = (maxValue + minValue) / 2.f;
			float range = std::max(maxValue - minValue, 20.f);
			minValue = center - range / 2.f;
			maxValue = center + range / 2.f;

			Q_ASSERT(measurements.size() <= (k_miniPlotSize.width()));
			size_t x = size_t(k_miniPlotSize.width()) - measurements.size(); //right align
			p2.setPen(QPen(QColor(39, 97, 123)));
			std::vector<QPointF> points;
			points.reserve(measurements.size());
			for (DB::Measurement const& m : measurements)
			{
				float nvalue = 1.f - (m.descriptor.humidity - minValue) / range;
				QPointF p(float(x), nvalue * k_miniPlotSize.height());
				points.push_back(p);
				x++;
			}
			p2.drawPolyline(points.data(), int(points.size()));
		}
	}
}

//////////////////////////////////////////////////////////////////////////

void SensorsDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    if (!index.isValid())
        return QStyledItemDelegate::paint(painter, option, index);

    SensorsModel::Column column = static_cast<SensorsModel::Column>(index.column());

	if (column == SensorsModel::Column::Temperature || column == SensorsModel::Column::Humidity)
	{
		int32_t sensorIndex = m_sensorsModel.getSensorIndex(m_sortingModel.mapToSource(index));
		if (sensorIndex >= 0)
		{
			DB& db = m_sensorsModel.getDB();
			DB::Sensor sensor = db.getSensor(size_t(sensorIndex));
			MiniPlot& miniPlot = m_miniPlots[sensor.id];
			if (IClock::rtNow() - miniPlot.lastRefreshedTimePoint > MiniPlot::k_refreshPeriod && IClock::rtNow() - m_lastMiniPlotRefreshedTimePoint > std::chrono::seconds(1))
			{
				miniPlot.lastRefreshedTimePoint = IClock::rtNow();
				m_lastMiniPlotRefreshedTimePoint = IClock::rtNow();
				refreshMiniPlot(miniPlot.temperatureImage, miniPlot.humidityImage, db, sensor);
			}
		}
	}

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
		painter->save();
		QStyledItemDelegate::paint(painter, option, index);
		painter->restore();

		int32_t sensorIndex = m_sensorsModel.getSensorIndex(m_sortingModel.mapToSource(index));
		if (sensorIndex >= 0)
		{
			DB& db = m_sensorsModel.getDB();
			DB::Sensor sensor = db.getSensor(size_t(sensorIndex));
			MiniPlot& miniPlot = m_miniPlots[sensor.id];
			painter->save();
			painter->setClipRect(option.rect);
			QSize size = sizeHint(option, index);
			QPoint pos = option.rect.topLeft() + QPoint(size.width(), (option.rect.height() - k_miniPlotSize.height()) / 2) - QPoint(k_miniPlotSize.width(), 0);
            //painter->drawImage(pos, miniPlot.temperatureImage);
			painter->drawPixmap(pos, miniPlot.temperatureImage);
			painter->restore();
		}
		return;
	}
	else if (column == SensorsModel::Column::Humidity)
	{
		painter->save();
		QStyledItemDelegate::paint(painter, option, index);
		painter->restore();

        int32_t sensorIndex = m_sensorsModel.getSensorIndex(m_sortingModel.mapToSource(index));
        if (sensorIndex >= 0)
		{
			DB& db = m_sensorsModel.getDB();
			DB::Sensor sensor = db.getSensor(size_t(sensorIndex));
			MiniPlot& miniPlot = m_miniPlots[sensor.id];
			painter->save();
			painter->setClipRect(option.rect);
			QSize size = sizeHint(option, index);
			QPoint pos = option.rect.topLeft() + QPoint(size.width(), (option.rect.height() - k_miniPlotSize.height()) / 2) - QPoint(k_miniPlotSize.width(), 0);
			//painter->drawImage(pos, miniPlot.humidityImage);
			painter->drawPixmap(pos, miniPlot.humidityImage);
			painter->restore();
		}
		return;
	}

    return QStyledItemDelegate::paint(painter, option, index);
}

//////////////////////////////////////////////////////////////////////////

QSize SensorsDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QSize sizeHint = QStyledItemDelegate::sizeHint(option, index);

    if (!index.isValid())
        return sizeHint;

    SensorsModel::Column column = static_cast<SensorsModel::Column>(index.column());
    if (column == SensorsModel::Column::Alarms)
    {
        int alarmTriggers = m_sortingModel.data(index).toInt();

        int32_t width = 0;
        int32_t iconSize = option.rect.height();

        if (alarmTriggers == -1)
            width += iconSize + k_iconMargin.width();
        else
            width += (int32_t)std::bitset<64>(alarmTriggers).count();

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

