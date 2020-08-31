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
    if (m_textLabel == item || m_arrow == item || m_closeBox == item)
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

bool PlotToolTip::mouseMoved(QCPGraph* graph, const QPointF& pos)
{
	if (pos.x() >= m_textLabel->left->pixelPosition().x() &&
		pos.x() <= m_textLabel->right->pixelPosition().x() &&
		pos.y() >= m_textLabel->top->pixelPosition().y() &&
		pos.y() <= m_textLabel->bottom->pixelPosition().y())
	{
		if (m_draggedItem != nullptr)
		{
			QPointF delta = pos - m_lastDragPosition;
			applyTextLabelPositionDelta(graph, delta);
			m_lastDragPosition = pos;
		}

		m_closeBox->setVisible(m_isVisible);
		return true;
	}
	m_closeBox->setVisible(false);
	return false;
}

void PlotToolTip::mouseReleased(QCPGraph* graph, const QPointF& pos)
{
    Q_ASSERT(m_draggedItem != nullptr);
    QPointF delta = pos - m_lastDragPosition;
    applyTextLabelPositionDelta(graph, delta);
    m_lastDragPosition = pos;

    QCPAbstractItem* draggedItem = m_draggedItem;
    m_draggedItem = nullptr;

    if (m_isFixed && draggedItem == m_closeBox)
    {
        QPointF d = m_lastDragPosition - m_firstDragPosition;
        if (std::abs(d.x()) < 20 && std::abs(d.y()) < 20)
        {
            m_isOpen = false;
            emit closeMe();
        }
    }
}

void PlotToolTip::applyTextLabelPositionDelta(QCPGraph* graph, const QPointF& delta)
{
    m_textLabelDelta += delta;
	double x, y;
	graph->coordsToPixels(m_key, m_value, x, y);
    setTextLabelPosition(graph, QPointF(x, y) + m_textLabelDelta);
}

bool PlotToolTip::isOpen() const
{
    return m_isOpen;
}

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

void PlotToolTip::refresh(QCPGraph* graph)
{
    if (graph)
    {
        if (!graph->keyAxis()->range().contains(m_key))
        {
			m_isOpen = false;
            emit closeMe();
            return;
        }

        double x, y;
        graph->coordsToPixels(m_key, m_value, x, y);

        refreshBestArrowAnchor(QPointF(x, y));
        updateGeometry(graph);
        setTextLabelPosition(graph, QPointF(x, y) + m_textLabelDelta);
    }
    refreshCloseBox();
}

void PlotToolTip::setFixed(bool fixed)
{
    m_isFixed = fixed;
    refreshCloseBox();
}

void PlotToolTip::setText(QCPGraph* graph, const QString& text)
{
    m_text = text;
    updateGeometry(graph);

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
    //m_graph = graph;

    updateGeometry(graph);
}

void PlotToolTip::updateGeometry(QCPGraph* graph)
{
    if (!m_textLabel)
    {
        m_textLabel = new QCPItemText(m_plot);
        m_textLabel->setLayer(m_layer);
        m_textLabel->setPositionAlignment(Qt::AlignTop|Qt::AlignHCenter);
        m_textLabel->position->setType(QCPItemPosition::ptAbsolute);
        m_textLabel->setPen(QPen(Qt::black)); // show black border around text
        m_textLabel->setBrush(QBrush(Qt::white));
        m_textLabel->setClipToAxisRect(false);
        m_textLabelDelta = QPointF(150, -150);
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

	if (!graph)
	{
		return;
	}
	if (!graph->keyAxis()->range().contains(m_key))
	{
		m_isOpen = false;
		emit closeMe();
		return;
	}

	double x, y;
	graph->coordsToPixels(m_key, m_value, x, y);

    //m_arrow->end->setAxes(graph->keyAxis(), graph->valueAxis());
    //m_arrow->end->setCoords(m_key, m_value); // point to (4, 1.6) in x-y-plot coordinates
    m_arrow->end->setPixelPosition(QPointF(x, y));

    setTextLabelPosition(graph, QPointF(x, y) + m_textLabelDelta);
}

void PlotToolTip::setTextLabelPosition(QCPGraph* graph, const QPointF& pos)
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

    refreshCloseBox();

    {
        double x, y;
        graph->coordsToPixels(m_key, m_value, x, y);
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

    //m_closeBox->setVisible(m_isVisible);
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
