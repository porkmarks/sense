#include "PlotWidget.h"

#include "ExportPicDialog.h"
#include "ExportDataDialog.h"
#include "SensorsModel.h"
#include "SensorsDelegate.h"
#include "ui_SensorsFilterDialog.h"

//////////////////////////////////////////////////////////////////////////

PlotWidget::PlotWidget(QWidget* parent)
    : QWidget(parent)
{
    m_ui.setupUi(this);
    setEnabled(false);

//    setContextMenuPolicy(Qt::CustomContextMenu);
//    connect(this, &QChartView::customContextMenuRequested, this, &PlotWidget::plotContextMenu);

    createPlotWidgets();
}

//////////////////////////////////////////////////////////////////////////

void PlotWidget::init(DB& db)
{
    setEnabled(true);
    m_db = &db;

    connect(m_ui.clearAnnotations, &QPushButton::released, this, &PlotWidget::clearAnnotations);
    connect(m_ui.dateTimePreset, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &PlotWidget::setDateTimePreset);
    connect(m_ui.selectSensors, &QPushButton::released, this, &PlotWidget::selectSensors);
    connect(m_ui.exportData, &QPushButton::released, this, &PlotWidget::exportData);

    connect(m_ui.minDateTime, &QDateTimeEdit::editingFinished, this, &PlotWidget::minDateTimeChanged);
    connect(m_ui.maxDateTime, &QDateTimeEdit::editingFinished, this, &PlotWidget::maxDateTimeChanged);

    connect(m_ui.fitMeasurements, &QCheckBox::stateChanged, this, &PlotWidget::refresh);
    connect(m_ui.minTemperature, &QDoubleSpinBox::editingFinished, this, &PlotWidget::refresh);
    connect(m_ui.maxTemperature, &QDoubleSpinBox::editingFinished, this, &PlotWidget::refresh);
    connect(m_ui.minHumidity, &QDoubleSpinBox::editingFinished, this, &PlotWidget::refresh);
    connect(m_ui.maxHumidity, &QDoubleSpinBox::editingFinished, this, &PlotWidget::refresh);

    connect(m_ui.useSmoothing, &QCheckBox::stateChanged, this, &PlotWidget::refresh);
    connect(m_ui.dateTimePreset, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &PlotWidget::refresh);
    connect(m_ui.minDateTime, &QDateTimeEdit::editingFinished, this, &PlotWidget::refresh);
    connect(m_ui.maxDateTime, &QDateTimeEdit::editingFinished, this, &PlotWidget::refresh);

    setDateTimePreset(m_ui.dateTimePreset->currentIndex());
    refresh();
}

//////////////////////////////////////////////////////////////////////////

void PlotWidget::shutdown()
{
    setEnabled(false);
    m_series.clear();
    m_db = nullptr;
}

//////////////////////////////////////////////////////////////////////////

DB::Filter PlotWidget::createFilter() const
{
    DB::Filter filter;

    if (!m_selectedSensorIds.empty())
    {
        filter.useSensorFilter = true;
        filter.sensorIds = m_selectedSensorIds;
    }

    //filter.useTimePointFilter = m_ui.dateTimeFilter->isChecked();
    filter.useTimePointFilter = true;
    filter.timePointFilter.min = DB::Clock::from_time_t(m_ui.minDateTime->dateTime().toTime_t());
    filter.timePointFilter.max = DB::Clock::from_time_t(m_ui.maxDateTime->dateTime().toTime_t());

    return filter;
}

//////////////////////////////////////////////////////////////////////////

void PlotWidget::refresh()
{
    //in case the date changed, reapply it
    if (m_ui.useDateTimePreset->isChecked())
    {
        setDateTimePreset(m_ui.dateTimePreset->currentIndex());
    }

    m_useSmoothing = m_ui.useSmoothing->isChecked();
    m_fitMeasurements = m_ui.fitMeasurements->isChecked();

    DB::Filter filter = createFilter();
    applyFilter(filter);
}

//////////////////////////////////////////////////////////////////////////

