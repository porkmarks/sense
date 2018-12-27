#pragma once

#include <memory>
#include <vector>
#include <QStyledItemDelegate>
#include <QAbstractItemModel>

class MeasurementsDelegate : public QStyledItemDelegate
{
public:
    MeasurementsDelegate(QAbstractItemModel& model);
    ~MeasurementsDelegate();

private:
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

    QAbstractItemModel& m_model;
};
