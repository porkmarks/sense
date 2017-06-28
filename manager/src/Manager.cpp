#include "Manager.h"

#include <QInputDialog>
#include <QMessageBox>

Manager::Manager(QWidget *parent)
    : QMainWindow(parent)
{
    m_ui.setupUi(this);

    m_ui.baseStationsWidget->init(m_comms);
    m_ui.configWidget->init(m_comms);
    m_ui.sensorsWidget->init(m_comms);
    m_ui.measurementsWidget->init(m_comms);

    auto* timer = new QTimer(this);
    timer->setSingleShot(false);
    timer->start(1);
    connect(timer, &QTimer::timeout, this, &Manager::process, Qt::QueuedConnection);

    connect(m_ui.baseStationsWidget, &BaseStationsWidget::baseStationSelected, this, &Manager::activateBaseStation);

    readSettings();

    show();
}

Manager::~Manager()
{
}

void Manager::activateBaseStation(Comms::BaseStation const& bs)
{
    m_comms.connectToBaseStation(bs.address);
}

void Manager::closeEvent(QCloseEvent* event)
{
    QSettings settings;
    settings.setValue("geometry", saveGeometry());
    settings.setValue("windowState", saveState());

    QMainWindow::closeEvent(event);
}

void Manager::readSettings()
{
    QSettings settings;
    restoreGeometry(settings.value("geometry").toByteArray());
    restoreState(settings.value("windowState").toByteArray());
}

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

