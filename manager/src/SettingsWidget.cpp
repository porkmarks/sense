#include "SettingsWidget.h"
#include "ConfigureAlarmDialog.h"
#include <QMessageBox>

#include "DB.h"

//////////////////////////////////////////////////////////////////////////

SettingsWidget::SettingsWidget(QWidget *parent)
    : QWidget(parent)
{
    m_ui.setupUi(this);
    //setEnabled(false);
}

//////////////////////////////////////////////////////////////////////////

SettingsWidget::~SettingsWidget()
{
    shutdown();
}

//////////////////////////////////////////////////////////////////////////

void SettingsWidget::init(Comms& comms, Settings& settings)
{
    setEnabled(true);
    m_settings = &settings;
    m_db = nullptr;

    m_ui.baseStationsWidget->init(comms, settings);
    m_ui.usersWidget->init(settings);
}

//////////////////////////////////////////////////////////////////////////

void SettingsWidget::shutdown()
{
    setEnabled(false);
    m_settings = nullptr;
    m_db = nullptr;

//    m_ui.baseStationsWidget->shutdown();
    m_ui.usersWidget->shutdown();
}

//////////////////////////////////////////////////////////////////////////

void SettingsWidget::initDB(DB& db)
{
    m_db = &db;
    m_ui.reportsWidget->init(*m_settings, db);
}

//////////////////////////////////////////////////////////////////////////

void SettingsWidget::shutdownDB()
{
    m_db = nullptr;

    m_ui.reportsWidget->shutdown();
}

//////////////////////////////////////////////////////////////////////////

