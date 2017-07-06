#pragma once

#include <QtCharts>
#include <QChartGlobal>
#include <QGraphicsItem>
#include <QFont>

class QGraphicsSceneMouseEvent;

class PlotToolTip : public QGraphicsItem
{
public:
    PlotToolTip(QChart* parent);

    void setText(const QString& text);
    void setAnchor(const QPointF& point, QAbstractSeries* series);
    void updateGeometry();

    QRectF boundingRect() const;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,QWidget* widget);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent* event);
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event);

private:
    QString m_text;
    QRectF m_textRect;
    QRectF m_rect;
    QPointF m_anchor;
    QAbstractSeries* m_series = nullptr;
    QFont m_font;
    QChart* m_chart = nullptr;
};
