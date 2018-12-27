#pragma once

#include "qcustomplot.h"
#include <QObject>
#include <QFont>
#include <chrono>

class PlotToolTip : public QObject
{
    Q_OBJECT
public:
    PlotToolTip(QCustomPlot* plot, QCPLayer* layer);
    ~PlotToolTip();

    bool mousePressed(QCPAbstractItem* item, const QPointF& pos);
    void mouseMoved(const QPointF& pos);
    void mouseReleased(const QPointF& pos);

    void setFixed(bool fixed);
    void setText(const QString& text);
    void setAnchor(QCPGraph* graph, const QPointF point, double key, double value);
    void setVisible(bool visible);

    void refresh();

//    QRectF boundingRect() const;
//    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,QWidget* widget);

signals:
    void closeMe();

private:
    void updateGeometry();
    void refreshBestArrowAnchor(const QPointF& point);
    void refreshCloseBox();
    void applyTextLabelPositionDelta(const QPointF& delta);
    void setTextLabelPosition(const QPointF& pos);

    QCustomPlot* m_plot = nullptr;
    QCPLayer* m_layer = nullptr;

    bool m_isFixed = false;

    QString m_text;
    QPointF m_textLabelPosition;
    double m_key = 0;
    double m_value = 0;
    QCPGraph* m_graph = nullptr;
    QCPItemText* m_textLabel = nullptr;
    QCPItemLine* m_arrow = nullptr;
    QCPItemText* m_closeBox = nullptr;
    QFont m_font;
    bool m_isVisible = true;

    QCPAbstractItem* m_draggedItem = nullptr;
    QPointF m_firstDragPosition;
    QPointF m_lastDragPosition;
};