void PlotWidget::setDateTimePreset(int preset)
{
    switch (preset)
    {
    case 0: setDateTimePresetToday(); break;
    case 1: setDateTimePresetYesterday(); break;
    case 2: setDateTimePresetThisWeek(); break;
    case 3: setDateTimePresetLastWeek(); break;
    case 4: setDateTimePresetThisMonth(); break;
    case 5: setDateTimePresetLastMonth(); break;
    }
}

//////////////////////////////////////////////////////////////////////////

void PlotWidget::setDateTimePresetToday()
{
    QDateTime dt = QDateTime::currentDateTime();

    dt.setTime(QTime(23, 59, 59, 999));
    m_ui.maxDateTime->blockSignals(true);
    m_ui.maxDateTime->setDateTime(dt);
    m_ui.maxDateTime->blockSignals(false);

    dt.setTime(QTime(0, 0));
    m_ui.minDateTime->setDateTime(dt);
}

//////////////////////////////////////////////////////////////////////////

void PlotWidget::setDateTimePresetYesterday()
{
    QDateTime dt = QDateTime::currentDateTime();

    dt.setDate(dt.date().addDays(-1));

    dt.setTime(QTime(23, 59, 59, 999));
    m_ui.maxDateTime->blockSignals(true);
    m_ui.maxDateTime->setDateTime(dt);
    m_ui.maxDateTime->blockSignals(false);

    dt.setTime(QTime(0, 0));
    m_ui.minDateTime->setDateTime(dt);
}

//////////////////////////////////////////////////////////////////////////

void PlotWidget::setDateTimePresetThisWeek()
{
    QDateTime dt = QDateTime::currentDateTime();

    dt.setDate(dt.date().addDays(-dt.date().dayOfWeek()));
    dt.setTime(QTime(0, 0));
    m_ui.minDateTime->blockSignals(true);
    m_ui.minDateTime->setDateTime(dt);
    m_ui.minDateTime->blockSignals(false);

    dt.setDate(dt.date().addDays(6));
    dt.setTime(QTime(23, 59, 59, 999));
    m_ui.maxDateTime->setDateTime(dt);
}

//////////////////////////////////////////////////////////////////////////

void PlotWidget::setDateTimePresetLastWeek()
{
    QDateTime dt = QDateTime::currentDateTime();

    dt.setDate(dt.date().addDays(-dt.date().dayOfWeek()).addDays(-7));
    dt.setTime(QTime(0, 0));
    m_ui.minDateTime->blockSignals(true);
    m_ui.minDateTime->setDateTime(dt);
    m_ui.minDateTime->blockSignals(false);

    dt.setDate(dt.date().addDays(6));
    dt.setTime(QTime(23, 59, 59, 999));
    m_ui.maxDateTime->setDateTime(dt);
}

//////////////////////////////////////////////////////////////////////////

void PlotWidget::setDateTimePresetThisMonth()
{
    QDate date = QDate::currentDate();
    date = QDate(date.year(), date.month(), 1);

    QDateTime dt(date, QTime(0, 0));
    m_ui.minDateTime->blockSignals(true);
    m_ui.minDateTime->setDateTime(dt);
    m_ui.minDateTime->blockSignals(false);

    dt = QDateTime(date.addMonths(1).addDays(-1), QTime(23, 59, 59, 999));
    m_ui.maxDateTime->setDateTime(dt);
}

//////////////////////////////////////////////////////////////////////////

void PlotWidget::setDateTimePresetLastMonth()
{
    QDate date = QDate::currentDate();
    date = QDate(date.year(), date.month(), 1).addMonths(-1);

    QDateTime dt(date, QTime(0, 0));
    m_ui.minDateTime->blockSignals(true);
    m_ui.minDateTime->setDateTime(dt);
    m_ui.minDateTime->blockSignals(false);

    dt = QDateTime(date.addMonths(1).addDays(-1), QTime(23, 59, 59, 999));
    m_ui.maxDateTime->setDateTime(dt);
}

//////////////////////////////////////////////////////////////////////////

void PlotWidget::minDateTimeChanged()
{
    QDateTime value = m_ui.minDateTime->dateTime();
    if (value >= m_ui.maxDateTime->dateTime())
    {
        QDateTime dt = value;
        dt.setTime(dt.time().addSecs(3600));
        m_ui.maxDateTime->setDateTime(dt);
    }
}

//////////////////////////////////////////////////////////////////////////

