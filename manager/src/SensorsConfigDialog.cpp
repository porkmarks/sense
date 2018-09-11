#include "SensorsConfigDialog.h"

#include <QMessageBox>

//////////////////////////////////////////////////////////////////////////

SensorsConfigDialog::SensorsConfigDialog(QWidget *parent)
    : QDialog(parent)
{
    m_ui.setupUi(this);
}

//////////////////////////////////////////////////////////////////////////

SensorsConfigDialog::~SensorsConfigDialog()
{
}

//////////////////////////////////////////////////////////////////////////

void SensorsConfigDialog::setSensorsConfig(DB::SensorsConfig const& config)
{
    m_config = config;

    m_ui.name->setText(config.descriptor.name.c_str());
    m_ui.measurementPeriod->setValue(std::chrono::duration<float>(config.descriptor.measurementPeriod).count() / 60.f);
    m_ui.commsPeriod->setValue(std::chrono::duration<float>(config.descriptor.commsPeriod).count() / 60.f);
    m_ui.computedCommsPeriod->setValue(std::chrono::duration<float>(config.computedCommsPeriod).count() / 60.f);
    m_ui.sensorsSleeping->setChecked(config.descriptor.sensorsSleeping);
}

//////////////////////////////////////////////////////////////////////////

DB::SensorsConfig const& SensorsConfigDialog::getSensorsConfig() const
{
    return m_config;
}

//////////////////////////////////////////////////////////////////////////

void SensorsConfigDialog::accept()
{
    DB::SensorsConfigDescriptor descriptor;

    descriptor.name = m_ui.name->text().toUtf8().data();
    descriptor.measurementPeriod = std::chrono::seconds(static_cast<size_t>(m_ui.measurementPeriod->value() * 60.0));
    descriptor.commsPeriod = std::chrono::seconds(static_cast<size_t>(m_ui.commsPeriod->value() * 60.0));
    descriptor.sensorsSleeping = m_ui.sensorsSleeping->isChecked();

    if (descriptor.commsPeriod.count() == 0)
    {
        QMessageBox::critical(this, "Error", "You need to specify a valid comms duration.");
        return;
    }
    if (descriptor.measurementPeriod.count() == 0)
    {
        QMessageBox::critical(this, "Error", "You need to specify a valid measurement duration.");
        return;
    }
    if (descriptor.commsPeriod < descriptor.measurementPeriod)
    {
        QMessageBox::critical(this, "Error", "The comms period cannot be smaller than the measurement period.");
        return;
    }

    m_config.descriptor = descriptor;
    QDialog::accept();
}
