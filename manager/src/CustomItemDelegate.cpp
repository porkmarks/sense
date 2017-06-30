#include "CustomItemDelegate.h"

void CustomItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    bool painted = false;
    if (cb_paint)
    {
        painted = cb_paint(painter, option, index);
    }


    if (!painted)
    {
        QStyledItemDelegate::paint(painter, option, index);
    }
}

QSize CustomItemDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QSize size;
    if (cb_sizeHint)
    {
        size = cb_sizeHint(option, index);
    }

    if (size.isEmpty())
    {
        return QStyledItemDelegate::sizeHint(option, index);
    }
    else
    {
        return size;
    }
}
