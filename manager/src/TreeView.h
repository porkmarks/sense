#pragma once

#include "QTreeView"
#include "QMouseEvent"

class TreeView : public QTreeView
{
public:
    TreeView(QWidget *parent) : QTreeView(parent) {}
    ~TreeView() override = default;

private:
    void mousePressEvent(QMouseEvent *event) override
    {
        QModelIndex item = indexAt(event->pos());
        bool selected = selectionModel()->isSelected(indexAt(event->pos()));
        QTreeView::mousePressEvent(event);
        if ((item.row() == -1 && item.column() == -1) || selected)
        {
            clearSelection();
            const QModelIndex index;
            selectionModel()->setCurrentIndex(index, QItemSelectionModel::Select);
        }
    }
};
