#include "PlotWidget.h"


PlotWidget::PlotWidget(QWidget* parent)
    : QWidget(parent)
{
    m_ui.setupUi(this);

    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &QChartView::customContextMenuRequested, this, &PlotWidget::plotContextMenu);

    createPlotWidgets();
}

void PlotWidget::init(DB& db, MeasurementsModel& model)
{
    m_db = &db;
    m_model = &model;
}

void PlotWidget::createPlotWidgets()
{
    m_tooltip.reset();
    m_tooltips.clear();

    delete m_chart;
    delete m_chartView;

    if (m_ui.plot->layout() != nullptr)
    {
        QLayoutItem* item;
        while ((item = m_ui.plot->layout()->takeAt(0)) != nullptr)
        {
            delete item->widget();
            delete item;
        }
    }


    QFont font;
    font.setPointSize(8);

    {
        QChart* chart = new QChart();
        chart->legend()->setAlignment(Qt::AlignRight);

        m_chart = chart;

        QDateTimeAxis* axisX = new QDateTimeAxis(chart);
        axisX->setLabelsFont(font);
        axisX->setTitleFont(font);
        axisX->setTickCount(10);
        axisX->setFormat("MM yy h:mm");
        axisX->setTitleText("Date");
        chart->addAxis(axisX, Qt::AlignBottom);
        //m_series->attachAxis(axisX);

        QValueAxis* axisTY = new QValueAxis(chart);
        axisTY->setLabelsFont(font);
        axisTY->setTitleFont(font);
        axisTY->setLabelFormat("%.1f");
        axisTY->setTitleText(u8"°C");
        axisTY->setTickCount(10);
        chart->addAxis(axisTY, Qt::AlignLeft);

        QValueAxis* axisHY = new QValueAxis(chart);
        axisHY->setLabelsFont(font);
        axisHY->setTitleFont(font);
        axisHY->setLabelFormat("%.1f");
        axisHY->setTitleText("%RH");
        axisHY->setTickCount(10);
        chart->addAxis(axisHY, Qt::AlignRight);

        m_axisX = axisX;
        m_axisTY = axisTY;
        m_axisHY = axisHY;

        QChartView* chartView = new QChartView(chart);
        chartView->setRenderHint(QPainter::Antialiasing);
        m_chartView = chartView;
    }

    //m_chartView->setMaximumHeight(m_ui.plot->height() - 100);
    m_ui.plot->layout()->addWidget(m_chartView);
    //connect(m_chartView, &QChartView::customContextMenuRequested, this, &PlotWidget::plotContextMenu);
    //m_chartView->setMaximumHeight(999999);
}

