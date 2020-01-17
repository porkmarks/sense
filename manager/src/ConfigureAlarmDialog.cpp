#include "ConfigureAlarmDialog.h"
#include <QMessageBox>
#include <QSettings>

//////////////////////////////////////////////////////////////////////////

ConfigureAlarmDialog::ConfigureAlarmDialog(DB& db, QWidget* parent)
    : QDialog(parent)
    , m_db(db)
{
    m_ui.setupUi(this);
    m_ui.sensorFilter->init(db);

    connect(m_ui.highTemperatureHard, &QDoubleSpinBox::editingFinished, this, [this]() 
    {
        m_ui.highTemperatureSoft->setValue(std::min(m_ui.highTemperatureSoft->value(), m_ui.highTemperatureHard->value()));
		m_ui.lowTemperatureSoft->setValue(std::min(m_ui.lowTemperatureSoft->value(), m_ui.highTemperatureSoft->value() - 1.f));
		m_ui.lowTemperatureHard->setValue(std::min(m_ui.lowTemperatureHard->value(), m_ui.lowTemperatureSoft->value()));
    });
	connect(m_ui.highTemperatureSoft, &QDoubleSpinBox::editingFinished, this, [this]()
	{
        m_ui.highTemperatureHard->setValue(std::max(m_ui.highTemperatureHard->value(), m_ui.highTemperatureSoft->value()));
		m_ui.lowTemperatureSoft->setValue(std::min(m_ui.lowTemperatureSoft->value(), m_ui.highTemperatureSoft->value() - 1.f));
		m_ui.lowTemperatureHard->setValue(std::min(m_ui.lowTemperatureHard->value(), m_ui.lowTemperatureSoft->value()));
	});
	connect(m_ui.lowTemperatureSoft, &QDoubleSpinBox::editingFinished, this, [this]()
	{
		m_ui.lowTemperatureHard->setValue(std::min(m_ui.lowTemperatureHard->value(), m_ui.lowTemperatureSoft->value()));
		m_ui.highTemperatureSoft->setValue(std::max(m_ui.highTemperatureSoft->value(), m_ui.lowTemperatureSoft->value() + 1.f));
		m_ui.highTemperatureHard->setValue(std::max(m_ui.highTemperatureHard->value(), m_ui.highTemperatureSoft->value()));
	});
	connect(m_ui.lowTemperatureHard, &QDoubleSpinBox::editingFinished, this, [this]()
	{
		m_ui.lowTemperatureSoft->setValue(std::max(m_ui.lowTemperatureSoft->value(), m_ui.lowTemperatureHard->value()));
		m_ui.highTemperatureSoft->setValue(std::max(m_ui.highTemperatureSoft->value(), m_ui.lowTemperatureSoft->value() + 1.f));
		m_ui.highTemperatureHard->setValue(std::max(m_ui.highTemperatureHard->value(), m_ui.highTemperatureSoft->value()));
	});

	connect(m_ui.highHumidityHard, &QDoubleSpinBox::editingFinished, this, [this]()
	{
		m_ui.highHumiditySoft->setValue(std::min(m_ui.highHumiditySoft->value(), m_ui.highHumidityHard->value()));
		m_ui.lowHumiditySoft->setValue(std::min(m_ui.lowHumiditySoft->value(), m_ui.highHumiditySoft->value() - 1.f));
		m_ui.lowHumidityHard->setValue(std::min(m_ui.lowHumidityHard->value(), m_ui.lowHumiditySoft->value()));
	});
	connect(m_ui.highHumiditySoft, &QDoubleSpinBox::editingFinished, this, [this]()
	{
		m_ui.highHumidityHard->setValue(std::max(m_ui.highHumidityHard->value(), m_ui.highHumiditySoft->value()));
		m_ui.lowHumiditySoft->setValue(std::min(m_ui.lowHumiditySoft->value(), m_ui.highHumiditySoft->value() - 1.f));
		m_ui.lowHumidityHard->setValue(std::min(m_ui.lowHumidityHard->value(), m_ui.lowHumiditySoft->value()));
	});
	connect(m_ui.lowHumiditySoft, &QDoubleSpinBox::editingFinished, this, [this]()
	{
		m_ui.lowHumidityHard->setValue(std::min(m_ui.lowHumidityHard->value(), m_ui.lowHumiditySoft->value()));
		m_ui.highHumiditySoft->setValue(std::max(m_ui.highHumiditySoft->value(), m_ui.lowHumiditySoft->value() + 1.f));
		m_ui.highHumidityHard->setValue(std::max(m_ui.highHumidityHard->value(), m_ui.highHumiditySoft->value()));
	});
	connect(m_ui.lowHumidityHard, &QDoubleSpinBox::editingFinished, this, [this]()
	{
		m_ui.lowHumiditySoft->setValue(std::max(m_ui.lowHumiditySoft->value(), m_ui.lowHumidityHard->value()));
		m_ui.highHumiditySoft->setValue(std::max(m_ui.highHumiditySoft->value(), m_ui.lowHumiditySoft->value() + 1.f));
		m_ui.highHumidityHard->setValue(std::max(m_ui.highHumidityHard->value(), m_ui.highHumiditySoft->value()));
	});

    adjustSize();
    loadSettings();
}

