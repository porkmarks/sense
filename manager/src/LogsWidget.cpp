#include "LogsWidget.h"
#include <iostream>
#include <QDateTime>
#include <QFileDialog>
#include <QMessageBox>

#include "Logger.h"
#include "ExportLogsDialog.h"

extern Logger s_logger;

//////////////////////////////////////////////////////////////////////////

LogsWidget::LogsWidget(QWidget *parent)
    : QWidget(parent)
{
    m_ui.setupUi(this);
    setEnabled(false);
}

//////////////////////////////////////////////////////////////////////////

LogsWidget::~LogsWidget()
{
    shutdown();
}

//////////////////////////////////////////////////////////////////////////

void LogsWidget::init()
{
    setEnabled(true);

    m_model.reset(new LogsModel(s_logger));
    m_sortingModel.setSourceModel(m_model.get());
    m_sortingModel.setSortRole(Qt::UserRole + 5);

    m_ui.list->setModel(&m_sortingModel);

    m_ui.list->setUniformRowHeights(true);

    setDateTimePreset(m_ui.dateTimePreset->currentIndex());
    refresh();

    connect(m_ui.exportLogs, &QPushButton::released, this, &LogsWidget::exportData);

    connect(m_ui.dateTimePreset, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &LogsWidget::setDateTimePreset);

    connect(m_ui.minDateTime, &QDateTimeEdit::editingFinished, this, &LogsWidget::minDateTimeChanged);
    connect(m_ui.maxDateTime, &QDateTimeEdit::editingFinished, this, &LogsWidget::maxDateTimeChanged);

    connect(m_ui.dateTimePreset, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &LogsWidget::refresh);
    connect(m_ui.minDateTime, &QDateTimeEdit::editingFinished, this, &LogsWidget::refresh);
    connect(m_ui.maxDateTime, &QDateTimeEdit::editingFinished, this, &LogsWidget::refresh);

    connect(m_ui.verbose, &QCheckBox::stateChanged, this, &LogsWidget::refresh);
    connect(m_ui.info, &QCheckBox::stateChanged, this, &LogsWidget::refresh);
    connect(m_ui.warning, &QCheckBox::stateChanged, this, &LogsWidget::refresh);
    connect(m_ui.error, &QCheckBox::stateChanged, this, &LogsWidget::refresh);
}

//////////////////////////////////////////////////////////////////////////

void LogsWidget::shutdown()
{
    setEnabled(false);
    m_ui.list->setModel(nullptr);
    m_sortingModel.setSourceModel(nullptr);
    m_model.reset();
}

//////////////////////////////////////////////////////////////////////////

Logger::Filter LogsWidget::createFilter() const
{
    Logger::Filter filter;

    filter.minTimePoint = Logger::Clock::from_time_t(m_ui.minDateTime->dateTime().toTime_t());
    filter.maxTimePoint = Logger::Clock::from_time_t(m_ui.maxDateTime->dateTime().toTime_t());
    filter.allowVerbose = m_ui.verbose->isChecked();
    filter.allowInfo = m_ui.info->isChecked();
    filter.allowWarning = m_ui.warning->isChecked();
    filter.allowError = m_ui.error->isChecked();

    return filter;
}

//////////////////////////////////////////////////////////////////////////

void LogsWidget::refresh()
{
    Logger::Clock::time_point start = Logger::Clock::now();

    //in case the date changed, reapply it
    if (m_ui.useDateTimePreset->isChecked())
    {
        setDateTimePreset(m_ui.dateTimePreset->currentIndex());
    }

    Logger::Filter filter = createFilter();
    m_model->setFilter(filter);

    s_logger.logVerbose(QString("Refreshed %1/%2 logs: %3ms")
                        .arg(m_model->getLineCount())
                        .arg(s_logger.getAllLogLineCount())
                        .arg(std::chrono::duration_cast<std::chrono::milliseconds>(Logger::Clock::now() - start).count()));

    //m_ui.resultCount->setText(QString("%1 out of %2 results.").arg(m_model->getLinesCount()).arg(s_logger->getAllLinesCount()));

    for (int i = 0; i < m_model->columnCount(QModelIndex()); i++)
    {
        m_ui.list->resizeColumnToContents(i);
    }

    m_ui.exportLogs->setEnabled(m_model->getLineCount() > 0);
}

//////////////////////////////////////////////////////////////////////////

void LogsWidget::exportData()
{
    ExportLogsDialog dialog(*m_model);
    int result = dialog.exec();
    if (result != QDialog::Accepted)
    {
        return;
    }
}

//////////////////////////////////////////////////////////////////////////

void LogsWidget::setMinDateTimeNow()
{
    QDateTime dt = QDateTime::currentDateTime();
    m_ui.minDateTime->setDateTime(dt);
}

//////////////////////////////////////////////////////////////////////////

void LogsWidget::setMaxDateTimeNow()
{
    QDateTime dt = QDateTime::currentDateTime();
    m_ui.maxDateTime->setDateTime(dt);
}

//////////////////////////////////////////////////////////////////////////

void LogsWidget::setDateTimePreset(int preset)
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

void LogsWidget::setDateTimePresetToday()
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

void LogsWidget::setDateTimePresetYesterday()
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

void LogsWidget::setDateTimePresetThisWeek()
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

void LogsWidget::setDateTimePresetLastWeek()
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

void LogsWidget::setDateTimePresetThisMonth()
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

void LogsWidget::setDateTimePresetLastMonth()
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

void LogsWidget::minDateTimeChanged()
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

void LogsWidget::maxDateTimeChanged()
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