void PlotWidget::maxDateTimeChanged()
{
    QDateTime value = m_ui.maxDateTime->dateTime();
    if (value <= m_ui.minDateTime->dateTime())
    {
        QDateTime dt = value;
        dt.setTime(dt.time().addSecs(-3600));
        m_ui.minDateTime->setDateTime(dt);
    }
}

//////////////////////////////////////////////////////////////////////////

void PlotWidget::exportData()
{
    ExportPicDialog dialog(*this);
    int result = dialog.exec();
    if (result != QDialog::Accepted)
    {
        return;
    }
}

//////////////////////////////////////////////////////////////////////////

void PlotWidget::selectSensors()
{
    QDialog dialog;
    Ui::SensorsFilterDialog ui;
    ui.setupUi(&dialog);

    SensorsModel model(*m_db);
    model.setShowCheckboxes(true);

    QSortFilterProxyModel sortingModel;
    sortingModel.setSourceModel(&model);

    SensorsDelegate delegate(sortingModel);

    size_t sensorCount = m_db->getSensorCount();
    if (m_selectedSensorIds.empty())
    {
        for (size_t i = 0; i < sensorCount; i++)
        {
            model.setSensorChecked(m_db->getSensor(i).id, true);
        }
    }
    else
    {
        for (DB::SensorId id: m_selectedSensorIds)
        {
            model.setSensorChecked(id, true);
        }
    }

    ui.list->setModel(&sortingModel);
    ui.list->setItemDelegate(&delegate);

    for (int i = 0; i < model.columnCount(); i++)
    {
        ui.list->header()->setSectionResizeMode(i, QHeaderView::ResizeToContents);
    }
    ui.list->header()->setStretchLastSection(true);

    int result = dialog.exec();
    if (result == QDialog::Accepted)
    {
        m_selectedSensorIds.clear();
        for (size_t i = 0; i < sensorCount; i++)
        {
            DB::Sensor const& sensor = m_db->getSensor(i);
            if (model.isSensorChecked(sensor.id))
            {
                m_selectedSensorIds.push_back(sensor.id);
            }
        }
    }

    refresh();
}

//////////////////////////////////////////////////////////////////////////

void PlotWidget::clearAnnotations()
{
    m_annotations.clear();
    m_ui.clearAnnotations->setEnabled(false);
}

//////////////////////////////////////////////////////////////////////////

