#pragma once

#include <QtCharts>
#include <QChartGlobal>
#include <QGraphicsItem>
#include <QFont>
#include <chrono>

class QGraphicsSceneMouseEvent;

class PlotToolTip : public QObject, public QGraphicsItem
{
    Q_OBJECT
public:
    PlotToolTip(QChart* parent);

    void setFixed(bool fixed);
    void setText(const QString& text);
    void setAnchor(const QPointF& point, QAbstractSeries* series);
    void updateGeometry();

    QRectF boundingRect() const;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,QWidget* widget);

signals:
    void closeMe();

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent* event);
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event);

private:
    bool m_isFixed = false;
    std::chrono::high_resolution_clock::time_point m_animationStartTP;

    QString m_text;
    QRectF m_textRect;
    QRectF m_rect;
    QRectF m_closeRect;
    QPointF m_anchor;
    QAbstractSeries* m_series = nullptr;
    QFont m_font;
    QChart* m_chart = nullptr;
};
