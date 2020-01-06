#include "SensorDetailsDialog.h"
#include <QMessageBox>
#include <QDateTime>
#include <QSettings>
#include "Utils.h"

extern std::pair<std::string, int32_t> computeRelativeTimePointString(DB::Clock::time_point tp);
extern uint32_t getSensorStorageCapacity(DB::Sensor const& sensor);

//////////////////////////////////////////////////////////////////////////

SensorDetailsDialog::SensorDetailsDialog(DB& db, QWidget* parent)
    : QDialog(parent)
    , m_db(db)
{
    m_ui.setupUi(this);
    adjustSize();
    loadSettings();
}

//////////////////////////////////////////////////////////////////////////

void SensorDetailsDialog::loadSettings()
{
    QSettings settings;
    resize(settings.value("SensorDetailsDialog/size", size()).toSize());
    QPoint defaultPos = parentWidget()->mapToGlobal(parentWidget()->rect().center()) - QPoint(width() / 2, height() / 2);
    move(settings.value("SensorDetailsDialog/pos", defaultPos).toPoint());
}

//////////////////////////////////////////////////////////////////////////

void SensorDetailsDialog::saveSettings()
{
    QSettings settings;
    settings.setValue("SensorDetailsDialog/size", size());
    settings.setValue("SensorDetailsDialog/pos", pos());
}

//////////////////////////////////////////////////////////////////////////

void SensorDetailsDialog::closeEvent(QCloseEvent* event)
{
    saveSettings();
    QDialog::closeEvent(event);
}

//////////////////////////////////////////////////////////////////////////

DB::Sensor const& SensorDetailsDialog::getSensor() const
{
    return m_sensor;
}

//////////////////////////////////////////////////////////////////////////

void SensorDetailsDialog::setSensor(DB::Sensor const& sensor)
{
    m_sensor = sensor;
    DB::SensorDescriptor descriptor = m_sensor.descriptor;

    DB::SensorSettings sensorSettings = m_db.getSensorSettings();
	//DB::SensorTimeConfig sensorTimeConfig = m_db.getLastSensorTimeConfig();

    m_ui.name->setText(descriptor.name.c_str());
    m_ui.serialNumber->setText(QString("%1").arg(sensor.serialNumber, 8, 16, QChar('0')));

    m_ui.sleepState->setVisible(false); //it's set back to true when the checkbox is checked through a signal
    m_ui.shoudSleep->setChecked(m_sensor.shouldSleep);
    if (m_sensor.state == DB::Sensor::State::Unbound)
    {
        m_ui.shoudSleep->setEnabled(false);
    }
    else if (m_sensor.state == DB::Sensor::State::Sleeping)
    {
        QDateTime dt;
        dt.setTime_t(DB::Clock::to_time_t(m_sensor.sleepStateTimePoint));

        auto pair = computeRelativeTimePointString(m_sensor.sleepStateTimePoint);
        std::string str = pair.first;
        str = (pair.second > 0) ? "In " + str : str + " ago";
        m_ui.sleepState->setText(QString("(sleeping since %1, %2)").arg(dt.toString("dd-MM-yyyy HH:mm")).arg(str.c_str()));
    }
    else if (m_sensor.state == DB::Sensor::State::Active)
    {
        m_ui.sleepState->setText(QString("(scheduled to sleep)"));
    }

    if (sensor.isRTMeasurementValid)
    {
		{
			float vcc = sensor.rtMeasurementVcc;
			m_ui.battery->setText(QString("%1% (%2V)").arg(static_cast<int>(utils::getBatteryLevel(vcc) * 100.f)).arg(vcc, 0, 'f', 2));
			m_ui.batteryIcon->setPixmap(utils::getBatteryIcon(m_db.getSensorSettings(), vcc).pixmap(24, 24));
		}

		{
			int8_t signal = static_cast<int8_t>(utils::getSignalLevel(m_sensor.averageSignalStrength.b2s) * 100.f);
			m_ui.signalStrengthB2S->setText(QString("%1% (%2 dBm)").arg(signal).arg(m_sensor.averageSignalStrength.b2s));
			m_ui.signalStrengthIconB2S->setPixmap(utils::getSignalIcon(m_db.getSensorSettings(), signal).pixmap(24, 24));
		}
		{
			int8_t signal = static_cast<int8_t>(utils::getSignalLevel(m_sensor.averageSignalStrength.s2b) * 100.f);
			m_ui.signalStrengthS2B->setText(QString("%1% (%2 dBm)").arg(signal).arg(m_sensor.averageSignalStrength.s2b));
			m_ui.signalStrengthIconS2B->setPixmap(utils::getSignalIcon(m_db.getSensorSettings(), signal).pixmap(24, 24));
		}
    }
    else
    {
        m_ui.battery->setText(QString("N/A"));
        m_ui.signalStrengthS2B->setText(QString("N/A"));
        m_ui.signalStrengthB2S->setText(QString("N/A"));
    }

    m_ui.power->setText(QString("%1 dBm").arg(sensorSettings.radioPower));
    {
        uint32_t capacity = getSensorStorageCapacity(m_sensor);
        if (capacity == 0)
        {
            m_ui.storage->setText(QString("%1").arg(m_sensor.estimatedStoredMeasurementCount));
        }
        else
        {
            float p = float(m_sensor.estimatedStoredMeasurementCount) / float(capacity);
            p = std::max(std::min(p, 1.f), 0.f) * 100.f;
            m_ui.storage->setText(QString("%1 (%2%)").arg(m_sensor.estimatedStoredMeasurementCount).arg(int(p)));
        }
    }


    if (DB::Clock::to_time_t(m_sensor.lastCommsTimePoint) > 0)
    {
        QDateTime dt;
        dt.setTime_t(DB::Clock::to_time_t(m_sensor.lastCommsTimePoint));

        auto pair = computeRelativeTimePointString(m_sensor.lastCommsTimePoint);
        std::string str = pair.first;
        str = (pair.second > 0) ? "In " + str : str + " ago";
        m_ui.lastComms->setText(QString("%1 (%2)").arg(dt.toString("dd-MM-yyyy HH:mm")).arg(str.c_str()));
    }
    else
    {
        m_ui.lastComms->setText(QString("N/A"));
    }

    m_ui.temperatureBias->setValue(m_sensor.calibration.temperatureBias);
    m_ui.humidityBias->setValue(m_sensor.calibration.humidityBias);

    m_ui.sensorType->setText(QString("%1").arg((int)m_sensor.deviceInfo.sensorType));
    m_ui.hardwareVersion->setText(QString("%1").arg((int)m_sensor.deviceInfo.hardwareVersion));
    m_ui.softwareVersion->setText(QString("%1").arg((int)m_sensor.deviceInfo.softwareVersion));

    setupErrorCountersUI();

    connect(m_ui.clearStats, &QPushButton::clicked, [this]()
    {
        Result<void> result = m_db.clearErrorCounters(m_sensor.id);
        Q_ASSERT(result == success);
        m_sensor.errorCounters = DB::ErrorCounters();
        setupErrorCountersUI();
    });
}