void PlotWidget::createPlotWidgets()
{
    m_annotation.reset();
    clearAnnotations();

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

//////////////////////////////////////////////////////////////////////////

void PlotWidget::applyFilter(DB::Filter const& filter)
{
    m_filter = filter;
    foreach (QLegendMarker* marker, m_chart->legend()->markers())
    {
        QObject::disconnect(marker, SIGNAL(clicked()), this, SLOT(handleMarkerClicked()));
    }

    //save the visibilities
    std::vector<bool> markersVisible;
    foreach (QLegendMarker* marker, m_chart->legend()->markers())
    {
        markersVisible.push_back(marker->series()->isVisible());
    }

    createPlotWidgets();

    m_series.clear();

    uint64_t minTS = std::numeric_limits<uint64_t>::max();
    uint64_t maxTS = std::numeric_limits<uint64_t>::lowest();
    qreal minT = std::numeric_limits<qreal>::max();
    qreal maxT = std::numeric_limits<qreal>::lowest();
    qreal minH = std::numeric_limits<qreal>::max();
    qreal maxH = std::numeric_limits<qreal>::lowest();

    std::vector<DB::Measurement> measurements = m_db->getFilteredMeasurements(m_filter);

    m_ui.resultCount->setText(QString("%1 out of %2 results.").arg(measurements.size()).arg(m_db->getAllMeasurementCount()));
    m_ui.exportData->setEnabled(!measurements.empty());

    std::sort(measurements.begin(), measurements.end(), [](DB::Measurement const& a, DB::Measurement const& b) { return a.descriptor.timePoint < b.descriptor.timePoint; });

    for (size_t i = 0; i < m_db->getSensorCount(); i++)
    {
        DB::Sensor const& sensor = m_db->getSensor(i);
        PlotData& plotData = m_series[sensor.id];
        plotData.sensor = sensor;
        plotData.temperatureLpf.setup(1, 1, 0.15);
        plotData.humidityLpf.setup(1, 1, 0.15);
        plotData.temperaturePoints.reserve(8192);
        plotData.humidityPoints.reserve(8192);
    }

    for (DB::Measurement const& m : measurements)
    {
        qreal millis = std::chrono::duration_cast<std::chrono::milliseconds>(m.descriptor.timePoint.time_since_epoch()).count();
        time_t tt = DB::Clock::to_time_t(m.descriptor.timePoint);
        minTS = std::min<uint64_t>(minTS, tt);
        maxTS = std::max<uint64_t>(maxTS, tt);

        auto it = m_series.find(m.descriptor.sensorId);
        if (it == m_series.end())
        {
            continue;
        }
        PlotData& plotData = it->second;

        qreal value = m.descriptor.temperature;
        if (m_useSmoothing)
        {
            plotData.temperatureLpf.process(value);
        }
        plotData.temperaturePoints.append(QPointF(millis, value));
        minT = std::min(minT, value);
        maxT = std::max(maxT, value);

        value = m.descriptor.humidity;
        if (m_useSmoothing)
        {
            plotData.humidityLpf.process(value);
        }
        plotData.humidityPoints.append(QPointF(millis, value));
        minH = std::min(minH, value);
        maxH = std::max(maxH, value);
    }

    QDateTime minDT, maxDT;
    minDT.setTime_t(minTS);
    maxDT.setTime_t(maxTS);
    m_axisX->setRange(minDT, maxDT);

    if (m_fitMeasurements)
    {
        constexpr qreal minTemperatureRange = 5.0;
        if (std::abs(maxT - minT) < minTemperatureRange)
        {
            qreal center = (maxT + minT) / 2.0;
            minT = center - minTemperatureRange / 2.0;
            maxT = center + minTemperatureRange / 2.0;
        }
        constexpr qreal minHumidityRange = 10.0;
        if (std::abs(maxH - minH) < minHumidityRange)
        {
            qreal center = (maxH + minH) / 2.0;
            minH = center - minHumidityRange / 2.0;
            maxH = center + minHumidityRange / 2.0;
        }
    }
    else
    {
        maxT = std::max(m_ui.minTemperature->value(), m_ui.maxTemperature->value());
        minT = std::min(m_ui.minTemperature->value(), m_ui.maxTemperature->value());
        maxH = std::max(m_ui.minHumidity->value(), m_ui.maxHumidity->value());
        minH = std::min(m_ui.minHumidity->value(), m_ui.maxHumidity->value());
    }

    m_axisTY->setRange(minT, maxT);
    m_axisHY->setRange(minH, maxH);


    for (auto& pair : m_series)
    {
        PlotData& plotData = pair.second;
        {
            QLineSeries* series = new QLineSeries(m_chart);
            m_chart->addSeries(series);
            QPen pen = series->pen();
            //pen.setWidth(4);
            //series->setUseOpenGL(true);
            series->setPen(pen);
            series->setName(QString("%1 °C").arg(plotData.sensor.descriptor.name.c_str()));
            series->attachAxis(m_axisX);
            series->attachAxis(m_axisTY);

            connect(series, SIGNAL(clicked(QPointF)), this, SLOT(keepAnnotation()));
            connect(series, &QLineSeries::hovered, [this, series, &plotData](QPointF const& point, bool state)
            {
                createAnnotation(series, point, state, plotData.sensor, true);
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
            //series->setUseOpenGL(true);
            series->setPen(pen);
            series->setName(QString("%1 %RH").arg(plotData.sensor.descriptor.name.c_str()));
            series->attachAxis(m_axisX);
            series->attachAxis(m_axisHY);

            connect(series, SIGNAL(clicked(QPointF)), this, SLOT(keepAnnotation()));
            connect(series, &QLineSeries::hovered, [this, series, &plotData](QPointF const& point, bool state)
            {
                createAnnotation(series, point, state, plotData.sensor, false);
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

    size_t index = 0;
    foreach (QLegendMarker* marker, m_chart->legend()->markers())
    {
        // Disconnect possible existing connection to avoid multiple connections
        QObject::disconnect(marker, SIGNAL(clicked()), this, SLOT(handleMarkerClicked()));
        QObject::connect(marker, SIGNAL(clicked()), this, SLOT(handleMarkerClicked()));
        if (index < markersVisible.size())
        {
            setMarkerVisible(marker, markersVisible[index]);
        }
        index++;
    }
}

//////////////////////////////////////////////////////////////////////////

void PlotWidget::resizeEvent(QResizeEvent *event)
{
    for (std::unique_ptr<PlotToolTip>& p: m_annotations)
    {
        p->updateGeometry();
    }
    QWidget::resizeEvent(event);
}

//////////////////////////////////////////////////////////////////////////

void PlotWidget::keepAnnotation()
{
    if (m_annotation != nullptr)
    {
        m_annotation->setFixed(true);

        PlotToolTip* annotation = m_annotation.get();
        connect(annotation, &PlotToolTip::closeMe, [this, annotation]()
        {
            auto it = std::find_if(m_annotations.begin(), m_annotations.end(), [annotation](std::unique_ptr<PlotToolTip> const& a) { return a.get() == annotation; });
            if (it != m_annotations.end())
            {
                m_annotations.erase(it);
            }
        });

        m_annotations.push_back(std::move(m_annotation));
        m_ui.clearAnnotations->setEnabled(true);

        m_annotation.reset(new PlotToolTip(m_chart));
    }
}

//////////////////////////////////////////////////////////////////////////

void PlotWidget::createAnnotation(QLineSeries* series, QPointF point, bool state, DB::Sensor const& sensor, bool temperature)
{
    if (m_annotation == nullptr)
    {
        m_annotation.reset(new PlotToolTip(m_chart));
    }

    if (state)
    {
        QColor color = series->color();
        QDateTime dt;
        dt.setMSecsSinceEpoch(point.x());
        if (temperature)
        {
            m_annotation->setText(QString("<p style=\"color:%4;\"><b>%1</b></p>%2<br>Temperature: <b>%3 °C</b>")
                               .arg(sensor.descriptor.name.c_str())
                               .arg(dt.toString("dd-MM-yyyy h:mm"))
                               .arg(point.y(), 0, 'f', 1)
                               .arg(color.name()));
        }
        else
        {
            m_annotation->setText(QString("<p style=\"color:%4;\"><b>%1</b></p>%2<br>Humidity: <b>%3 %RH</b>")
                               .arg(sensor.descriptor.name.c_str())
                               .arg(dt.toString("dd-MM-yyyy h:mm"))
                               .arg(point.y(), 0, 'f', 1)
                               .arg(color.name()));
        }
        m_annotation->setAnchor(point, series);
        m_annotation->setZValue(11);
        m_annotation->updateGeometry();
        m_annotation->show();
    }
    else
    {
        m_annotation->hide();
    }
}

//////////////////////////////////////////////////////////////////////////

void PlotWidget::plotContextMenu(QPoint const& position)
{
//    QMenu menu;
//    menu.exec(mapToGlobal(position));
}

//////////////////////////////////////////////////////////////////////////

void PlotWidget::handleMarkerClicked()
{
    QLegendMarker* marker = qobject_cast<QLegendMarker*>(sender());
    Q_ASSERT(marker);

    setMarkerVisible(marker, !marker->series()->isVisible());
}

//////////////////////////////////////////////////////////////////////////

void PlotWidget::setMarkerVisible(QLegendMarker* marker, bool visible)
{
    switch (marker->type())
    {
    case QLegendMarker::LegendMarkerTypeXY:
    {
        // Toggle visibility of series
        marker->series()->setVisible(visible);

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

//////////////////////////////////////////////////////////////////////////

QSize PlotWidget::getPlotSize() const
{
    return m_chartView->size();
}

//////////////////////////////////////////////////////////////////////////

QPixmap PlotWidget::grabPic(bool showLegend)
{
    QPixmap pixmap(m_chartView->size());
    pixmap.fill();

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.setRenderHint(QPainter::HighQualityAntialiasing, true);

    m_chart->legend()->setVisible(showLegend);

    m_chartView->render(&painter, QRectF(), QRect(), Qt::IgnoreAspectRatio);

    m_chart->legend()->setVisible(true);

    return pixmap;
}

//////////////////////////////////////////////////////////////////////////

