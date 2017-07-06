#include "PlotWidget.h"

PlotWidget::PlotWidget(QWidget* parent)
    : QWidget(parent)
{
    m_ui.setupUi(this);

    createPlotWidgets();
}

void PlotWidget::createPlotWidgets()
{
    m_ui.legend->clear();
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
        //chart->addSeries(m_series);
        chart->legend()->hide();
        //chart->setTitle("plot");
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
    //m_chartView->setMaximumHeight(999999);
}

void PlotWidget::refresh(DB& db, MeasurementsModel& model)
{
    createPlotWidgets();

    m_series.clear();

    uint64_t minTS = std::numeric_limits<uint64_t>::max();
    uint64_t maxTS = std::numeric_limits<uint64_t>::lowest();
    float minT = std::numeric_limits<float>::max();
    float maxT = std::numeric_limits<float>::lowest();
    float minH = std::numeric_limits<float>::max();
    float maxH = std::numeric_limits<float>::lowest();
    for (size_t i = 0; i < model.getMeasurementCount(); i++)
    {
        DB::Measurement const& m = model.getMeasurement(i);
        int32_t sensorIndex = db.findSensorIndexById(m.descriptor.sensorId);
        if (sensorIndex < 0)
        {
            continue;
        }
//        if (i & 1)
//        {
//            continue;
//        }

        float millis = std::chrono::duration_cast<std::chrono::milliseconds>(m.descriptor.timePoint.time_since_epoch()).count();
        time_t tt = DB::Clock::to_time_t(m.descriptor.timePoint);
        minTS = std::min<uint64_t>(minTS, tt);
        maxTS = std::max<uint64_t>(maxTS, tt);

        PlotData& plotData = m_series[m.descriptor.sensorId];
        plotData.sensor = db.getSensor(sensorIndex);
        plotData.temperaturePoints.append(QPointF(millis, m.descriptor.temperature));
        plotData.humidityPoints.append(QPointF(millis, m.descriptor.humidity));

        minT = std::min(minT, m.descriptor.temperature);
        maxT = std::max(maxT, m.descriptor.temperature);

        minH = std::min(minH, m.descriptor.humidity);
        maxH = std::max(maxH, m.descriptor.humidity);
    }

    for (auto& pair : m_series)
    {
        PlotData& plotData = pair.second;
        {
            QLineSeries* series = new QLineSeries(m_chart);
            m_chart->addSeries(series);
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
            {
                QListWidgetItem* item = new QListWidgetItem(plotData.sensor.descriptor.name.c_str());
                item->setIcon(QIcon(":/icons/ui/temperature.png"));
                item->setBackgroundColor(series->color());
                m_ui.legend->addItem(item);
            }
        }
        {
            QLineSeries* series = new QLineSeries(m_chart);
            m_chart->addSeries(series);
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
            {
                QListWidgetItem* item = new QListWidgetItem(plotData.sensor.descriptor.name.c_str());
                item->setIcon(QIcon(":/icons/ui/humidity.png"));
                item->setBackgroundColor(series->color());
                m_ui.legend->addItem(item);
            }
        }
    }


    QDateTime minDT, maxDT;
    minDT.setTime_t(minTS);
    maxDT.setTime_t(maxTS);
    m_axisX->setRange(minDT, maxDT);

    constexpr float minTemperatureRange = 10.f;
    if (std::abs(maxT - minT) < minTemperatureRange)
    {
        float center = (maxT + minT) / 2.f;
        minT = center - minTemperatureRange / 2.f;
        maxT = center + minTemperatureRange / 2.f;
    }
    m_axisTY->setRange(minT, maxT);

    constexpr float minHumidityRange = 20.f;
    if (std::abs(maxH - minH) < minHumidityRange)
    {
        float center = (maxH + minH) / 2.f;
        minH = center - minHumidityRange / 2.f;
        maxH = center + minHumidityRange / 2.f;
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
            m_tooltip->setText(QString("<p style=\"color:%4;\"><b>%1</b></p>%2<br>Temperature: <b>%3 %RH</b>")
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
