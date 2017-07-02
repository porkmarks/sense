#pragma once

#include <memory>
#include <vector>
#include <QStyledItemDelegate>
#include <QAbstractItemModel>

class SensorsDelegate : public QStyledItemDelegate
{
public:
    SensorsDelegate(QAbstractItemModel& model);
    ~SensorsDelegate();

private:
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

    QAbstractItemModel& m_model;
};
