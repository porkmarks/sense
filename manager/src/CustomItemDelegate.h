#pragma once

#include <QStyledItemDelegate>
#include <functional>

class CustomItemDelegate : public QStyledItemDelegate
{
public:
    std::function<bool(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index)> cb_paint;
    std::function<QSize(const QStyleOptionViewItem& option, const QModelIndex& index)> cb_sizeHint;

private:
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

};

