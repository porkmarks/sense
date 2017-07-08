#include "Manager.h"

#include <QInputDialog>
#include <QMessageBox>

//////////////////////////////////////////////////////////////////////////

Manager::Manager(QWidget *parent)
    : QMainWindow(parent)
{
    m_ui.setupUi(this);

    m_ui.baseStationsWidget->init(m_comms);

    auto* timer = new QTimer(this);
    timer->setSingleShot(false);
    timer->start(1);
    connect(timer, &QTimer::timeout, this, &Manager::process, Qt::QueuedConnection);

    connect(m_ui.baseStationsWidget, &BaseStationsWidget::baseStationActivated, this, &Manager::activateBaseStation);
    connect(m_ui.baseStationsWidget, &BaseStationsWidget::baseStationDeactivated, this, &Manager::deactivateBaseStation);

    readSettings();

    show();
}

//////////////////////////////////////////////////////////////////////////

Manager::~Manager()
{
    delete m_ui.baseStationsWidget;
    delete m_ui.configWidget;
    delete m_ui.sensorsWidget;
    delete m_ui.measurementsWidget;
    delete m_ui.alarmsWidget;
    delete m_ui.reportsWidget;
}

//////////////////////////////////////////////////////////////////////////

void Manager::activateBaseStation(Comms::BaseStationDescriptor const& bs, DB& db)
{
    m_ui.configWidget->init(db);
    m_ui.sensorsWidget->init(db);
    m_ui.measurementsWidget->init(db);
    m_ui.alarmsWidget->init(db);
    m_ui.reportsWidget->init(db);
}

//////////////////////////////////////////////////////////////////////////

void Manager::deactivateBaseStation(Comms::BaseStationDescriptor const& bs)
{
    m_ui.configWidget->shutdown();
    m_ui.sensorsWidget->shutdown();
    m_ui.measurementsWidget->shutdown();
    m_ui.alarmsWidget->shutdown();
    m_ui.reportsWidget->shutdown();
}

//////////////////////////////////////////////////////////////////////////

void Manager::closeEvent(QCloseEvent* event)
{
    QSettings settings;
    settings.setValue("geometry", saveGeometry());
    settings.setValue("windowState", saveState());

    QMainWindow::closeEvent(event);
}

//////////////////////////////////////////////////////////////////////////

void Manager::readSettings()
{
    QSettings settings;
    restoreGeometry(settings.value("geometry").toByteArray());
    restoreState(settings.value("windowState").toByteArray());
}

//////////////////////////////////////////////////////////////////////////

void Manager::process()
{
//    if (m_comms.is_connected())
//    {
//        m_ui.statusbar->showMessage(("Connected to " + m_remote_address).c_str());
//    }
//    else
//    {
//        m_ui.statusbar->showMessage("");
//    }

    m_comms.process();
}

//////////////////////////////////////////////////////////////////////////

