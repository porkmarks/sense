#include "PlotToolTip.h"

#include <QtGui/QFontMetrics>
#include <QtGui/QMouseEvent>

PlotToolTip::PlotToolTip(QCustomPlot* plot, QCPLayer* layer)
    : m_plot(plot)
    , m_layer(layer)
{
}

PlotToolTip::~PlotToolTip()
{
    m_plot->removeItem(m_textLabel);
    m_plot->removeItem(m_arrow);
    m_plot->removeItem(m_closeBox);
}

bool PlotToolTip::mousePressed(QCPAbstractItem* item, const QPointF& pos)
{
    if (m_textLabel == item ||
            m_arrow == item ||
            m_closeBox == item)
    {
        m_draggedItem = item;

        //refine the dragged item
        if (pos.x() >= m_closeBox->left->pixelPosition().x() &&
                pos.x() <= m_closeBox->right->pixelPosition().x() &&
                pos.y() >= m_closeBox->top->pixelPosition().y() &&
                pos.y() <= m_closeBox->bottom->pixelPosition().y())
        {
            m_draggedItem = m_closeBox;
        }

        m_lastDragPosition = pos;
        m_firstDragPosition = pos;
        return true;
    }
    return false;
}

void PlotToolTip::mouseMoved(const QPointF& pos)
{
    Q_ASSERT(m_draggedItem != nullptr);
    QPointF delta = pos - m_lastDragPosition;
    applyTextLabelPositionDelta(delta);
    m_lastDragPosition = pos;
}

void PlotToolTip::mouseReleased(const QPointF& pos)
{
    Q_ASSERT(m_draggedItem != nullptr);
    QPointF delta = pos - m_lastDragPosition;
    applyTextLabelPositionDelta(delta);
    m_lastDragPosition = pos;

    QCPAbstractItem* draggedItem = m_draggedItem;
    m_draggedItem = nullptr;

    if (m_isFixed && draggedItem == m_closeBox)
    {
        QPointF d = m_lastDragPosition - m_firstDragPosition;
        if (std::abs(d.x()) < 20 && std::abs(d.y()) < 20)
        {
            emit closeMe();
        }
    }
}

void PlotToolTip::applyTextLabelPositionDelta(const QPointF& delta)
{
    setTextLabelPosition(m_textLabelPosition + delta);
}

//void PlotToolTip::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
//{
//    Q_UNUSED(option)
//    Q_UNUSED(widget)
//    QPainterPath path;
//    path.setFillRule(Qt::WindingFill);
//    path.addRoundedRect(m_rect, 5, 5);

//    QPointF anchor = mapFromParent(m_chart->mapToPosition(m_anchor, m_series));
//    if (!m_rect.contains(anchor))
//    {
//        QPointF point1, point2;

//        // establish the position of the anchor point in relation to m_rect
//        bool above = anchor.y() <= m_rect.top();
//        bool aboveCenter = anchor.y() > m_rect.top() && anchor.y() <= m_rect.center().y();
//        bool belowCenter = anchor.y() > m_rect.center().y() && anchor.y() <= m_rect.bottom();
//        bool below = anchor.y() > m_rect.bottom();

//        bool onLeft = anchor.x() <= m_rect.left();
//        bool leftOfCenter = anchor.x() > m_rect.left() && anchor.x() <= m_rect.center().x();
//        bool rightOfCenter = anchor.x() > m_rect.center().x() && anchor.x() <= m_rect.right();
//        bool onRight = anchor.x() > m_rect.right();

//        // get the nearest m_rect corner.
//        qreal x = (onRight + rightOfCenter) * m_rect.width();
//        qreal y = (below + belowCenter) * m_rect.height();
//        bool cornerCase = (above && onLeft) || (above && onRight) || (below && onLeft) || (below && onRight);
//        bool vertical = qAbs(anchor.x() - x) > qAbs(anchor.y() - y);

//        qreal x1 = x + leftOfCenter * 10 - rightOfCenter * 20 + cornerCase * !vertical * (onLeft * 10 - onRight * 20);
//        qreal y1 = y + aboveCenter * 10 - belowCenter * 20 + cornerCase * vertical * (above * 10 - below * 20);;
//        point1.setX(x1);
//        point1.setY(y1);

//        qreal x2 = x + leftOfCenter * 20 - rightOfCenter * 10 + cornerCase * !vertical * (onLeft * 20 - onRight * 10);;
//        qreal y2 = y + aboveCenter * 20 - belowCenter * 10 + cornerCase * vertical * (above * 20 - below * 10);;
//        point2.setX(x2);
//        point2.setY(y2);

//        path.moveTo(point1);
//        path.lineTo(anchor);
//        path.lineTo(point2);
//        path = path.simplified();
//    }

//    if (m_isFixed)
//    {
//        path.addRect(m_closeRect);
//        path.moveTo(m_closeRect.bottomLeft() + QPointF(3, -3));
//        path.lineTo(m_closeRect.topRight() + QPointF(-3, 3));
//        path.moveTo(m_closeRect.topLeft() + QPointF(3, 3));
//        path.lineTo(m_closeRect.bottomRight() - QPointF(3, 3));
//    }

//    QPen pen = painter->pen();
//    pen.setWidthF(1.0);
//    painter->setPen(pen);

//    painter->setBrush(QColor(255, 255, 255));
//    painter->drawPath(path);

//    QTextDocument td;
//    td.setHtml(m_text);
//    td.drawContents(painter, m_textRect);
//    //painter->drawText(m_textRect, m_text);
//}

//void PlotToolTip::mousePressEvent(QGraphicsSceneMouseEvent* event)
//{
//    event->setAccepted(true);
//    if (m_isFixed && m_closeRect.contains(event->pos()))
//    {
//        emit closeMe();
//    }
//}

