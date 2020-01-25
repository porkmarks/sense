#include "SensorDetailsDialog.h"
#include <QMessageBox>
#include <QDateTime>
#include <QSettings>
#include "Utils.h"

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
        auto pair = utils::computeRelativeTimePointString(m_sensor.sleepStateTimePoint);
        std::string str = pair.first;
        str = (pair.second > 0) ? "In " + str : str + " ago";
		m_ui.sleepState->setText(QString("(sleeping since %1, %2)").arg(utils::toString<IClock>(m_sensor.sleepStateTimePoint, m_db.getGeneralSettings().dateTimeFormat)).arg(str.c_str()));
    }
    else if (m_sensor.state == DB::Sensor::State::Active)
    {
        m_ui.sleepState->setText(QString("(scheduled to sleep)"));
    }

    if (sensor.isRTMeasurementValid)
    {
		{
			float vcc = sensor.rtMeasurementVcc;
			m_ui.battery->setText(QString("%1% (%2V)").arg(utils::getBatteryLevel(vcc) * 100.f, 0, 'f', 2).arg(vcc, 0, 'f', 2));
			m_ui.batteryIcon->setPixmap(utils::getBatteryIcon(m_db.getSensorSettings(), vcc).pixmap(24, 24));
		}

		{
			float signal = utils::getSignalLevel(m_sensor.averageSignalStrength.b2s) * 100.f;
			m_ui.signalStrengthB2S->setText(QString("%1% (%2 dBm)").arg(signal, 0, 'f', 2).arg(m_sensor.averageSignalStrength.b2s));
			m_ui.signalStrengthIconB2S->setPixmap(utils::getSignalIcon(m_db.getSensorSettings(), signal).pixmap(24, 24));
		}
		{
			float signal = utils::getSignalLevel(m_sensor.averageSignalStrength.s2b) * 100.f;
			m_ui.signalStrengthS2B->setText(QString("%1% (%2 dBm)").arg(signal, 0, 'f', 2).arg(m_sensor.averageSignalStrength.s2b));
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
        uint32_t capacity = utils::getSensorStorageCapacity(m_sensor);
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


    if (IClock::to_time_t(m_sensor.lastCommsTimePoint) > 0)
    {
        auto pair = utils::computeRelativeTimePointString(m_sensor.lastCommsTimePoint);
        std::string str = pair.first;
        str = (pair.second > 0) ? "In " + str : str + " ago";

        m_ui.lastComms->setText(QString("%1 (%2)").arg(utils::toString<IClock>(m_sensor.lastCommsTimePoint, m_db.getGeneralSettings().dateTimeFormat)).arg(str.c_str()));
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

	setupStatsUI();

#define RESET_STAT(rb, s, v) \
	connect(m_ui.rb, &QPushButton::clicked, [this]() \
	{ \
		m_sensor.stats.s = v; \
		Result<void> result = m_db.setSensorStats(m_sensor.id, m_sensor.stats); \
		Q_ASSERT(result == success); \
		setupStatsUI(); \
	});

    RESET_STAT(resetCommsBlackouts, commsBlackouts, 0);
    RESET_STAT(resetCommsFailures, commsFailures, 0);
	RESET_STAT(resetUnknownReboots, unknownReboots, 0);
    RESET_STAT(resetBrownoutReboots, brownoutReboots, 0);
    RESET_STAT(resetWatchdogReboots, watchdogReboots, 0);
    RESET_STAT(resetManualReboots, resetReboots, 0);
    RESET_STAT(resetPowerOnReboots, powerOnReboots, 0);
	RESET_STAT(resetCommsRetries, commsRetries, 0);
    RESET_STAT(resetAsleep, asleep, IClock::duration::zero());
    RESET_STAT(resetAwake, awake, IClock::duration::zero());
    RESET_STAT(resetCommsRounds, commsRounds, 0);
    RESET_STAT(resetMeasurementRounds, measurementRounds, 0);
}

//////////////////////////////////////////////////////////////////////////

void SensorDetailsDialog::setupStatsUI()
{
	m_ui.commsBlackouts->setText(QString("%1").arg(m_sensor.stats.commsBlackouts));
	m_ui.commsFailures->setText(QString("%1").arg(m_sensor.stats.commsFailures));
	m_ui.unknownReboots->setText(QString("%1").arg(m_sensor.stats.unknownReboots));
	m_ui.brownoutReboots->setText(QString("%1").arg(m_sensor.stats.brownoutReboots));
	m_ui.watchdogReboots->setText(QString("%1").arg(m_sensor.stats.watchdogReboots));
	m_ui.manualReboots->setText(QString("%1").arg(m_sensor.stats.resetReboots));
	m_ui.powerOnReboots->setText(QString("%1").arg(m_sensor.stats.powerOnReboots));
	m_ui.commsRetries->setText(QString("%1").arg(m_sensor.stats.commsRetries));
	m_ui.asleep->setText(QString("%1").arg(utils::computeDurationString(m_sensor.stats.asleep).first.c_str()));
	m_ui.awake->setText(QString("%1").arg(utils::computeDurationString(m_sensor.stats.awake).first.c_str()));
	m_ui.commsRounds->setText(QString("%1").arg(m_sensor.stats.commsRounds));
	m_ui.measurementRounds->setText(QString("%1").arg(m_sensor.stats.measurementRounds));
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

    std::optional<DB::Sensor> sensor = m_db.findSensorByName(descriptor.name);
    if (sensor.has_value() && sensor->id != m_sensor.id)
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

