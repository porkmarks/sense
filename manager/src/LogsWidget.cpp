#include "LogsWidget.h"
#include <iostream>
#include <QDateTime>
#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>

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

void LogsWidget::init(DB& db)
{
    m_db = &db;
    for (const QMetaObject::Connection& connection: m_uiConnections)
        QObject::disconnect(connection);
    m_uiConnections.clear();

    setEnabled(true);

    m_model.reset(new LogsModel(*m_db, s_logger));
    m_sortingModel.setSourceModel(m_model.get());
    m_sortingModel.setSortRole(Qt::UserRole + 5);

    m_ui.list->setModel(&m_sortingModel);
    //m_ui.list->setUniformRowHeights(true);

    connect(m_ui.list->header(), &QHeaderView::sectionResized, [this]()
    {
        if (!m_sectionSaveScheduled)
        {
            m_sectionSaveScheduled = true;
//            QTimer::singleShot(500, [this]()
            {
                m_sectionSaveScheduled = false;
                QSettings settings;
                settings.setValue("logs/list/state", m_ui.list->header()->saveState());
            }//);
        }
    });

    {
        QSettings settings;
        m_ui.list->header()->restoreState(settings.value("logs/list/state").toByteArray());
    }

    refresh();

    m_uiConnections.push_back(connect(m_ui.exportLogs, &QPushButton::released, this, &LogsWidget::exportData));
    m_uiConnections.push_back(connect(m_ui.dateTimeFilter, &DateTimeFilterWidget::filterChanged, this, &LogsWidget::refresh, Qt::QueuedConnection));
    m_uiConnections.push_back(connect(m_ui.verbose, &QCheckBox::stateChanged, this, &LogsWidget::refresh, Qt::QueuedConnection));
    m_uiConnections.push_back(connect(m_ui.info, &QCheckBox::stateChanged, this, &LogsWidget::refresh, Qt::QueuedConnection));
    m_uiConnections.push_back(connect(m_ui.warning, &QCheckBox::stateChanged, this, &LogsWidget::refresh, Qt::QueuedConnection));
    m_uiConnections.push_back(connect(m_ui.error, &QCheckBox::stateChanged, this, &LogsWidget::refresh, Qt::QueuedConnection));
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

void LogsWidget::setAutoRefresh(bool enabled)
{
    m_model->setAutoRefresh(enabled);
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
    filter.allowCritical = m_ui.error->isChecked();

    return filter;
}

//////////////////////////////////////////////////////////////////////////

void LogsWidget::refresh()
{
    Logger::Clock::time_point start = Logger::Clock::now();

    Logger::Filter filter = createFilter();
    m_model->setFilter(filter);

    std::cout << (QString("Refreshed %1 logs: %3ms\n")
                        .arg(m_model->getLineCount())
                        .arg(std::chrono::duration_cast<std::chrono::milliseconds>(Logger::Clock::now() - start).count())).toStdString();

    //m_ui.resultCount->setText(QString("%1 out of %2 results.").arg(m_model->getLinesCount()).arg(s_logger->getAllLinesCount()));

    m_ui.exportLogs->setEnabled(m_model->getLineCount() > 0);
}

//////////////////////////////////////////////////////////////////////////

void LogsWidget::exportData()
{
    ExportLogsDialog dialog(*m_model, this);
    int result = dialog.exec();
    if (result != QDialog::Accepted)
        return;
}

//////////////////////////////////////////////////////////////////////////

