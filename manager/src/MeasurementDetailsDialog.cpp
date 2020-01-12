#include "MeasurementDetailsDialog.h"
#include <QMessageBox>
#include <QDateTime>
#include <QSettings>
#include "Utils.h"
#include "PermissionsCheck.h"

//////////////////////////////////////////////////////////////////////////

MeasurementDetailsDialog::MeasurementDetailsDialog(DB& db, QWidget* parent)
    : QDialog(parent)
    , m_db(db)
{
    m_ui.setupUi(this);
    adjustSize();
    loadSettings();
}

//////////////////////////////////////////////////////////////////////////

void MeasurementDetailsDialog::loadSettings()
{
    QSettings settings;
    resize(settings.value("MeasurementDetailsDialog/size", size()).toSize());
    QPoint defaultPos = parentWidget()->mapToGlobal(parentWidget()->rect().center()) - QPoint(width() / 2, height() / 2);
    move(settings.value("MeasurementDetailsDialog/pos", defaultPos).toPoint());
}

//////////////////////////////////////////////////////////////////////////

void MeasurementDetailsDialog::saveSettings()
{
    QSettings settings;
    settings.setValue("MeasurementDetailsDialog/size", size());
    settings.setValue("MeasurementDetailsDialog/pos", pos());
}

//////////////////////////////////////////////////////////////////////////

void MeasurementDetailsDialog::closeEvent(QCloseEvent* event)
{
    saveSettings();
    QDialog::closeEvent(event);
}

//////////////////////////////////////////////////////////////////////////

DB::Measurement const& MeasurementDetailsDialog::getMeasurement() const
{
    return m_measurement;
}

//////////////////////////////////////////////////////////////////////////

void MeasurementDetailsDialog::setMeasurement(DB::Measurement const& measurement)
{
    m_measurement = measurement;
    DB::MeasurementDescriptor descriptor = m_measurement.descriptor;

    m_ui.id->setText(QString("%1").arg(m_measurement.id));

    int32_t sensorIndex = m_db.findSensorIndexById(m_measurement.descriptor.sensorId);
    if (sensorIndex >= 0)
	{
        DB::Sensor const& sensor = m_db.getSensor((size_t)sensorIndex);
		m_ui.sensor->setText(QString("%1 (S/N %2)").arg(sensor.descriptor.name.c_str()).arg(sensor.serialNumber, 8, 16, QChar('0')));
	}
    else
    {
		m_ui.sensor->setText("N/A");
    }

    m_ui.timestamp->setText(utils::toString<DB::Clock>(m_measurement.timePoint, m_db.getGeneralSettings().dateTimeFormat));
    m_ui.receivedTimestamp->setText(utils::toString<DB::Clock>(m_measurement.receivedTimePoint, m_db.getGeneralSettings().dateTimeFormat));

	{
		float vcc = m_measurement.descriptor.vcc;
		m_ui.battery->setText(QString("%1% (%2V)").arg(static_cast<int>(utils::getBatteryLevel(vcc) * 100.f)).arg(vcc, 0, 'f', 2));
		m_ui.batteryIcon->setPixmap(utils::getBatteryIcon(m_db.getSensorSettings(), vcc).pixmap(24, 24));

		{
			int signal = static_cast<int>(utils::getSignalLevel(m_measurement.descriptor.signalStrength.b2s) * 100.f);
			m_ui.signalStrengthB2S->setText(QString("%1% (%2 dBm)").arg(signal).arg(m_measurement.descriptor.signalStrength.b2s));
			m_ui.signalStrengthIconB2S->setPixmap(utils::getSignalIcon(m_db.getSensorSettings(), signal).pixmap(24, 24));
		}
		{
			int signal = static_cast<int>(utils::getSignalLevel(m_measurement.descriptor.signalStrength.s2b) * 100.f);
			m_ui.signalStrengthS2B->setText(QString("%1% (%2 dBm)").arg(signal).arg(m_measurement.descriptor.signalStrength.s2b));
			m_ui.signalStrengthIconS2B->setPixmap(utils::getSignalIcon(m_db.getSensorSettings(), signal).pixmap(24, 24));
		}
	}

    if (hasPermission(m_db, DB::UserDescriptor::PermissionChangeMeasurements))
	{
		m_ui.temperatureRW->setValue(m_measurement.descriptor.temperature);
		m_ui.humidityRW->setValue(m_measurement.descriptor.humidity);
		m_ui.temperatureRO->setVisible(false);
		m_ui.humidityRO->setVisible(false);
	}
    else
    {
		m_ui.temperatureRW->setVisible(false);
		m_ui.humidityRW->setVisible(false);
		m_ui.temperatureRO->setText(QString(u8"%1°C").arg(m_measurement.descriptor.temperature, 0, 'f', 1));
		m_ui.humidityRO->setText(QString("%1 %RH").arg(m_measurement.descriptor.humidity, 0, 'f', 1));
    }
}

//////////////////////////////////////////////////////////////////////////

void MeasurementDetailsDialog::done(int result)
{
    saveSettings();
    QDialog::done(result);
}

//////////////////////////////////////////////////////////////////////////

void MeasurementDetailsDialog::accept()
{
    if (hasPermission(m_db, DB::UserDescriptor::PermissionChangeMeasurements))
	{
		m_measurement.descriptor.temperature = static_cast<float>(m_ui.temperatureRW->value());
		m_measurement.descriptor.humidity = static_cast<float>(m_ui.humidityRW->value());
	}

    QDialog::accept();
}

//////////////////////////////////////////////////////////////////////////