void PlotWidget::refresh()
{
    foreach (QLegendMarker* marker, m_chart->legend()->markers())
    {
        QObject::disconnect(marker, SIGNAL(clicked()), this, SLOT(handleMarkerClicked()));
    }

    createPlotWidgets();

    m_series.clear();

    uint64_t minTS = std::numeric_limits<uint64_t>::max();
    uint64_t maxTS = std::numeric_limits<uint64_t>::lowest();
    qreal minT = std::numeric_limits<qreal>::max();
    qreal maxT = std::numeric_limits<qreal>::lowest();
    qreal minH = std::numeric_limits<qreal>::max();
    qreal maxH = std::numeric_limits<qreal>::lowest();
    for (size_t i = 0; i < m_model->getMeasurementCount(); i++)
    {
        DB::Measurement const& m = m_model->getMeasurement(i);
        int32_t sensorIndex = m_db->findSensorIndexById(m.descriptor.sensorId);
        if (sensorIndex < 0)
        {
            continue;
        }
        //        if (i & 1)
        //        {
        //            continue;
        //        }

        qreal millis = std::chrono::duration_cast<std::chrono::milliseconds>(m.descriptor.timePoint.time_since_epoch()).count();
        time_t tt = DB::Clock::to_time_t(m.descriptor.timePoint);
        minTS = std::min<uint64_t>(minTS, tt);
        maxTS = std::max<uint64_t>(maxTS, tt);

        PlotData* plotData = nullptr;
        auto it = m_series.find(m.descriptor.sensorId);
        if (it == m_series.end())
        {
            plotData = &m_series[m.descriptor.sensorId];
            plotData->temperatureLpf.setup(1, 1, 0.15);
            plotData->humidityLpf.setup(1, 1, 0.15);
        }
        else
        {
            plotData = &it->second;
        }

        plotData->sensor = m_db->getSensor(sensorIndex);

        qreal value = m.descriptor.temperature;
        if (m_useFiltering)
        {
            plotData->temperatureLpf.process(value);
        }
        plotData->temperaturePoints.append(QPointF(millis, value));
        minT = std::min(minT, value);
        maxT = std::max(maxT, value);

        value = m.descriptor.humidity;
        if (m_useFiltering)
        {
            plotData->humidityLpf.process(value);
        }
        plotData->humidityPoints.append(QPointF(millis, value));
        minH = std::min(minH, value);
        maxH = std::max(maxH, value);
    }

    for (auto& pair : m_series)
    {
        PlotData& plotData = pair.second;
        {
            QLineSeries* series = new QLineSeries(m_chart);
            m_chart->addSeries(series);
            QPen pen = series->pen();
            //pen.setWidth(4);
            series->setPen(pen);
            series->setName(QString("%1 °C").arg(plotData.sensor.descriptor.name.c_str()));
            series->attachAxis(m_axisX);
            series->attachAxis(m_axisTY);

            connect(series, SIGNAL(clicked(QPointF)), this, SLOT(keepTooltip()));
            connect(series, &QLineSeries::hovered, [this, series, &plotData](QPointF const& point, bool state)
            {
                tooltip(series, point, state, plotData.sensor, true);
            });

            QList<QPointF> const& points = plotData.temperaturePoints;
            if (points.size() < 400)
            {
                series->setPointsVisible();
            }
            series->append(points);
            plotData.temperatureSeries.reset(series);
        }
        {
            QLineSeries* series = new QLineSeries(m_chart);
            m_chart->addSeries(series);
            QPen pen = series->pen();
            //pen.setWidth(2);
            series->setPen(pen);
            series->setName(QString("%1 %RH").arg(plotData.sensor.descriptor.name.c_str()));
            series->attachAxis(m_axisX);
            series->attachAxis(m_axisHY);

            connect(series, SIGNAL(clicked(QPointF)), this, SLOT(keepTooltip()));
            connect(series, &QLineSeries::hovered, [this, series, &plotData](QPointF const& point, bool state)
            {
                tooltip(series, point, state, plotData.sensor, false);
            });

            QList<QPointF> const& points = plotData.humidityPoints;
            if (points.size() < 400)
            {
                series->setPointsVisible();
            }
            series->append(points);
            plotData.humiditySeries.reset(series);
        }
    }

    foreach (QLegendMarker* marker, m_chart->legend()->markers())
    {
        // Disconnect possible existing connection to avoid multiple connections
        QObject::disconnect(marker, SIGNAL(clicked()), this, SLOT(handleMarkerClicked()));
        QObject::connect(marker, SIGNAL(clicked()), this, SLOT(handleMarkerClicked()));
    }

    QDateTime minDT, maxDT;
    minDT.setTime_t(minTS);
    maxDT.setTime_t(maxTS);
    m_axisX->setRange(minDT, maxDT);

    constexpr qreal minTemperatureRange = 5.0;
    if (std::abs(maxT - minT) < minTemperatureRange)
    {
        qreal center = (maxT + minT) / 2.0;
        minT = center - minTemperatureRange / 2.0;
        maxT = center + minTemperatureRange / 2.0;
    }
    m_axisTY->setRange(minT, maxT);

    constexpr qreal minHumidityRange = 10.0;
    if (std::abs(maxH - minH) < minHumidityRange)
    {
        qreal center = (maxH + minH) / 2.0;
        minH = center - minHumidityRange / 2.0;
        maxH = center + minHumidityRange / 2.0;
    }
    m_axisHY->setRange(minH, maxH);
}

void PlotWidget::resizeEvent(QResizeEvent *event)
{
    for (std::unique_ptr<PlotToolTip>& p: m_tooltips)
    {
        p->updateGeometry();
    }
    QWidget::resizeEvent(event);
}

