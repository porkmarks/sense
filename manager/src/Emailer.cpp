#include "Emailer.h"
#include <iostream>
#include <cassert>
#include <sstream>
#include "Smtp/SmtpMime"
#include "Logger.h"
#include "Utils.h"
#include "DB.h"
#include "mimeattachment.h"

extern Logger s_logger;

//////////////////////////////////////////////////////////////////////////

Emailer::Emailer(DB& db)
    : m_db(db)
{
    m_emailThread = std::thread(std::bind(&Emailer::emailThreadProc, this));

    connect(&m_db, &DB::alarmSensorTriggersChanged, this, &Emailer::alarmSensorTriggersChanged, Qt::QueuedConnection);
    connect(&m_db, &DB::alarmBaseStationTriggersChanged, this, &Emailer::alarmBaseStationTriggersChanged, Qt::QueuedConnection);
	connect(&m_db, &DB::alarmStillTriggered, this, &Emailer::alarmStillTriggered, Qt::QueuedConnection);
	connect(&m_db, &DB::reportTriggered, this, &Emailer::reportTriggered, Qt::QueuedConnection);
}

//////////////////////////////////////////////////////////////////////////

Emailer::~Emailer()
{
	{
		std::unique_lock<std::mutex> lg(m_emailMutex);
		m_threadsExit = true;
	}
    m_emailCV.notify_all();
 
    if (m_emailThread.joinable())
        m_emailThread.join();
}

//////////////////////////////////////////////////////////////////////////

void Emailer::reportTriggered(DB::ReportId reportId, IClock::time_point from, IClock::time_point to)
{
    std::optional<DB::Report> report = m_db.findReportById(reportId);
    if (!report.has_value())
	{
		assert(false);
		return;
	}

    s_logger.logInfo(QString("Report '%1' triggered").arg(report->descriptor.name.c_str()));
    sendReportEmail(*report, from, to);
}

//////////////////////////////////////////////////////////////////////////

void Emailer::alarmStillTriggered(DB::AlarmId alarmId)
{
    std::optional<DB::Alarm> alarm = m_db.findAlarmById(alarmId);
    if (!alarm.has_value())
	{
		assert(false);
		return;
	}

    if (alarm->descriptor.sendEmailAction)
        sendAlarmRetriggerEmail(*alarm);
}

//////////////////////////////////////////////////////////////////////////

void Emailer::alarmSensorTriggersChanged(DB::AlarmId alarmId, DB::SensorId sensorId, std::optional<DB::Measurement> measurement, uint32_t oldTriggers, DB::AlarmTriggers triggers)
{
	std::optional<DB::Alarm> alarm = m_db.findAlarmById(alarmId);
	std::optional<DB::Sensor> sensor = m_db.findSensorById(sensorId);
	if (!alarm.has_value() || !sensor.has_value())
	{
		assert(false);
		return;
	}

	if (alarm->descriptor.sendEmailAction)
	{
		if (triggers.removed != 0)
			sendSensorAlarmEmail(*alarm, *sensor, measurement, oldTriggers, triggers.current, triggers.removed, Action::Recovery);
	
        if (triggers.added != 0)
			sendSensorAlarmEmail(*alarm, *sensor, measurement, oldTriggers, triggers.current, triggers.added, Action::Trigger);
	}
}

//////////////////////////////////////////////////////////////////////////

