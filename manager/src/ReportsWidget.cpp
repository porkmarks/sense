#include "ReportsWidget.h"
#include "ConfigureReportDialog.h"
#include "Settings.h"
#include <QSettings>

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

void ReportsWidget::init(Settings& settings, DB& db)
{
    for (const QMetaObject::Connection& connection: m_uiConnections)
    {
        QObject::disconnect(connection);
    }
    m_uiConnections.clear();

    setEnabled(true);

    m_settings = &settings;
    m_db = &db;

    m_model.reset(new ReportsModel(db));
    m_ui.list->setModel(m_model.get());
    m_ui.list->setUniformRowHeights(true);

    connect(m_ui.list->header(), &QHeaderView::sectionResized, [this]()
    {
        QSettings settings;
        settings.setValue("reports/list/state", m_ui.list->header()->saveState());
    });

    {
        QSettings settings;
        m_ui.list->header()->restoreState(settings.value("reports/list/state").toByteArray());
    }

    setRW();
    m_uiConnections.push_back(connect(&settings, &Settings::userLoggedIn, this, &ReportsWidget::setRW));
}

//////////////////////////////////////////////////////////////////////////

void ReportsWidget::shutdown()
{
    setEnabled(false);
    m_ui.list->setModel(nullptr);
    //m_ui.list->setItemDelegate(nullptr);
    m_model.reset();
    m_db = nullptr;
}

//////////////////////////////////////////////////////////////////////////

void ReportsWidget::setRW()
{
    m_ui.add->setEnabled(m_settings->isLoggedInAsAdmin());
    m_ui.remove->setEnabled(m_settings->isLoggedInAsAdmin());
}

//////////////////////////////////////////////////////////////////////////

void ReportsWidget::configureReport(QModelIndex const& index)
{
    if (!index.isValid() || static_cast<size_t>(index.row()) >= m_db->getReportCount())
    {
        return;
    }

    DB::Report report = m_db->getReport(index.row());

    ConfigureReportDialog dialog(*m_settings, *m_db);
    dialog.setReport(report);
    dialog.setEnabled(m_settings->isLoggedInAsAdmin());

    int result = dialog.exec();
    if (result == QDialog::Accepted)
    {
        report = dialog.getReport();
        m_db->setReport(report.id, report.descriptor);
        m_model->refresh();
    }
}

//////////////////////////////////////////////////////////////////////////

void ReportsWidget::addReport()
{
    ConfigureReportDialog dialog(*m_settings, *m_db);

    DB::Report report;
    report.descriptor.name = "Report " + std::to_string(m_db->getReportCount());
    dialog.setReport(report);

    int result = dialog.exec();
    if (result == QDialog::Accepted)
    {
        report = dialog.getReport();
        m_db->addReport(report.descriptor);
        m_model->refresh();
    }
}

//////////////////////////////////////////////////////////////////////////

void ReportsWidget::removeReports()
{
    QModelIndexList selected = m_ui.list->selectionModel()->selectedIndexes();
    if (selected.isEmpty())
    {
        QMessageBox::critical(this, "Error", "Please select the report you want to remove.");
        return;
    }

    QModelIndex mi = m_model->index(selected.at(0).row(), static_cast<int>(ReportsModel::Column::Id));
    DB::ReportId id = m_model->data(mi).toUInt();
    int32_t index = m_db->findReportIndexById(id);
    if (index < 0)
    {
        QMessageBox::critical(this, "Error", "Invalid report selected.");
        return;
    }

    DB::Report const& report = m_db->getReport(index);

    int response = QMessageBox::question(this, "Confirmation", QString("Are you sure you want to delete report '%1'").arg(report.descriptor.name.c_str()));
    if (response != QMessageBox::Yes)
    {
        return;
    }

    m_db->removeReport(index);
}

//////////////////////////////////////////////////////////////////////////

