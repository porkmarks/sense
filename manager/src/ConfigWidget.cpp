#include "ConfigWidget.h"

//////////////////////////////////////////////////////////////////////////

ConfigWidget::ConfigWidget(QWidget *parent)
    : QWidget(parent)
{
    m_ui.setupUi(this);
    setEnabled(false);

    connect(m_ui.reset, &QPushButton::released, this, &ConfigWidget::resetConfig);
    connect(m_ui.apply, &QPushButton::released, this, &ConfigWidget::applyConfig);
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

    m_receivedConfig = m_db->getConfig();
    showConfig(m_receivedConfig);

    connect(m_db, &DB::configChanged, this, &ConfigWidget::configChanged);
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
    m_ui.name->setText(config.descriptor.name.c_str());
    m_ui.measurementPeriod->setValue(std::chrono::duration<float>(config.descriptor.measurementPeriod).count() / 60.f);
    m_ui.commsPeriod->setValue(std::chrono::duration<float>(config.descriptor.commsPeriod).count() / 60.f);
    m_ui.computedCommsPeriod->setValue(std::chrono::duration<float>(config.computedCommsPeriod).count() / 60.f);
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
    m_crtConfig.descriptor.name = m_ui.name->text().toUtf8().data();
    m_crtConfig.descriptor.measurementPeriod = std::chrono::seconds(static_cast<size_t>(m_ui.measurementPeriod->value() * 60.0));
    m_crtConfig.descriptor.commsPeriod = std::chrono::seconds(static_cast<size_t>(m_ui.commsPeriod->value() * 60.0));
    m_crtConfig.descriptor.sensorsSleeping = m_ui.sensorsSleeping->isChecked();

    if (!m_db->setConfig(m_crtConfig.descriptor))
    {
        QMessageBox::critical(this, "Error", "Failed to set the config.\nCheck the values.");
    }
}

//////////////////////////////////////////////////////////////////////////

