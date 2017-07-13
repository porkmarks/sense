#include "Manager.h"

#include <QInputDialog>
#include <QMessageBox>

#include "SensorSettingsDialog.h"
#include "EmailSettingsDialog.h"
#include "FtpSettingsDialog.h"

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

    connect(m_ui.actionSensorSettings, &QAction::triggered, this, &Manager::openSensorSettingsDialog);
    connect(m_ui.actionEmailSettings, &QAction::triggered, this, &Manager::openEmailSettingsDialog);
    connect(m_ui.actionFtpSettings, &QAction::triggered, this, &Manager::openFtpSettingsDialog);

    readSettings();

    show();
}

//////////////////////////////////////////////////////////////////////////

Manager::~Manager()
{
    delete m_ui.baseStationsWidget;
    delete m_ui.sensorsWidget;
    delete m_ui.measurementsWidget;
    delete m_ui.alarmsWidget;
    delete m_ui.reportsWidget;
}

//////////////////////////////////////////////////////////////////////////

void Manager::activateBaseStation(Comms::BaseStationDescriptor const& bs, DB& db)
{
    m_ui.sensorsWidget->init(db);
    m_ui.measurementsWidget->init(db);
    m_ui.alarmsWidget->init(db);
    m_ui.reportsWidget->init(db);

    m_ui.actionEmailSettings->setEnabled(true);
    m_ui.actionFtpSettings->setEnabled(true);
    m_ui.actionSensorSettings->setEnabled(true);

    m_activeDB = &db;

    //db.test();
}

//////////////////////////////////////////////////////////////////////////

void Manager::deactivateBaseStation(Comms::BaseStationDescriptor const& bs)
{
    m_ui.sensorsWidget->shutdown();
    m_ui.measurementsWidget->shutdown();
    m_ui.alarmsWidget->shutdown();
    m_ui.reportsWidget->shutdown();

    m_ui.actionEmailSettings->setEnabled(false);
    m_ui.actionFtpSettings->setEnabled(false);
    m_ui.actionSensorSettings->setEnabled(false);

    m_activeDB = nullptr;
}

//////////////////////////////////////////////////////////////////////////

void Manager::openSensorSettingsDialog()
{
    if (!m_activeDB)
    {
        return;
    }

    DB::SensorSettings settings = m_activeDB->getSensorSettings();

    SensorSettingsDialog dialog(this);
    dialog.setSensorSettings(settings);

    int result = dialog.exec();
    if (result == QDialog::Accepted)
    {
        settings = dialog.getSensorSettings();
        if (!m_activeDB->setSensorSettings(settings.descriptor))
        {
            QMessageBox::critical(this, "Error", "Cannot set the sensor settings.");
            return;
        }
    }
}

//////////////////////////////////////////////////////////////////////////

void Manager::openEmailSettingsDialog()
{
    if (!m_activeDB)
    {
        return;
    }

    DB::EmailSettings settings = m_activeDB->getEmailSettings();

    EmailSettingsDialog dialog(this);
    dialog.setEmailSettings(settings);

    int result = dialog.exec();
    if (result == QDialog::Accepted)
    {
        settings = dialog.getEmailSettings();
        if (!m_activeDB->setEmailSettings(settings))
        {
            QMessageBox::critical(this, "Error", "Cannot set the email settings.");
            return;
        }
    }
}

//////////////////////////////////////////////////////////////////////////

void Manager::openFtpSettingsDialog()
{
    if (!m_activeDB)
    {
        return;
    }

    DB::FtpSettings settings = m_activeDB->getFtpSettings();

    FtpSettingsDialog dialog(this);
    dialog.setFtpSettings(settings);

    int result = dialog.exec();
    if (result == QDialog::Accepted)
    {
        settings = dialog.getFtpSettings();
        if (!m_activeDB->setFtpSettings(settings))
        {
            QMessageBox::critical(this, "Error", "Cannot set the ftp settings.");
            return;
        }
    }
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