//void PlotToolTip::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
//{
//    if (event->buttons() & Qt::LeftButton)
//    {
//        setPos(mapToParent(event->pos() - event->buttonDownPos(Qt::LeftButton)));
//        event->setAccepted(true);
//    }
//    else
//    {
//        event->setAccepted(false);
//    }
//}

void PlotToolTip::setVisible(bool visible)
{
    m_isVisible = visible;
    if (m_textLabel)
    {
        m_textLabel->setVisible(m_isVisible);
    }
    if (m_arrow)
    {
        m_arrow->setVisible(m_isVisible);
    }
    if (m_closeBox)
    {
        m_closeBox->setVisible(m_isVisible);
    }
}

void PlotToolTip::refresh()
{
    if (m_graph)
    {
        double x, y;
        m_graph->coordsToPixels(m_key, m_value, x, y);
        refreshBestArrowAnchor(QPointF(x, y));
    }
    refreshCloseBox();
}

void PlotToolTip::setFixed(bool fixed)
{
    m_isFixed = fixed;
    refreshCloseBox();
}

void PlotToolTip::setText(const QString& text)
{
    m_text = text;
    updateGeometry();

    QTextDocument td;
    td.setHtml(m_text);
//    m_textRect = QRectF(QPointF(5, 5), td.size());

//    m_rect = m_textRect.adjusted(-5, -5, 5, 5);
}

void PlotToolTip::setAnchor(QCPGraph* graph, const QPointF point, double key, double value)
{
    //m_anchor = point;
    m_key = key;
    m_value = value;
    m_graph = graph;

    updateGeometry();
}

void PlotToolTip::updateGeometry()
{
    if (!m_graph)
    {
        return;
    }
    double x, y;
    m_graph->coordsToPixels(m_key, m_value, x, y);

    if (!m_textLabel)
    {
        m_textLabel = new QCPItemText(m_plot);
        m_textLabel->setLayer(m_layer);
        m_textLabel->setPositionAlignment(Qt::AlignTop|Qt::AlignHCenter);
        m_textLabel->position->setType(QCPItemPosition::ptAbsolute);
        m_textLabel->setPen(QPen(Qt::black)); // show black border around text
        m_textLabel->setBrush(QBrush(Qt::white));
        m_textLabel->setClipToAxisRect(false);
    }
    m_textLabel->setVisible(m_isVisible);
    m_textLabel->setText(m_text);

    // add the arrow:
    if (!m_arrow)
    {
        m_arrow = new QCPItemLine(m_plot);
        m_arrow->setLayer(m_layer);
        m_arrow->setHead(QCPLineEnding::esSpikeArrow);
        m_arrow->setClipToAxisRect(false);
    }
    m_arrow->setVisible(m_isVisible);
    m_arrow->end->setAxes(m_graph->keyAxis(), m_graph->valueAxis());
    m_arrow->end->setCoords(m_key, m_value); // point to (4, 1.6) in x-y-plot coordinates

    setTextLabelPosition(QPointF(x + 150, y - 150));
}

void PlotToolTip::setTextLabelPosition(const QPointF& pos)
{
    QRect rect = m_plot->axisRect()->rect();
    double px = pos.x();
    double py = pos.y();
    m_textLabel->position->setCoords(px, py);

    double coord = m_textLabel->left->pixelPosition().x();
    if (coord < rect.left())
    {
        px += rect.left() - coord;
    }
    coord = m_textLabel->right->pixelPosition().x();
    if (coord > rect.right())
    {
        px -= coord - rect.right();
    }
    coord = m_textLabel->top->pixelPosition().y();
    if (coord < rect.top())
    {
        py += rect.top() - coord;
    }
    coord = m_textLabel->bottom->pixelPosition().y();
    if (coord > rect.bottom())
    {
        py -= coord - rect.bottom();
    }

    m_textLabel->position->setCoords(px, py);
    m_textLabelPosition = QPointF(px, py);

    refreshCloseBox();

    {
        double x, y;
        m_graph->coordsToPixels(m_key, m_value, x, y);
        refreshBestArrowAnchor(QPointF(x, y));
    }
}

void PlotToolTip::refreshCloseBox()
{
    if (!m_isFixed)
    {
        delete m_closeBox;
        m_closeBox = nullptr;

        return;
    }

    if (!m_closeBox)
    {
        m_closeBox = new QCPItemText(m_plot);
        m_closeBox->setLayer(m_layer);
        m_closeBox->setPositionAlignment(Qt::AlignTop|Qt::AlignRight);
        m_closeBox->position->setType(QCPItemPosition::ptAbsolute);
        m_closeBox->setPen(QPen(Qt::black)); // show black border around text
        m_closeBox->setBrush(QBrush(Qt::white));
        m_closeBox->setClipToAxisRect(false);
        m_closeBox->setText("x");
    }

    m_closeBox->setVisible(m_isVisible);
    m_closeBox->position->setCoords(m_textLabel->topRight->pixelPosition());// + QPointF(-width, height));
}

void PlotToolTip::refreshBestArrowAnchor(const QPointF& point)
{
    int bestIndex = 0;
    double bestdistance = std::numeric_limits<double>::max();

    QList<QCPItemAnchor*> anchors = m_textLabel->anchors();
    for (int i = 0; i < anchors.size(); i++)
    {
        double dx = point.x() - anchors[i]->pixelPosition().x();
        double dy = point.y() - anchors[i]->pixelPosition().y();
        double distance = std::sqrt(dx*dx + dy*dy);
        if (distance < bestdistance)
        {
            bestdistance = distance;
            bestIndex = i;
        }
    }

    m_arrow->start->setParentAnchor(anchors[bestIndex]);
}
