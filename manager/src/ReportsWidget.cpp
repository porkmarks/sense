#include "ReportsWidget.h"
#include "ConfigureReportDialog.h"

//////////////////////////////////////////////////////////////////////////

ReportsWidget::ReportsWidget(QWidget *parent)
    : QWidget(parent)
{
    m_ui.setupUi(this);
    setEnabled(false);

    connect(m_ui.add, &QPushButton::released, this, &ReportsWidget::addReport);
    connect(m_ui.remove, &QPushButton::released, this, &ReportsWidget::removeReports);
    connect(m_ui.list, &QTreeView::activated, this, &ReportsWidget::configureReport);
}

//////////////////////////////////////////////////////////////////////////

ReportsWidget::~ReportsWidget()
{
    shutdown();
}

//////////////////////////////////////////////////////////////////////////

void ReportsWidget::init(DB& db)
{
    setEnabled(true);
    m_db = &db;

    m_model.reset(new ReportsModel(db));
    m_ui.list->setModel(m_model.get());

    for (int i = 0; i < m_model->columnCount(QModelIndex()); i++)
    {
        m_ui.list->resizeColumnToContents(i);
    }
}

//////////////////////////////////////////////////////////////////////////

void ReportsWidget::shutdown()
{
    setEnabled(false);
    m_ui.list->setModel(nullptr);
    m_ui.list->setItemDelegate(nullptr);
    m_model.reset();
    m_db = nullptr;
}

//////////////////////////////////////////////////////////////////////////

void ReportsWidget::configureReport(QModelIndex const& index)
{
    if (!index.isValid() || static_cast<size_t>(index.row()) >= m_db->getReportCount())
    {
        return;
    }

    DB::Report report = m_db->getReport(index.row());

    ConfigureReportDialog dialog(*m_db);
    dialog.setReport(report);

    int result = dialog.exec();
    if (result == QDialog::Accepted)
    {
        report = dialog.getReport();
        m_db->setReport(report.id, report.descriptor);
        m_model->refresh();

        for (int i = 0; i < m_model->columnCount(QModelIndex()); i++)
        {
            m_ui.list->resizeColumnToContents(i);
        }
    }
}

//////////////////////////////////////////////////////////////////////////

void ReportsWidget::addReport()
{
    ConfigureReportDialog dialog(*m_db);

    int result = dialog.exec();
    if (result == QDialog::Accepted)
    {
        DB::Report report = dialog.getReport();
        m_db->addReport(report.descriptor);
        m_model->refresh();

        for (int i = 0; i < m_model->columnCount(QModelIndex()); i++)
        {
            m_ui.list->resizeColumnToContents(i);
        }
    }
}

//////////////////////////////////////////////////////////////////////////

void ReportsWidget::removeReports()
{
    for (int i = 0; i < m_model->columnCount(QModelIndex()); i++)
    {
        m_ui.list->resizeColumnToContents(i);
    }
}

//////////////////////////////////////////////////////////////////////////

