#include "SensorSettingsDialog.h"

//////////////////////////////////////////////////////////////////////////

SensorSettingsDialog::SensorSettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    m_ui.setupUi(this);
}

//////////////////////////////////////////////////////////////////////////

SensorSettingsDialog::~SensorSettingsDialog()
{
}

//////////////////////////////////////////////////////////////////////////

void SensorSettingsDialog::setSensorSettings(DB::SensorSettings const& settings)
{
    m_settings = settings;

    m_ui.name->setText(settings.descriptor.name.c_str());
    m_ui.measurementPeriod->setValue(std::chrono::duration<float>(settings.descriptor.measurementPeriod).count() / 60.f);
    m_ui.commsPeriod->setValue(std::chrono::duration<float>(settings.descriptor.commsPeriod).count() / 60.f);
    m_ui.computedCommsPeriod->setValue(std::chrono::duration<float>(settings.computedCommsPeriod).count() / 60.f);
    m_ui.sensorsSleeping->setChecked(settings.descriptor.sensorsSleeping);
}

//////////////////////////////////////////////////////////////////////////

DB::SensorSettings const& SensorSettingsDialog::getSensorSettings() const
{
    return m_settings;
}

//////////////////////////////////////////////////////////////////////////

void SensorSettingsDialog::accept()
{
    DB::SensorSettingsDescriptor descriptor;

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

    m_settings.descriptor = descriptor;
    QDialog::accept();
}