void Emailer::sendSensorAlarmEmail(DB::Alarm const& alarm, DB::Sensor const& sensor, std::optional<DB::Measurement> measurement, uint32_t oldTriggers, uint32_t newTriggers, uint32_t triggers, Action action)
{
    Email email;

    if (measurement.has_value())
	{
		if (action == Action::Trigger)
			email.subject = "SENSE - Sensor '" + sensor.descriptor.name + "' THRESHOLD ALERT for '" + alarm.descriptor.name + "'";
		else
			email.subject = "SENSE - Sensor '" + sensor.descriptor.name + "' THRESHOLD RECOVERY for '" + alarm.descriptor.name + "'";

		const char* alertTemplateStr = R"X(<table style="border-style: solid; border-width: 2px; border-color: #%1;"><tbody><tr><td><strong>%2%3%4</strong></td></tr></tbody></table>)X";

		QString temperatureStr;
		QString humidityStr;
		QString batteryStr;
		QString signalStrengthStr;
        QString commsStr;

        DB::Measurement const& m = *measurement;
		if (newTriggers & DB::AlarmTrigger::MeasurementTemperatureMask)
		{
			if (newTriggers & DB::AlarmTrigger::MeasurementHighTemperatureHard)
				temperatureStr = QString(alertTemplateStr).arg(utils::k_highThresholdHardColor & 0xFFFFFF, 6, 16, QChar('0')).arg("Very High: ").arg(m.descriptor.temperature, 0, 'f', 1).arg("&deg;C");
			else if (newTriggers & DB::AlarmTrigger::MeasurementHighTemperatureSoft)
				temperatureStr = QString(alertTemplateStr).arg(utils::k_highThresholdSoftColor & 0xFFFFFF, 6, 16, QChar('0')).arg("High: ").arg(m.descriptor.temperature, 0, 'f', 1).arg("&deg;C");
			else if (newTriggers & DB::AlarmTrigger::MeasurementLowTemperatureHard)
				temperatureStr = QString(alertTemplateStr).arg(utils::k_lowThresholdHardColor & 0xFFFFFF, 6, 16, QChar('0')).arg("Very Low: ").arg(m.descriptor.temperature, 0, 'f', 1).arg("&deg;C");
			else if (newTriggers & DB::AlarmTrigger::MeasurementLowTemperatureSoft)
				temperatureStr = QString(alertTemplateStr).arg(utils::k_lowThresholdSoftColor & 0xFFFFFF, 6, 16, QChar('0')).arg("Low: ").arg(m.descriptor.temperature, 0, 'f', 1).arg("&deg;C");
		}
		else if (oldTriggers & DB::AlarmTrigger::MeasurementTemperatureMask)
			temperatureStr = QString(alertTemplateStr).arg(utils::k_inRangeColor & 0xFFFFFF, 6, 16, QChar('0')).arg("Recovered: ").arg(m.descriptor.temperature, 0, 'f', 1).arg("&deg;C");
		else
			temperatureStr = QString(R"X(%1&deg;C)X").arg(m.descriptor.temperature, 0, 'f', 1);

		if (newTriggers & DB::AlarmTrigger::MeasurementHumidityMask)
		{
			if (newTriggers & DB::AlarmTrigger::MeasurementHighHumidityHard)
				humidityStr = QString(alertTemplateStr).arg(utils::k_highThresholdHardColor & 0xFFFFFF, 6, 16, QChar('0')).arg("Very High: ").arg(m.descriptor.humidity, 0, 'f', 1).arg("%");
			else if (newTriggers & DB::AlarmTrigger::MeasurementHighHumiditySoft)
				humidityStr = QString(alertTemplateStr).arg(utils::k_highThresholdSoftColor & 0xFFFFFF, 6, 16, QChar('0')).arg("High: ").arg(m.descriptor.humidity, 0, 'f', 1).arg("%");
			else if (newTriggers & DB::AlarmTrigger::MeasurementLowHumidityHard)
				humidityStr = QString(alertTemplateStr).arg(utils::k_lowThresholdHardColor & 0xFFFFFF, 6, 16, QChar('0')).arg("Very Low: ").arg(m.descriptor.humidity, 0, 'f', 1).arg("%");
			else if (newTriggers & DB::AlarmTrigger::MeasurementLowHumiditySoft)
				humidityStr = QString(alertTemplateStr).arg(utils::k_lowThresholdSoftColor & 0xFFFFFF, 6, 16, QChar('0')).arg("Low: ").arg(m.descriptor.humidity, 0, 'f', 1).arg("%");
		}
		else if (oldTriggers & DB::AlarmTrigger::MeasurementHumidityMask)
			humidityStr = QString(alertTemplateStr).arg(utils::k_inRangeColor & 0xFFFFFF, 6, 16, QChar('0')).arg("Recovered: ").arg(m.descriptor.humidity, 0, 'f', 1).arg("%");
		else
			humidityStr = QString(R"X(%1&deg;C)X").arg(m.descriptor.humidity, 0, 'f', 1);

		int batteryPercentage = static_cast<int>(utils::getBatteryLevel(m.descriptor.vcc) * 100.f);
		if (newTriggers & DB::AlarmTrigger::MeasurementLowVcc)
			batteryStr = QString(alertTemplateStr).arg(utils::k_highThresholdHardColor & 0xFFFFFF, 6, 16, QChar('0')).arg("Very Low: ").arg(batteryPercentage).arg("%");
		else if (oldTriggers & DB::AlarmTrigger::MeasurementLowVcc)
			batteryStr = QString(alertTemplateStr).arg(utils::k_inRangeColor & 0xFFFFFF, 6, 16, QChar('0')).arg("Recovered: ").arg(batteryPercentage).arg("%");
		else
			batteryStr = QString(R"X(%1%)X").arg(batteryPercentage);

		int signalStrengthPercentage = static_cast<int>(utils::getSignalLevel(std::min(m.descriptor.signalStrength.s2b, m.descriptor.signalStrength.b2s)) * 100.f);
		if (newTriggers & DB::AlarmTrigger::MeasurementLowSignal)
			signalStrengthStr = QString(alertTemplateStr).arg(utils::k_highThresholdHardColor & 0xFFFFFF, 6, 16, QChar('0')).arg("Very Low: ").arg(signalStrengthPercentage).arg("%");
		else if (oldTriggers & DB::AlarmTrigger::MeasurementLowSignal)
			signalStrengthStr = QString(alertTemplateStr).arg(utils::k_inRangeColor & 0xFFFFFF, 6, 16, QChar('0')).arg("Recovered: ").arg(signalStrengthPercentage).arg("%");
		else
			signalStrengthStr = QString(R"X(%1%)X").arg(signalStrengthPercentage);

        if (newTriggers & DB::AlarmTrigger::SensorBlackout)
            commsStr = QString(alertTemplateStr).arg(utils::k_highThresholdHardColor & 0xFFFFFF, 6, 16, QChar('0'))
                                                .arg("Blackout: ")
                                                .arg(utils::toString<IClock>(sensor.lastCommsTimePoint, m_db.getGeneralSettings().dateTimeFormat))
                                                .arg((" (" + utils::computeRelativeTimePointString(sensor.lastCommsTimePoint).first + " ago)").c_str());
		else if (oldTriggers & DB::AlarmTrigger::SensorBlackout)
            commsStr = QString(alertTemplateStr).arg(utils::k_inRangeColor & 0xFFFFFF, 6, 16, QChar('0')).arg("Recovered from blackout").arg("").arg("");
        else
            commsStr = QString(R"X(Normal)X");

		email.body += QString(R"X(
<table>
<tbody>
<tr><td><strong>Sensor:</strong></td>           <td>%1</td></tr>
<tr><td><strong>S/N:</strong></td>              <td>%2</td></tr>
<tr><td><strong>Measurement Time:</strong></td> <td>%3</td></tr>
<tr><td><strong>Received Time:</strong></td>    <td>%4</td></tr>
<tr><td><strong>Temperature:</strong></td>      <td>%5</td></tr>
<tr><td><strong>Humidity:</strong></td>         <td>%6</td></tr>
<tr><td><strong>Battery:</strong></td>          <td>%7</td></tr>
<tr><td><strong>Signal Strength:</strong></td>  <td>%8</td></tr>
<tr><td><strong>Comms:</strong></td>            <td>%9</td></tr>
</tbody>
</table>
    )X").arg(sensor.descriptor.name.c_str())
			.arg(sensor.serialNumber, 8, 16, QChar('0'))
			.arg(utils::toString<IClock>(m.timePoint, m_db.getGeneralSettings().dateTimeFormat))
			.arg(utils::toString<IClock>(m.receivedTimePoint, m_db.getGeneralSettings().dateTimeFormat))
			.arg(temperatureStr)
			.arg(humidityStr)
			.arg(batteryStr)
			.arg(signalStrengthStr)
            .arg(commsStr)
			.toUtf8().data();


		if (alarm.descriptor.lowTemperatureWatch || alarm.descriptor.highTemperatureWatch)
		{
			QString lowTemperatureStr;
			if (alarm.descriptor.lowTemperatureWatch)
				lowTemperatureStr = QString(R"X(<span style="color: #011fff;">%1</span>&nbsp;|&nbsp;<span style="color: #004a96;">%2</span>&nbsp;|&nbsp;)X").arg(alarm.descriptor.lowTemperatureHard, 0, 'f', 1).arg(alarm.descriptor.lowTemperatureSoft, 0, 'f', 1);
			
            QString highTemperatureStr;
			if (alarm.descriptor.highTemperatureWatch)
				highTemperatureStr = QString(R"X(&nbsp;|&nbsp;<span style="color: #f37736;">%1</span>&nbsp;|&nbsp;<span style="color: #ff2015;">%2</span>)X").arg(alarm.descriptor.highTemperatureSoft, 0, 'f', 1).arg(alarm.descriptor.highTemperatureHard, 0, 'f', 1);
			
            email.body += QString(R"X(<p><strong>Temperature Thresholds&nbsp;(&deg;C):&nbsp;%1<span style="color: #339966;">OK</span>%2</strong></p>)X").arg(lowTemperatureStr).arg(highTemperatureStr).toUtf8().data();
		}
		if (alarm.descriptor.lowHumidityWatch || alarm.descriptor.highHumidityWatch)
		{
			QString lowHumidityStr;
			if (alarm.descriptor.lowHumidityWatch)
				lowHumidityStr = QString(R"X(<span style="color: #011fff;">%1</span>&nbsp;|&nbsp;<span style="color: #004a96;">%2</span>&nbsp;|&nbsp;)X").arg(alarm.descriptor.lowHumidityHard, 0, 'f', 1).arg(alarm.descriptor.lowHumiditySoft, 0, 'f', 1);
			
            QString highHumidityStr;
			if (alarm.descriptor.highHumidityWatch)
				highHumidityStr = QString(R"X(&nbsp;|&nbsp;<span style="color: #f37736;">%1</span>&nbsp;|&nbsp;<span style="color: #ff2015;">%2</span>)X").arg(alarm.descriptor.highHumiditySoft, 0, 'f', 1).arg(alarm.descriptor.highHumidityHard, 0, 'f', 1);
			
            email.body += QString(R"X(<p><strong>Humidity Thresholds&nbsp;(%RH):&nbsp;%1<span style="color: #339966;">OK</span>%2</strong></p>)X").arg(lowHumidityStr).arg(highHumidityStr).toUtf8().data();
		}
		if (alarm.descriptor.lowVccWatch)
		{
			email.body += QString(R"X(<p><strong>Battery Threshold&nbsp;(%):&nbsp;<span style="color: #ff2015;">%1</span>&nbsp;|&nbsp;<span style="color: #339966;">OK</span></strong></p>)X")
				.arg(static_cast<int>(m_db.getSensorSettings().alertBatteryLevel * 100.f))
				.toUtf8().data();
		}
		if (alarm.descriptor.lowSignalWatch)
		{
			email.body += QString(R"X(<p><strong>Signal Strength Threshold&nbsp;(%):&nbsp;<span style="color: #ff2015;">%1</span>&nbsp;|&nbsp;<span style="color: #339966;">OK</span></strong></p>)X")
				.arg(static_cast<int>(m_db.getSensorSettings().alertSignalStrengthLevel * 100.f))
				.toUtf8().data();
		}

		QString dateTimeFormatStr = utils::getQDateTimeFormatString(m_db.getGeneralSettings().dateTimeFormat);
		email.body += QString(R"X(
<p>&nbsp;</p>
<p><strong>Date/Time Format: %1</strong></p>
<p>&nbsp;</p>)X").arg(dateTimeFormatStr.toUpper()).toUtf8().data();
    }
	else
	{
	    if (action == Action::Trigger)
		    email.subject = "SENSE - Sensor '" + sensor.descriptor.name + "' TRIGGER for '" + alarm.descriptor.name + "'";
	    else
		    email.subject = "SENSE - Sensor '" + sensor.descriptor.name + "' RECOVERY for '" + alarm.descriptor.name + "'";

		const char* alertTemplateStr = R"X(<table style="border-style: solid; border-width: 2px; border-color: #%1;"><tbody><tr><td><strong>%2%3%4</strong></td></tr></tbody></table>)X";

		QString commsStr;

        if (newTriggers & DB::AlarmTrigger::SensorBlackout)
            commsStr = QString(alertTemplateStr).arg(utils::k_highThresholdHardColor & 0xFFFFFF, 6, 16, QChar('0'))
                                                .arg("Blackout: ")
                                                .arg(utils::toString<IClock>(sensor.lastCommsTimePoint, m_db.getGeneralSettings().dateTimeFormat))
                                                .arg((" (" + utils::computeRelativeTimePointString(sensor.lastCommsTimePoint).first + " ago)").c_str());
		else if (oldTriggers & DB::AlarmTrigger::SensorBlackout)
			commsStr = QString(alertTemplateStr).arg(utils::k_inRangeColor & 0xFFFFFF, 6, 16, QChar('0')).arg("Recovered from blackout").arg("").arg("");
		else
			commsStr = QString(R"X(Normal)X");

		email.body += QString(R"X(
<table>
<tbody>
<tr><td><strong>Sensor:</strong></td>           <td>%1</td></tr>
<tr><td><strong>S/N:</strong></td>              <td>%2</td></tr>
<tr><td><strong>Comms:</strong></td>            <td>%3</td></tr>
</tbody>
</table>
    )X").arg(sensor.descriptor.name.c_str())
			.arg(sensor.serialNumber, 8, 16, QChar('0'))
			.arg(commsStr)
			.toUtf8().data();

		QString dateTimeFormatStr = utils::getQDateTimeFormatString(m_db.getGeneralSettings().dateTimeFormat);
		email.body += QString(R"X(
<p>&nbsp;</p>
<p><strong>Date/Time Format: %1</strong></p>
<p>&nbsp;</p>)X").arg(dateTimeFormatStr.toUpper()).toUtf8().data();
	}

	email.settings = m_db.getEmailSettings();

	s_logger.logInfo(QString("Sending sensor alarm email"));

    sendEmail(email);
}

//////////////////////////////////////////////////////////////////////////

void Emailer::alarmBaseStationTriggersChanged(DB::AlarmId alarmId, DB::BaseStationId baseStationId, uint32_t oldTriggers, DB::AlarmTriggers triggers)
{
	std::optional<DB::Alarm> alarm = m_db.findAlarmById(alarmId);
	std::optional<DB::BaseStation> bs = m_db.findBaseStationById(baseStationId);
	if (!alarm.has_value() || !bs.has_value())
	{
		assert(false);
		return;
	}

	if (alarm->descriptor.sendEmailAction)
	{
		if (triggers.removed != 0)
			sendBaseStationAlarmEmail(*alarm, *bs, oldTriggers, triggers.current, triggers.removed, Action::Recovery);
		if (triggers.added != 0)
			sendBaseStationAlarmEmail(*alarm, *bs, oldTriggers, triggers.current, triggers.added, Action::Trigger);
	}
}

//////////////////////////////////////////////////////////////////////////

void Emailer::sendBaseStationAlarmEmail(DB::Alarm const& alarm, DB::BaseStation const& bs, uint32_t oldTriggers, uint32_t newTriggers, uint32_t triggers, Action action)
{
	Email email;

	if (action == Action::Trigger)
		email.subject = "SENSE - Base Station '" + bs.descriptor.name + "' TRIGGER for '" + alarm.descriptor.name + "'";
	else
		email.subject = "SENSE - Base Station '" + bs.descriptor.name + "' RECOVERY for '" + alarm.descriptor.name + "'";

	const char* alertTemplateStr = R"X(<table style="border-style: solid; border-width: 2px; border-color: #%1;"><tbody><tr><td><strong>%2%3%4</strong></td></tr></tbody></table>)X";

	QString commsStr;

    if (newTriggers & DB::AlarmTrigger::BaseStationDisconnected)
        commsStr = QString(alertTemplateStr).arg(utils::k_highThresholdHardColor & 0xFFFFFF, 6, 16, QChar('0'))
                                            .arg("Disconnected: ")
                                            .arg(utils::toString<IClock>(bs.lastCommsTimePoint, m_db.getGeneralSettings().dateTimeFormat))
                                            .arg((" (" + utils::computeRelativeTimePointString(bs.lastCommsTimePoint).first + " ago)").c_str());
	else if (oldTriggers & DB::AlarmTrigger::BaseStationDisconnected)
		commsStr = QString(alertTemplateStr).arg(utils::k_inRangeColor & 0xFFFFFF, 6, 16, QChar('0')).arg("Reconnected").arg("").arg("");
	else
		commsStr = QString(R"X(Normal)X");

	email.body += QString(R"X(
<table>
<tbody>
<tr><td><strong>Base Station:</strong></td>     <td>%1</td></tr>
<tr><td><strong>Comms:</strong></td>            <td>%2</td></tr>
</tbody>
</table>
)X").arg(bs.descriptor.name.c_str())
		.arg(commsStr)
		.toUtf8().data();

	QString dateTimeFormatStr = utils::getQDateTimeFormatString(m_db.getGeneralSettings().dateTimeFormat);
	email.body += QString(R"X(
<p>&nbsp;</p>
<p><strong>Date/Time Format: %1</strong></p>
<p>&nbsp;</p>)X").arg(dateTimeFormatStr.toUpper()).toUtf8().data();

	email.settings = m_db.getEmailSettings();

	s_logger.logInfo(QString("Sending base station alarm email"));

	sendEmail(email);
}

//////////////////////////////////////////////////////////////////////////

void Emailer::sendAlarmRetriggerEmail(DB::Alarm const& alarm)
{
	Email email;

	///////////////////////////////
	// SUBJECT
	email.subject = "ALARM '" + alarm.descriptor.name + "' still triggered";

	///////////////////////////////
	// BODY
	for (auto p : alarm.triggersPerSensor)
	{
        std::optional<DB::Sensor> sensor = m_db.findSensorById(p.first);
        if (sensor.has_value())
            email.body += "\n<p>Sensor '<strong>" + sensor->descriptor.name + "</strong>': " + utils::sensorTriggersToString(p.second, true) + "</p>";
	}
	for (auto p : alarm.triggersPerBaseStation)
	{
		std::optional<DB::BaseStation> bs = m_db.findBaseStationById(p.first);
		if (bs.has_value())
			email.body += "\n<p>Base Station '<strong>" + bs->descriptor.name + "</strong>': " + utils::baseStationTriggersToString(p.second) + "</p>";
	}

	email.settings = m_db.getEmailSettings();

	s_logger.logInfo(QString("Sending alarm retrigger email"));

	sendEmail(email);
}

//////////////////////////////////////////////////////////////////////////

std::optional<utils::CsvData> Emailer::getCsvData(std::vector<DB::Measurement> const& measurements, size_t index) const
{
	utils::CsvData data;
    data.measurement = measurements[index];
    std::optional<DB::Sensor> sensor = m_db.findSensorById(data.measurement.descriptor.sensorId);
    if (sensor.has_value())
        data.sensor = *sensor;

    return data;
}

//////////////////////////////////////////////////////////////////////////

void Emailer::sendReportEmail(DB::Report const& report, IClock::time_point from, IClock::time_point to)
{
    QString dateTimeFormatStr = utils::getQDateTimeFormatString(m_db.getGeneralSettings().dateTimeFormat);

    Email email;
    email.settings = m_db.getEmailSettings();

    s_logger.logInfo(QString("Sending report email"));

    switch (report.descriptor.period)
    {
    case DB::ReportDescriptor::Period::Daily:
        email.subject = "Daily report: " + report.descriptor.name + "";
        break;
    case DB::ReportDescriptor::Period::Weekly:
        email.subject = "Weekly report: " + report.descriptor.name + "";
        break;
    case DB::ReportDescriptor::Period::Monthly:
        email.subject = "Monthly report: " + report.descriptor.name + "";
        break;
    case DB::ReportDescriptor::Period::Custom:
        email.subject = "Custom report: " + report.descriptor.name + "";
        break;
    }

    QDateTime startDt = QDateTime::fromTime_t(IClock::to_time_t(from));
    QDateTime endDt = QDateTime::fromTime_t(IClock::to_time_t(to));

    DB::Filter filter;
    filter.useTimePointFilter = true;
    filter.timePointFilter.min = from;
    filter.timePointFilter.max = to;
    filter.useSensorFilter = report.descriptor.filterSensors;
    filter.sensorIds = report.descriptor.sensors;
    std::vector<DB::Measurement> measurements = m_db.getFilteredMeasurements(filter);
    std::sort(measurements.begin(), measurements.end(), [](DB::Measurement const& a, DB::Measurement const& b) { return a.timePoint > b.timePoint; });

    email.body += QString(R"X(
                          <html>
                          <head>
                          <style type="text/css">
                          body
                          {
                            line-height: 1.6em;
                          }

                          #hor-minimalist-a
                          {
                              font-family: "Lucida Sans Unicode", "Lucida Grande", Sans-Serif;
                              font-size: 12px;
                              background: #fff;
                              margin: 45px;
                              width: 480px;
                              border-collapse: collapse;
                              text-align: left;
                          }
                          #hor-minimalist-a th
                          {
                              font-size: 14px;
                              font-weight: normal;
                              color: #039;
                              padding: 10px 8px;
                              border-bottom: 2px solid #6678b1;
                          }
                          #hor-minimalist-a td
                          {
                              color: #669;
                              padding: 9px 8px 0px 8px;
                          }
                          #hor-minimalist-a tbody tr:hover td
                          {
                              color: #009;
                          }
                          </style>
                          </head>
                          <body>
                          <p>This is an automated daily report</p>
                          <p>Measurements between <strong>%1</strong> and&nbsp;<strong>%2</strong></p>)X")
                            .arg(startDt.toString(dateTimeFormatStr))
                            .arg(endDt.toString(dateTimeFormatStr))
                            .toUtf8().data();


    //summary
    {
        email.body += QString(R"X(
                              <table id="hor-minimalist-a">
                              <tbody>
                                  <tr>
                                      <th style="width: 80.6667px; text-align: center; white-space: nowrap;"><strong>Sensor</strong></th>
                                      <th style="width: 80.6667px; text-align: center; white-space: nowrap;"><strong>Min Temperature</strong></th>
                                      <th style="width: 80.6667px; text-align: center; white-space: nowrap;"><strong>Max Temperature</strong></th>
                                      <th style="width: 80.6667px; text-align: center; white-space: nowrap;"><strong>Min Humidity</strong></th>
                                      <th style="width: 81.3333px; text-align: center; white-space: nowrap;"><strong>Max Humidity</strong></th>
                                      <th style="width: 81.3333px; text-align: center; white-space: nowrap;"><strong>Alerts</strong></th>
                                  </tr>)X").toUtf8().data();

        struct SensorData
        {
            bool hasData = false;
            std::string name;
            float minTemperature = std::numeric_limits<float>::max();
            float maxTemperature = std::numeric_limits<float>::lowest();
            float minHumidity = std::numeric_limits<float>::max();
            float maxHumidity = std::numeric_limits<float>::lowest();
            DB::AlarmTriggers alarmTriggers;
        };

        std::vector<SensorData> sensorDatas;
        for (DB::Measurement const& m: measurements)
        {
            QDateTime dt;
            dt.setTime_t(IClock::to_time_t(m.timePoint));
            std::string sensorName = "N/A";
            int32_t _sensorIndex = m_db.findSensorIndexById(m.descriptor.sensorId);
            if (_sensorIndex < 0)
                continue;

            size_t sensorIndex = static_cast<size_t>(_sensorIndex);
            if (sensorIndex >= sensorDatas.size())
                sensorDatas.resize(sensorIndex + 1);

            SensorData& sd = sensorDatas[sensorIndex];
            sd.hasData = true;
            sd.name = m_db.getSensor(sensorIndex).descriptor.name;
            sd.maxTemperature = std::max(sd.maxTemperature, m.descriptor.temperature);
            sd.minTemperature = std::min(sd.minTemperature, m.descriptor.temperature);
            sd.maxHumidity = std::max(sd.maxHumidity, m.descriptor.humidity);
            sd.minHumidity = std::min(sd.minHumidity, m.descriptor.humidity);
            sd.alarmTriggers |= m.alarmTriggers;
        }

        for (SensorData const& sd: sensorDatas)
        {
            if (sd.hasData)
            {
                email.body += QString(R"X(
                                      <tr>
                                      <td style="width: 80.6667px;">%1</td>
                                      <td style="width: 80.6667px; text-align: right; white-space: nowrap;">%2 &deg;C</td>
                                      <td style="width: 80.6667px; text-align: right; white-space: nowrap;">%3 &deg;C</td>
                                      <td style="width: 80.6667px; text-align: right; white-space: nowrap;">%4 %</td>
                                      <td style="width: 81.3333px; text-align: right; white-space: nowrap;">%5 %</td>
                                      <td style="width: 81.3333px; text-align: right; white-space: nowrap;">%6</td>
                                      </tr>
                                      )X")
                        .arg(sd.name.c_str())
                        .arg(sd.minTemperature, 0, 'f', 1)
                        .arg(sd.maxTemperature, 0, 'f', 1)
                        .arg(sd.minHumidity, 0, 'f', 1)
                        .arg(sd.maxHumidity, 0, 'f', 1)
                        .arg(sd.alarmTriggers.current == 0 ? "no" : "yes")
                        .toUtf8().data();
            }
            else
            {
                email.body += QString(R"X(
                                      <tr>
                                      <td style="width: 80.6667px;">%1</td>
                                      <td style="width: 80.6667px; text-align: right; white-space: nowrap;">N/A</td>
                                      <td style="width: 80.6667px; text-align: right; white-space: nowrap;">N/A</td>
                                      <td style="width: 80.6667px; text-align: right; white-space: nowrap;">N/A</td>
                                      <td style="width: 81.3333px; text-align: right; white-space: nowrap;">N/A</td>
                                      <td style="width: 81.3333px; text-align: right; white-space: nowrap;">N/A</td>
                                      </tr>
                                      )X")
                        .arg(sd.name.c_str())
                        .toUtf8().data();
            }
        }

        email.body += "</tbody>"
                      "</table>";
    }

    std::stringstream stream;
    utils::exportCsvTo(stream, m_db.getGeneralSettings(), m_db.getCsvSettings(), [&measurements, this](size_t index) { return getCsvData(measurements, index); }, measurements.size(), true);

    email.attachments.push_back({ std::string("report.csv"), stream.str() });

    sendEmail(email);
}

//////////////////////////////////////////////////////////////////////////

void Emailer::sendShutdownEmail()
{
	Email email;
	email.subject = "SENSE - Manager Closed";

	const char* templateStr = R"X(<table style="border-style: solid; border-width: 2px; border-color: #%1;"><tbody><tr><td><strong>%2%3%4</strong></td></tr></tbody></table>)X";

	QString str = QString(templateStr).arg(utils::k_highThresholdHardColor & 0xFFFFFF, 6, 16, QChar('0'))
		.arg("Manager closed on ")
        .arg(utils::toString<IClock>(IClock::rtNow(), m_db.getGeneralSettings().dateTimeFormat))
        .arg(", all measurements will be stored on the sensors until started again");

	email.body += str.toUtf8().data();
	QString dateTimeFormatStr = utils::getQDateTimeFormatString(m_db.getGeneralSettings().dateTimeFormat);
	email.body += QString(R"X(
<p>&nbsp;</p>
<p><strong>Date/Time Format: %1</strong></p>
<p>&nbsp;</p>)X").arg(dateTimeFormatStr.toUpper()).toUtf8().data();

	email.settings = m_db.getEmailSettings();

	s_logger.logInfo(QString("Sending shutdown alarm email"));

	sendEmail(email);
}

//////////////////////////////////////////////////////////////////////////

void Emailer::sendEmail(Email const& email)
{
    {
        std::unique_lock<std::mutex> lg(m_emailMutex);
        m_emails.push_back(email);
    }
    m_emailCV.notify_all();
}

//////////////////////////////////////////////////////////////////////////

bool Emailer::hasPendingEmails() const
{
	std::unique_lock<std::mutex> lg(m_emailMutex);
	return !m_emails.empty();
}

//////////////////////////////////////////////////////////////////////////

void Emailer::sendEmails(std::vector<Email> const& emails)
{
    for (Email const& email: emails)
    {
        if (email.settings.recipients.empty())
        {
            s_logger.logCritical(QString("Failed to send email: no recipients configured"));
            continue;
        }

        SmtpClient::ConnectionType connectionType = SmtpClient::SslConnection;
        switch (email.settings.connection)
        {
        case EmailSettings::Connection::Ssl: connectionType = SmtpClient::SslConnection; break;
        case EmailSettings::Connection::Tcp: connectionType = SmtpClient::TcpConnection; break;
        case EmailSettings::Connection::Tls: connectionType = SmtpClient::TlsConnection; break;
        }

        SmtpClient smtp(QString::fromUtf8(email.settings.host.c_str()), email.settings.port, connectionType);

        std::string errorMsg;
        connect(&smtp, &SmtpClient::smtpError, [&errorMsg, &smtp](SmtpClient::SmtpError error)
        {
            switch (error)
            {
            case SmtpClient::ConnectionTimeoutError: errorMsg = "Connection timeout."; break;
            case SmtpClient::ResponseTimeoutError: errorMsg = "Response timeout."; break;
            case SmtpClient::SendDataTimeoutError: errorMsg = "Send data timeout."; break;
            case SmtpClient::AuthenticationFailedError: errorMsg = "Authentication failed."; break;
            case SmtpClient::ServerError: errorMsg = std::string("Server error: ") + smtp.getResponseMessage().toUtf8().data(); break;
            case SmtpClient::ClientError: errorMsg = std::string("Client error: ") + smtp.getResponseMessage().toUtf8().data(); break;
            default: errorMsg = "Unknown error."; break;
            }
        });

        smtp.setUser(QString::fromUtf8(email.settings.username.c_str()));
        smtp.setPassword(QString::fromUtf8(email.settings.password.c_str()));

        MimeMessage message;

        message.setSender(new EmailAddress(QString::fromUtf8(email.settings.sender.c_str())));
        for (std::string const& recipient: email.settings.recipients)
            message.addRecipient(new EmailAddress(QString::fromUtf8(recipient.c_str())));

        message.setSubject(QString::fromUtf8(email.subject.c_str()));

        MimeHtml body;
        body.setHtml(QString::fromUtf8(email.body.c_str()));

        message.addPart(&body);

        std::vector<MimeAttachment*> attachments;
        for (Email::Attachment const& a: email.attachments)
		{
            MimeAttachment* attachment = new MimeAttachment(QByteArray(a.contents.c_str(), int(a.contents.size())), a.filename.c_str());
            attachments.push_back(attachment);
            message.addPart(attachment);
		}

        if (smtp.connectToHost() && smtp.login() && smtp.sendMail(message))
            s_logger.logInfo(QString("Successfully sent email"));
        else
            s_logger.logCritical(QString("Failed to send email: %2").arg(errorMsg.c_str()));

        smtp.quit();

        for (MimeAttachment* attachment : attachments)
        {
            delete attachment;
        }
    }
}

//////////////////////////////////////////////////////////////////////////

void Emailer::emailThreadProc()
{
    while (!m_threadsExit)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        std::vector<Email> emails;
        {
            //wait for data
            std::unique_lock<std::mutex> lg(m_emailMutex);
            if (m_emails.empty())
                m_emailCV.wait(lg, [this]{ return !m_emails.empty() || m_threadsExit; });

            if (m_threadsExit)
                break;

            emails = m_emails;
            m_emails.clear();
        }

        sendEmails(emails);
    }
}

//////////////////////////////////////////////////////////////////////////
