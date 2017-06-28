#include "ConfigWidget.h"

ConfigWidget::ConfigWidget(QWidget *parent)
    : QWidget(parent)
{
    m_ui.setupUi(this);
    setEnabled(false);

    QObject::connect(m_ui.reset, &QPushButton::released, this, &ConfigWidget::resetConfig);
    QObject::connect(m_ui.apply, &QPushButton::released, this, &ConfigWidget::applyConfig);
}

void ConfigWidget::init(Comms& comms)
{
    m_comms = &comms;

    QObject::connect(m_comms, &Comms::configReceived, this, &ConfigWidget::configReceived);
    QObject::connect(m_comms, &Comms::baseStationDisconnected, this, &ConfigWidget::baseStationDisconnected);
}

void ConfigWidget::configReceived(Comms::Config const& config)
{
    m_receivedConfig = config;

    showConfig(config);

    setEnabled(true);
}

void ConfigWidget::baseStationDisconnected(Comms::BaseStation const& bs)
{
    setEnabled(false);
}

void ConfigWidget::showConfig(Comms::Config const& config)
{
    m_ui.measurementPeriod->setValue(std::chrono::duration<float>(config.measurement_period).count() / 60.f);
    m_ui.commsPeriod->setValue(std::chrono::duration<float>(config.comms_period).count() / 60.f);
    m_ui.computedCommsPeriod->setValue(std::chrono::duration<float>(config.computed_comms_period).count() / 60.f);
    m_ui.sensorsSleeping->setChecked(config.sensors_sleeping);
}

void ConfigWidget::resetConfig()
{
    m_crtConfig = m_receivedConfig;
    showConfig(m_receivedConfig);
}

void ConfigWidget::applyConfig()
{
//    showConfig(m_receivedConfig);
}

