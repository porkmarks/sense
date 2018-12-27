#include "ConfigureAlarmDialog.h"
#include <QMessageBox>

//////////////////////////////////////////////////////////////////////////

ConfigureAlarmDialog::ConfigureAlarmDialog(DB& db, QWidget* parent)
    : QDialog(parent)
    , m_db(db)
{
    m_ui.setupUi(this);
    m_ui.sensorFilter->init(db);
}

//////////////////////////////////////////////////////////////////////////

DB::Alarm const& ConfigureAlarmDialog::getAlarm() const
{
    return m_alarm;
}

//////////////////////////////////////////////////////////////////////////

void ConfigureAlarmDialog::setAlarm(DB::Alarm const& alarm)
{
    m_alarm = alarm;
    DB::AlarmDescriptor descriptor = m_alarm.descriptor;

    m_ui.name->setText(descriptor.name.c_str());

    m_ui.sensorGroup->setChecked(descriptor.filterSensors);
    if (descriptor.filterSensors)
    {
        size_t sensorCount = m_db.getSensorCount();
        for (size_t i = 0; i < sensorCount; i++)
        {
            m_ui.sensorFilter->getSensorModel().setSensorChecked(m_db.getSensor(i).id, false);
        }

        for (DB::SensorId id: descriptor.sensors)
        {
            m_ui.sensorFilter->getSensorModel().setSensorChecked(id, true);
        }
        if (descriptor.sensors.empty()) //if no sensors were selected, select all
        {
            for (size_t i = 0; i < sensorCount; i++)
            {
                m_ui.sensorFilter->getSensorModel().setSensorChecked(m_db.getSensor(i).id, true);
            }
        }
    }
    else
    {
        size_t sensorCount = m_db.getSensorCount();
        for (size_t i = 0; i < sensorCount; i++)
        {
            m_ui.sensorFilter->getSensorModel().setSensorChecked(m_db.getSensor(i).id, true);
        }
    }

    m_ui.highTemperatureWatch->setChecked(descriptor.highTemperatureWatch);
    m_ui.highTemperature->setValue(descriptor.highTemperature);
    m_ui.lowTemperatureWatch->setChecked(descriptor.lowTemperatureWatch);
    m_ui.lowTemperature->setValue(descriptor.lowTemperature);

    m_ui.highHumidityWatch->setChecked(descriptor.highHumidityWatch);
    m_ui.highHumidity->setValue(descriptor.highHumidity);
    m_ui.lowHumidityWatch->setChecked(descriptor.lowHumidityWatch);
    m_ui.lowHumidity->setValue(descriptor.lowHumidity);

    m_ui.lowSignalWatch->setChecked(descriptor.lowSignalWatch);
    m_ui.lowBatteryWatch->setChecked(descriptor.lowVccWatch);

    m_ui.sendEmailAction->setChecked(descriptor.sendEmailAction);
//    m_ui.emailRecipient->setText(descriptor.emailRecipient.c_str());
}

//////////////////////////////////////////////////////////////////////////

void ConfigureAlarmDialog::accept()
{
    DB::AlarmDescriptor descriptor;
    descriptor.name = m_ui.name->text().toUtf8().data();

    descriptor.filterSensors = m_ui.sensorGroup->isChecked();
    descriptor.sensors.clear();
    if (descriptor.filterSensors)
    {
        size_t sensorCount = m_db.getSensorCount();
        for (size_t i = 0; i < sensorCount; i++)
        {
            DB::Sensor const& sensor = m_db.getSensor(i);
            if (m_ui.sensorFilter->getSensorModel().isSensorChecked(sensor.id))
            {
                descriptor.sensors.insert(sensor.id);
            }
        }
        if (descriptor.sensors.empty()) //if no sensors were selected, select all
        {
            for (size_t i = 0; i < sensorCount; i++)
            {
                descriptor.sensors.insert(m_db.getSensor(i).id);
            }
        }
    }

    descriptor.highTemperatureWatch = m_ui.highTemperatureWatch->isChecked();
    descriptor.highTemperature = static_cast<float>(m_ui.highTemperature->value());
    descriptor.lowTemperatureWatch = m_ui.lowTemperatureWatch->isChecked();
    descriptor.lowTemperature = static_cast<float>(m_ui.lowTemperature->value());

    descriptor.highHumidityWatch = m_ui.highHumidityWatch->isChecked();
    descriptor.highHumidity = static_cast<float>(m_ui.highHumidity->value());
    descriptor.lowHumidityWatch = m_ui.lowHumidityWatch->isChecked();
    descriptor.lowHumidity = static_cast<float>(m_ui.lowHumidity->value());

    descriptor.lowSignalWatch = m_ui.lowSignalWatch->isChecked();
    descriptor.lowVccWatch = m_ui.lowBatteryWatch->isChecked();

    descriptor.sendEmailAction = m_ui.sendEmailAction->isChecked();
//    descriptor.emailRecipient = m_ui.emailRecipient->text().toUtf8().data();

    if (descriptor.name.empty())
    {
        QMessageBox::critical(this, "Error", "You need to specify a name for this alarm.");
        return;
    }
//    if (descriptor.sendEmailAction && descriptor.emailRecipient.empty())
//    {
//        QMessageBox::critical(this, "Error", "You need to specify the email recipient.");
//        return;
//    }

    int32_t alarmIndex = m_db.findAlarmIndexByName(descriptor.name);
    if (alarmIndex >= 0 && m_db.getAlarm(static_cast<size_t>(alarmIndex)).id != m_alarm.id)
    {
        QMessageBox::critical(this, "Error", QString("Alarm '%1' already exists.").arg(descriptor.name.c_str()));
        return;
    }
    if (descriptor.filterSensors && descriptor.sensors.empty())
    {
        QMessageBox::critical(this, "Error", "You need to specify at least one sensor.");
        return;
    }
    if (!descriptor.highTemperatureWatch && !descriptor.lowTemperatureWatch &&
            !descriptor.highHumidityWatch && !descriptor.lowHumidityWatch &&
            !descriptor.lowSignalWatch &&
            !descriptor.lowVccWatch)
    {
        QMessageBox::critical(this, "Error", "You need to specify at least a trigger condition.");
        return;
    }

    m_alarm.descriptor = descriptor;
    QDialog::accept();
}

//////////////////////////////////////////////////////////////////////////