//////////////////////////////////////////////////////////////////////////

void SensorDetailsDialog::setupErrorCountersUI()
{
	m_ui.commsErrors->setText(QString("%1").arg(m_sensor.errorCounters.comms));
	m_ui.unknownReboots->setText(QString("%1").arg(m_sensor.errorCounters.unknownReboots));
	m_ui.brownoutReboots->setText(QString("%1").arg(m_sensor.errorCounters.brownoutReboots));
	m_ui.watchdogReboots->setText(QString("%1").arg(m_sensor.errorCounters.watchdogReboots));
	m_ui.resets->setText(QString("%1").arg(m_sensor.errorCounters.resetReboots));
	m_ui.powerOnReboots->setText(QString("%1").arg(m_sensor.errorCounters.powerOnReboots));
}

//////////////////////////////////////////////////////////////////////////

void SensorDetailsDialog::done(int result)
{
    saveSettings();
    QDialog::done(result);
}

//////////////////////////////////////////////////////////////////////////

void SensorDetailsDialog::accept()
{
    DB::SensorDescriptor descriptor;
    descriptor.name = m_ui.name->text().toUtf8().data();

    if (descriptor.name.empty())
    {
        QMessageBox::critical(this, "Error", "You need to specify a name for this alarm.");
        return;
    }

    int32_t sensorIndex = m_db.findSensorIndexByName(descriptor.name);
    if (sensorIndex >= 0 && m_db.getSensor(static_cast<size_t>(sensorIndex)).id != m_sensor.id)
    {
        QMessageBox::critical(this, "Error", QString("Sensor '%1' already exists.").arg(descriptor.name.c_str()));
        return;
    }

    DB::Sensor::Calibration calibration;
    calibration.temperatureBias = static_cast<float>(m_ui.temperatureBias->value());
    calibration.humidityBias = static_cast<float>(m_ui.humidityBias->value());

    m_sensor.calibration = calibration;
    m_sensor.descriptor = descriptor;
    m_sensor.shouldSleep = m_ui.shoudSleep->isChecked();

    QDialog::accept();
}

//////////////////////////////////////////////////////////////////////////

