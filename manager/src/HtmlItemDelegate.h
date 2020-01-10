#pragma once

#include <memory>
#include <vector>
#include <QStyledItemDelegate>
#include <QAbstractItemModel>

class HtmlItemDelegate : public QStyledItemDelegate
{
public:
	HtmlItemDelegate(QObject* parent = nullptr);
	~HtmlItemDelegate();

private:
	void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
	QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
};
