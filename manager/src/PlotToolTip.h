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
    bool mouseMoved(QCPGraph* graph, const QPointF& pos);
    void mouseReleased(QCPGraph* graph, const QPointF& pos);

    void setFixed(bool fixed);
    void setText(QCPGraph* graph, const QString& text);
    void setAnchor(QCPGraph* graph, const QPointF point, double key, double value);
    void setVisible(bool visible);

    void refresh(QCPGraph* graph);

    bool isOpen() const;

signals:
    void closeMe();

private:
    void updateGeometry(QCPGraph* graph);
    void refreshBestArrowAnchor(const QPointF& point);
    void refreshCloseBox();
    void applyTextLabelPositionDelta(QCPGraph* graph, const QPointF& delta);
    void setTextLabelPosition(QCPGraph* graph, const QPointF& pos);

    QCustomPlot* m_plot = nullptr;
    QCPLayer* m_layer = nullptr;

    bool m_isFixed = false;
    bool m_isOpen = true;

    QString m_text;
    QPointF m_textLabelDelta;
    double m_key = 0;
    double m_value = 0;
    //QCPGraph* m_graph = nullptr;
    QCPItemText* m_textLabel = nullptr;
    QCPItemLine* m_arrow = nullptr;
    QCPItemText* m_closeBox = nullptr;
    QFont m_font;
    bool m_isVisible = true;

    QCPAbstractItem* m_draggedItem = nullptr;
    QPointF m_firstDragPosition;
    QPointF m_lastDragPosition;
};