void PlotWidget::keepTooltip()
{
    if (m_tooltip != nullptr)
    {
        m_tooltip->setFixed(true);
        m_tooltips.push_back(std::move(m_tooltip));
        m_tooltip.reset(new PlotToolTip(m_chart));
    }
}

void PlotWidget::tooltip(QLineSeries* series, QPointF point, bool state, DB::Sensor const& sensor, bool temperature)
{
    if (m_tooltip == nullptr)
    {
        m_tooltip.reset(new PlotToolTip(m_chart));
    }

    if (state)
    {
        QColor color = series->color();
        QDateTime dt;
        dt.setMSecsSinceEpoch(point.x());
        if (temperature)
        {
            m_tooltip->setText(QString("<p style=\"color:%4;\"><b>%1</b></p>%2<br>Temperature: <b>%3 °C</b>")
                               .arg(sensor.descriptor.name.c_str())
                               .arg(dt.toString("dd-MM-yyyy h:mm"))
                               .arg(point.y(), 0, 'f', 1)
                               .arg(color.name()));
        }
        else
        {
            m_tooltip->setText(QString("<p style=\"color:%4;\"><b>%1</b></p>%2<br>Humidity: <b>%3 %RH</b>")
                               .arg(sensor.descriptor.name.c_str())
                               .arg(dt.toString("dd-MM-yyyy h:mm"))
                               .arg(point.y(), 0, 'f', 1)
                               .arg(color.name()));
        }
        m_tooltip->setAnchor(point, series);
        m_tooltip->setZValue(11);
        m_tooltip->updateGeometry();
        m_tooltip->show();
    }
    else
    {
        m_tooltip->hide();
    }
}

void PlotWidget::plotContextMenu(QPoint const& position)
{
    QMenu menu;

    QAction* action = menu.addAction(QIcon(":/icons/ui/smooth.png"), "Use Smoothing");
    action->setCheckable(true);
    action->setChecked(m_useFiltering);
    connect(action, &QAction::toggled, [this](bool triggered)
    {
        m_useFiltering = triggered;
        refresh();
    });

    action = menu.addAction(QIcon(":/icons/ui/remove.png"), "Remove Bubbles");
    action->setEnabled(!m_tooltips.empty());
    connect(action, &QAction::triggered, [this](bool triggered)
    {
        m_tooltips.clear();
    });

    menu.exec(mapToGlobal(position));
}

void PlotWidget::handleMarkerClicked()
{
    QLegendMarker* marker = qobject_cast<QLegendMarker*>(sender());
    Q_ASSERT(marker);

    switch (marker->type())
    {
    case QLegendMarker::LegendMarkerTypeXY:
    {
        // Toggle visibility of series
        marker->series()->setVisible(!marker->series()->isVisible());

        // Turn legend marker back to visible, since hiding series also hides the marker
        // and we don't want it to happen now.
        marker->setVisible(true);

        // Dim the marker, if series is not visible
        qreal alpha = 1.0;

        if (!marker->series()->isVisible())
        {
            alpha = 0.5;
        }

        QColor color;
        QBrush brush = marker->labelBrush();
        color = brush.color();
        color.setAlphaF(alpha);
        brush.setColor(color);
        marker->setLabelBrush(brush);

        brush = marker->brush();
        color = brush.color();
        color.setAlphaF(alpha);
        brush.setColor(color);
        marker->setBrush(brush);

        QPen pen = marker->pen();
        color = pen.color();
        color.setAlphaF(alpha);
        pen.setColor(color);
        marker->setPen(pen);

        break;
    }
    default:
    {
        qDebug() << "Unknown marker type";
        break;
    }
    }
}

QSize PlotWidget::getPlotSize() const
{
    return m_chartView->size();
}

QPixmap PlotWidget::grabPic(bool showLegend)
{
    QPixmap pixmap(m_chartView->size());
    pixmap.fill();

    QPainter painter(&pixmap);

    m_chart->legend()->setVisible(showLegend);

    m_chartView->render(&painter, QRectF(), QRect(), Qt::IgnoreAspectRatio);

    m_chart->legend()->setVisible(true);

    return pixmap;
}