//////////////////////////////////////////////////////////////////////////

void ConfigureAlarmDialog::loadSettings()
{
    QSettings settings;
    resize(settings.value("ConfigureAlarmDialog/size", size()).toSize());
    QPoint defaultPos = parentWidget()->mapToGlobal(parentWidget()->rect().center()) - QPoint(width() / 2, height() / 2);
    move(settings.value("ConfigureAlarmDialog/pos", defaultPos).toPoint());
}

//////////////////////////////////////////////////////////////////////////

void ConfigureAlarmDialog::saveSettings()
{
    QSettings settings;
    settings.setValue("ConfigureAlarmDialog/size", size());
    settings.setValue("ConfigureAlarmDialog/pos", pos());
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
    m_ui.highTemperatureSoft->setValue(descriptor.highTemperatureSoft);
    m_ui.highTemperatureHard->setValue(descriptor.highTemperatureHard);
    m_ui.lowTemperatureWatch->setChecked(descriptor.lowTemperatureWatch);
    m_ui.lowTemperatureSoft->setValue(descriptor.lowTemperatureSoft);
    m_ui.lowTemperatureHard->setValue(descriptor.lowTemperatureHard);

    m_ui.highHumidityWatch->setChecked(descriptor.highHumidityWatch);
    m_ui.highHumiditySoft->setValue(descriptor.highHumiditySoft);
    m_ui.highHumidityHard->setValue(descriptor.highHumidityHard);
    m_ui.lowHumidityWatch->setChecked(descriptor.lowHumidityWatch);
    m_ui.lowHumiditySoft->setValue(descriptor.lowHumiditySoft);
    m_ui.lowHumidityHard->setValue(descriptor.lowHumidityHard);

    m_ui.lowSignalWatch->setChecked(descriptor.lowSignalWatch);
    m_ui.lowBatteryWatch->setChecked(descriptor.lowVccWatch);

    m_ui.sendEmailAction->setChecked(descriptor.sendEmailAction);
//    m_ui.emailRecipient->setText(descriptor.emailRecipient.c_str());

    m_ui.resendValue->setValue(std::chrono::duration_cast<std::chrono::hours>(descriptor.resendPeriod).count());
}

//////////////////////////////////////////////////////////////////////////

void ConfigureAlarmDialog::done(int result)
{
    saveSettings();
    QDialog::done(result);
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
            DB::Sensor sensor = m_db.getSensor(i);
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
    descriptor.highTemperatureSoft = static_cast<float>(m_ui.highTemperatureSoft->value());
    descriptor.highTemperatureHard = static_cast<float>(m_ui.highTemperatureHard->value());
    descriptor.lowTemperatureWatch = m_ui.lowTemperatureWatch->isChecked();
    descriptor.lowTemperatureSoft = static_cast<float>(m_ui.lowTemperatureSoft->value());
    descriptor.lowTemperatureHard = static_cast<float>(m_ui.lowTemperatureHard->value());

    descriptor.highHumidityWatch = m_ui.highHumidityWatch->isChecked();
    descriptor.highHumiditySoft = static_cast<float>(m_ui.highHumiditySoft->value());
    descriptor.highHumidityHard = static_cast<float>(m_ui.highHumidityHard->value());
    descriptor.lowHumidityWatch = m_ui.lowHumidityWatch->isChecked();
    descriptor.lowHumiditySoft = static_cast<float>(m_ui.lowHumiditySoft->value());
    descriptor.lowHumidityHard = static_cast<float>(m_ui.lowHumidityHard->value());

    descriptor.lowSignalWatch = m_ui.lowSignalWatch->isChecked();
    descriptor.lowVccWatch = m_ui.lowBatteryWatch->isChecked();

    descriptor.sendEmailAction = m_ui.sendEmailAction->isChecked();
//    descriptor.emailRecipient = m_ui.emailRecipient->text().toUtf8().data();
    descriptor.resendPeriod = std::chrono::hours(std::max(m_ui.resendValue->value(), 1));

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

