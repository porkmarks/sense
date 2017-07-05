#include "PlotWidget.h"

PlotWidget::PlotWidget(QWidget* parent)
    : QWidget(parent)
{
    m_ui.setupUi(this);

    createPlotWidgets();
}

void PlotWidget::createPlotWidgets()
{
    delete m_plot.chart;
    delete m_plot.chartView;

    QFont font;
    font.setPointSize(8);

    {
        QChart* chart = new QChart();
        //chart->addSeries(m_plot.series);
        chart->legend()->hide();
        //chart->setTitle("plot");
        m_plot.chart = chart;

        QDateTimeAxis* axisX = new QDateTimeAxis(chart);
        axisX->setLabelsFont(font);
        axisX->setTitleFont(font);
        axisX->setTickCount(10);
        axisX->setFormat("MM yy h:mm");
        axisX->setTitleText("Date");
        chart->addAxis(axisX, Qt::AlignBottom);
        //m_plot.series->attachAxis(axisX);

        QValueAxis* axisTY = new QValueAxis(chart);
        axisTY->setLabelsFont(font);
        axisTY->setTitleFont(font);
        axisTY->setLabelFormat("%.1f");
        axisTY->setTitleText(u8"°C");
        chart->addAxis(axisTY, Qt::AlignLeft);

        QValueAxis* axisHY = new QValueAxis(chart);
        axisHY->setLabelsFont(font);
        axisHY->setTitleFont(font);
        axisHY->setLabelFormat("%.1f");
        axisHY->setTitleText("% RH");
        chart->addAxis(axisHY, Qt::AlignRight);

        m_plot.axisX = axisX;
        m_plot.axisTY = axisTY;
        m_plot.axisHY = axisHY;

        QChartView* chartView = new QChartView(chart);
        chartView->setRenderHint(QPainter::Antialiasing);
        m_plot.chartView = chartView;
    }

    m_ui.plot->layout()->addWidget(m_plot.chartView);
}

void PlotWidget::refresh(DB& db, MeasurementsModel& model)
{
    createPlotWidgets();

    m_plot.chartView->hide();
    m_plot.series.clear();

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

        float millis = std::chrono::duration_cast<std::chrono::milliseconds>(m.descriptor.timePoint.time_since_epoch()).count();
        time_t tt = DB::Clock::to_time_t(m.descriptor.timePoint);
        minTS = std::min<uint64_t>(minTS, tt);
        maxTS = std::max<uint64_t>(maxTS, tt);

        PlotData& plotData = m_plot.series[m.descriptor.sensorId];
        if (!plotData.temperatureSeries)
        {
            plotData.temperatureSeries.reset(new QLineSeries);
            m_plot.chart->addSeries(plotData.temperatureSeries.get());
            plotData.temperatureSeries->attachAxis(m_plot.axisX);
            plotData.temperatureSeries->attachAxis(m_plot.axisTY);
        }
        plotData.temperaturePoints.append(QPointF(millis, m.descriptor.temperature));
        //temperatureSeries->append(millis, m.descriptor.temperature);

        minT = std::min(minT, m.descriptor.temperature);
        maxT = std::max(maxT, m.descriptor.temperature);

        if (!plotData.humiditySeries)
        {
            plotData.humiditySeries.reset(new QLineSeries);
            m_plot.chart->addSeries(plotData.humiditySeries.get());
            plotData.humiditySeries->attachAxis(m_plot.axisX);
            plotData.humiditySeries->attachAxis(m_plot.axisHY);
        }
        plotData.humidityPoints.append(QPointF(millis, m.descriptor.humidity));

        minH = std::min(minH, m.descriptor.humidity);
        maxH = std::max(maxH, m.descriptor.humidity);
    }

    for (auto const& pair : m_plot.series)
    {
        pair.second.temperatureSeries->append(pair.second.temperaturePoints);
        pair.second.humiditySeries->append(pair.second.humidityPoints);
    }


    QDateTime minDT, maxDT;
    minDT.setTime_t(minTS);
    maxDT.setTime_t(maxTS);
    m_plot.axisX->setRange(minDT, maxDT);

    m_plot.axisTY->setRange(minT, maxT);
    m_plot.axisHY->setRange(minH, maxH);

    m_plot.chartView->show();
}
