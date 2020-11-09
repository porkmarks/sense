#pragma once

#include <memory>
#include <vector>
#include <QStyledItemDelegate>
#include <QAbstractItemModel>
#include <QSortFilterProxyModel>
#include "DB.h"

class SensorsModel;

class SensorsDelegate : public QStyledItemDelegate
{
public:
    SensorsDelegate(QSortFilterProxyModel& sortingModel, SensorsModel& sensorsModel);
    ~SensorsDelegate();

private:
    void refreshMiniPlot(QPixmap& temperature, QPixmap& humidity, DB& db, DB::Sensor const& sensor) const;
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

    QSortFilterProxyModel& m_sortingModel;
    SensorsModel& m_sensorsModel;

    struct MiniPlot
    {
        static constexpr IClock::duration k_refreshPeriod = std::chrono::seconds(60);

		IClock::time_point lastRefreshedTimePoint = IClock::rtNow() - k_refreshPeriod + std::chrono::seconds(1);
        QPixmap temperatureImage;
        QPixmap humidityImage;
    };

    mutable IClock::time_point m_lastMiniPlotRefreshedTimePoint = IClock::time_point(IClock::duration::zero());

    mutable std::map<DB::SensorId, MiniPlot> m_miniPlots;
};
