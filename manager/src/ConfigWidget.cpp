#include "ConfigWidget.h"

//////////////////////////////////////////////////////////////////////////

ConfigWidget::ConfigWidget(QWidget *parent)
    : QWidget(parent)
{
    m_ui.setupUi(this);
    setEnabled(false);

    QObject::connect(m_ui.reset, &QPushButton::released, this, &ConfigWidget::resetConfig);
    QObject::connect(m_ui.apply, &QPushButton::released, this, &ConfigWidget::applyConfig);
}

//////////////////////////////////////////////////////////////////////////

ConfigWidget::~ConfigWidget()
{
    shutdown();
}

//////////////////////////////////////////////////////////////////////////

void ConfigWidget::init(DB& db)
{
    setEnabled(true);
    m_db = &db;

    QObject::connect(m_db, &DB::configChanged, this, &ConfigWidget::configChanged);
}

//////////////////////////////////////////////////////////////////////////

void ConfigWidget::shutdown()
{
    setEnabled(false);
    m_db = nullptr;
    showConfig(DB::Config());
}

//////////////////////////////////////////////////////////////////////////

void ConfigWidget::configChanged()
{
    m_receivedConfig = m_db->getConfig();
    showConfig(m_receivedConfig);
}

//////////////////////////////////////////////////////////////////////////

void ConfigWidget::showConfig(DB::Config const& config)
{
    m_ui.measurementPeriod->setValue(std::chrono::duration<float>(config.descriptor.measurementPeriod).count() / 60.f);
    m_ui.commsPeriod->setValue(std::chrono::duration<float>(config.descriptor.commsPeriod).count() / 60.f);
    m_ui.computedCommsPeriod->setValue(std::chrono::duration<float>(config.descriptor.computedCommsPeriod).count() / 60.f);
    m_ui.sensorsSleeping->setChecked(config.descriptor.sensorsSleeping);
}

//////////////////////////////////////////////////////////////////////////

void ConfigWidget::resetConfig()
{
    m_crtConfig = m_receivedConfig;
    showConfig(m_receivedConfig);
}

//////////////////////////////////////////////////////////////////////////

void ConfigWidget::applyConfig()
{
//    showConfig(m_receivedConfig);
}

//////////////////////////////////////////////////////////////////////////

