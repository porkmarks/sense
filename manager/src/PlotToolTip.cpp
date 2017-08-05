#include "PlotToolTip.h"

#include <QtGui/QPainter>
#include <QtGui/QFontMetrics>
#include <QtWidgets/QGraphicsSceneMouseEvent>
#include <QtGui/QMouseEvent>
#include <QtCharts/QChart>

PlotToolTip::PlotToolTip(QChart* chart)
    : QGraphicsItem(chart)
    , m_chart(chart)
{
}

QRectF PlotToolTip::boundingRect() const
{
    QPointF anchor = mapFromParent(m_chart->mapToPosition(m_anchor, m_series));
    QRectF rect;
    rect.setLeft(qMin(m_rect.left(), anchor.x()));
    rect.setRight(qMax(m_rect.right(), anchor.x()));
    rect.setTop(qMin(m_rect.top(), anchor.y()));
    rect.setBottom(qMax(m_rect.bottom(), anchor.y()));
    return rect;
}

void PlotToolTip::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    Q_UNUSED(option)
    Q_UNUSED(widget)
    QPainterPath path;
    path.setFillRule(Qt::WindingFill);
    path.addRoundedRect(m_rect, 5, 5);

    QPointF anchor = mapFromParent(m_chart->mapToPosition(m_anchor, m_series));
    if (!m_rect.contains(anchor))
    {
        QPointF point1, point2;

        // establish the position of the anchor point in relation to m_rect
        bool above = anchor.y() <= m_rect.top();
        bool aboveCenter = anchor.y() > m_rect.top() && anchor.y() <= m_rect.center().y();
        bool belowCenter = anchor.y() > m_rect.center().y() && anchor.y() <= m_rect.bottom();
        bool below = anchor.y() > m_rect.bottom();

        bool onLeft = anchor.x() <= m_rect.left();
        bool leftOfCenter = anchor.x() > m_rect.left() && anchor.x() <= m_rect.center().x();
        bool rightOfCenter = anchor.x() > m_rect.center().x() && anchor.x() <= m_rect.right();
        bool onRight = anchor.x() > m_rect.right();

        // get the nearest m_rect corner.
        qreal x = (onRight + rightOfCenter) * m_rect.width();
        qreal y = (below + belowCenter) * m_rect.height();
        bool cornerCase = (above && onLeft) || (above && onRight) || (below && onLeft) || (below && onRight);
        bool vertical = qAbs(anchor.x() - x) > qAbs(anchor.y() - y);

        qreal x1 = x + leftOfCenter * 10 - rightOfCenter * 20 + cornerCase * !vertical * (onLeft * 10 - onRight * 20);
        qreal y1 = y + aboveCenter * 10 - belowCenter * 20 + cornerCase * vertical * (above * 10 - below * 20);;
        point1.setX(x1);
        point1.setY(y1);

        qreal x2 = x + leftOfCenter * 20 - rightOfCenter * 10 + cornerCase * !vertical * (onLeft * 20 - onRight * 10);;
        qreal y2 = y + aboveCenter * 20 - belowCenter * 10 + cornerCase * vertical * (above * 20 - below * 10);;
        point2.setX(x2);
        point2.setY(y2);

        path.moveTo(point1);
        path.lineTo(anchor);
        path.lineTo(point2);
        path = path.simplified();
    }

    if (m_isFixed)
    {
        path.addRect(m_closeRect);
        path.moveTo(m_closeRect.bottomLeft() + QPointF(3, -3));
        path.lineTo(m_closeRect.topRight() + QPointF(-3, 3));
        path.moveTo(m_closeRect.topLeft() + QPointF(3, 3));
        path.lineTo(m_closeRect.bottomRight() - QPointF(3, 3));
    }

    if (m_isFixed)
    {
        constexpr float animationDurationS = 0.5f;
        float d = std::chrono::duration<float>(std::chrono::high_resolution_clock::now() - m_animationStartTP).count();
        if (d < animationDurationS)
        {
            float mu = d / animationDurationS;
            float x = std::abs(std::sin(mu * M_PI));
            QPen pen = painter->pen();
            pen.setWidthF(1.f + x * 5.f);
            painter->setPen(pen);
            update();
        }
        else
        {
            QPen pen = painter->pen();
            pen.setWidthF(1.f);
            painter->setPen(pen);
        }
    }
    else
    {
        QPen pen = painter->pen();
        pen.setWidthF(1.f);
        painter->setPen(pen);
    }


    painter->setBrush(QColor(255, 255, 255));
    painter->drawPath(path);

    QTextDocument td;
    td.setHtml(m_text);
    td.drawContents(painter, m_textRect);
    //painter->drawText(m_textRect, m_text);
}

void PlotToolTip::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    event->setAccepted(true);
    if (m_isFixed && m_closeRect.contains(event->pos()))
    {
        emit closeMe();
    }
}

void PlotToolTip::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    if (event->buttons() & Qt::LeftButton)
    {
        setPos(mapToParent(event->pos() - event->buttonDownPos(Qt::LeftButton)));
        event->setAccepted(true);
    }
    else
    {
        event->setAccepted(false);
    }
}

void PlotToolTip::setFixed(bool fixed)
{
    m_isFixed = fixed;
    m_animationStartTP = std::chrono::high_resolution_clock::now();
    update();
}

void PlotToolTip::setText(const QString& text)
{
    m_text = text;

    QTextDocument td;
    td.setHtml(m_text);
    m_textRect = QRectF(QPointF(5, 5), td.size());

//    QFontMetrics metrics(m_font);
//    m_textRect = metrics.boundingRect(QRect(0, 0, 150, 150), Qt::AlignLeft, m_text);
//    m_textRect.translate(5, 5);
    prepareGeometryChange();
    m_rect = m_textRect.adjusted(-5, -5, 5, 5);

    {
        float length = 14;
        float margin = 6;
        m_closeRect = QRectF(m_rect.topRight() + QPointF(-(length + margin), margin), QSizeF(length, length));
    }
}

void PlotToolTip::setAnchor(const QPointF& point, QAbstractSeries* series)
{
    m_anchor = point;
    m_series = series;
}

void PlotToolTip::updateGeometry()
{
    prepareGeometryChange();
    setPos(m_chart->mapToPosition(m_anchor, m_series) + QPoint(10, -50));
}
