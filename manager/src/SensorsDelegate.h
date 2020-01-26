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
    void refreshMiniPlot(QImage& image, DB& db, DB::Sensor const& sensor, bool temperature) const;
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

    QSortFilterProxyModel& m_sortingModel;
    SensorsModel& m_sensorsModel;

    struct MiniPlot
    {
		QImage temperatureImage;
        IClock::time_point temperatureLastRefreshedTimePoint = IClock::time_point(IClock::duration::zero());

        QImage humidityImage;
        IClock::time_point humidityLastRefreshedTimePoint = IClock::time_point(IClock::duration::zero());
    };

    mutable std::map<DB::SensorId, MiniPlot> m_miniPlots;
};
