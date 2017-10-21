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

    refresh();

    connect(m_ui.exportLogs, &QPushButton::released, this, &LogsWidget::exportData);
    connect(m_ui.dateTimeFilter, &DateTimeFilterWidget::filterChanged, this, &LogsWidget::refresh, Qt::QueuedConnection);
    connect(m_ui.verbose, &QCheckBox::stateChanged, this, &LogsWidget::refresh, Qt::QueuedConnection);
    connect(m_ui.info, &QCheckBox::stateChanged, this, &LogsWidget::refresh, Qt::QueuedConnection);
    connect(m_ui.warning, &QCheckBox::stateChanged, this, &LogsWidget::refresh, Qt::QueuedConnection);
    connect(m_ui.error, &QCheckBox::stateChanged, this, &LogsWidget::refresh, Qt::QueuedConnection);
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

    filter.minTimePoint = Logger::Clock::from_time_t(m_ui.dateTimeFilter->getFromDateTime().toTime_t());
    filter.maxTimePoint = Logger::Clock::from_time_t(m_ui.dateTimeFilter->getToDateTime().toTime_t());
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

