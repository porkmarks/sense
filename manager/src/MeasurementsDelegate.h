#pragma once

#include <memory>
#include <vector>
#include <QStyledItemDelegate>
#include <QSortFilterProxyModel>

#include "MeasurementsModel.h"

class MeasurementsDelegate : public QStyledItemDelegate
{
public:
    MeasurementsDelegate(MeasurementsModel& sourceModel, QSortFilterProxyModel& proxyModel);
    ~MeasurementsDelegate();

private:
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

    MeasurementsModel& m_sourceModel;
    QSortFilterProxyModel& m_proxyModel;
};
