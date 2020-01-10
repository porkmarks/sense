#include "Emailer.h"
#include <iostream>
#include <cassert>
#include "Smtp/SmtpMime"
#include "Settings.h"
#include "Logger.h"
#include "Utils.h"

extern Logger s_logger;

//////////////////////////////////////////////////////////////////////////

Emailer::Emailer(Settings& settings, DB& db)
    : m_settings(settings)
    , m_db(db)
{
    m_emailThread = std::thread(std::bind(&Emailer::emailThreadProc, this));

    connect(&m_db, &DB::alarmTriggersChanged, this, &Emailer::alarmTriggersChanged, Qt::QueuedConnection);
	connect(&m_db, &DB::alarmStillTriggered, this, &Emailer::alarmStillTriggered, Qt::QueuedConnection);

    m_timer.setSingleShot(false);
    m_timer.setInterval(60 * 1000); //every minute
    m_timer.start();
    connect(&m_timer, &QTimer::timeout, this, &Emailer::checkReports);
}

//////////////////////////////////////////////////////////////////////////

Emailer::~Emailer()
{
    m_threadsExit = true;
    m_emailCV.notify_all();
    if (m_emailThread.joinable())
    {
        m_emailThread.join();
    }
}

//////////////////////////////////////////////////////////////////////////

void Emailer::checkReports()
{
    size_t count = m_db.getReportCount();
    for (size_t i = 0; i < count; i++)
    {
        DB::Report const& report = m_db.getReport(i);
        if (m_db.isReportTriggered(report.id))
        {
            s_logger.logInfo(QString("Report '%1' triggered").arg(report.descriptor.name.c_str()));
            sendReportEmail(report);
            m_db.setReportExecuted(report.id);
        }
    }
}

//////////////////////////////////////////////////////////////////////////

void Emailer::alarmTriggersChanged(DB::AlarmId alarmId, DB::Measurement const& m, uint32_t oldTriggers, uint32_t newTriggers, uint32_t addedTriggers, uint32_t removedTriggers)
{
    int32_t alarmIndex = m_db.findAlarmIndexById(alarmId);
    int32_t sensorIndex = m_db.findSensorIndexById(m.descriptor.sensorId);
    if (alarmIndex < 0 || sensorIndex < 0)
    {
        assert(false);
        return;
    }

    DB::Alarm const& alarm = m_db.getAlarm(static_cast<size_t>(alarmIndex));
    DB::Sensor const& sensor = m_db.getSensor(static_cast<size_t>(sensorIndex));

    if (alarm.descriptor.sendEmailAction)
    {
        if (removedTriggers != 0)
        {
            sendAlarmEmail(alarm, sensor, m, oldTriggers, newTriggers, removedTriggers, Action::Recovery);
        }
        if (addedTriggers != 0)
        {
            sendAlarmEmail(alarm, sensor, m, oldTriggers, newTriggers, addedTriggers, Action::Trigger);
        }
    }
}


//////////////////////////////////////////////////////////////////////////

void Emailer::alarmStillTriggered(DB::AlarmId alarmId)
{
	int32_t alarmIndex = m_db.findAlarmIndexById(alarmId);
	if (alarmIndex < 0)
	{
		assert(false);
		return;
	}

	DB::Alarm const& alarm = m_db.getAlarm(static_cast<size_t>(alarmIndex));

	if (alarm.descriptor.sendEmailAction)
	{
		sendAlarmRetriggerEmail(alarm);
	}
}

//////////////////////////////////////////////////////////////////////////

void Emailer::sendAlarmEmail(DB::Alarm const& alarm, DB::Sensor const& sensor, DB::Measurement const& m, uint32_t oldTriggers, uint32_t newTriggers, uint32_t triggers, Action action)
{
    Email email;

    ///////////////////////////////
    // SUBJECT
    if (action == Action::Trigger)
    {
        email.subject = "SENSE - Sensor '" + sensor.descriptor.name + "' THRESHOLD ALERT for '" + alarm.descriptor.name + "'";
    }
    else
    {
        email.subject = "SENSE - Sensor '" + sensor.descriptor.name + "' THRESHOLD RECOVERY for '" + alarm.descriptor.name + "'";
    }

    ///////////////////////////////
    // BODY

    const char* alertTemplateStr = R"X(<table style="border-style: solid; border-width: 2px; border-color: #%1;"><tbody><tr><td><strong>%2%3%4</strong></td></tr></tbody></table>)X";

    QString temperatureStr;
    if (newTriggers & DB::AlarmTrigger::Temperature)
	{
		if (newTriggers & DB::AlarmTrigger::HighTemperatureHard)
			temperatureStr = QString(alertTemplateStr).arg(utils::k_highThresholdHardColor << 8, 0, 16).arg("Very High: ").arg(m.descriptor.temperature, 0, 'f', 1).arg("&deg;C");
		else if (newTriggers & DB::AlarmTrigger::HighTemperatureSoft)
			temperatureStr = QString(alertTemplateStr).arg(utils::k_highThresholdSoftColor << 8, 0, 16).arg("High: ").arg(m.descriptor.temperature, 0, 'f', 1).arg("&deg;C");
		else if (newTriggers & DB::AlarmTrigger::LowTemperatureHard)
			temperatureStr = QString(alertTemplateStr).arg(utils::k_lowThresholdHardColor << 8, 0, 16).arg("Very Low: ").arg(m.descriptor.temperature, 0, 'f', 1).arg("&deg;C");
		else if (newTriggers & DB::AlarmTrigger::LowTemperatureSoft)
			temperatureStr = QString(alertTemplateStr).arg(utils::k_lowThresholdSoftColor << 8, 0, 16).arg("Low: ").arg(m.descriptor.temperature, 0, 'f', 1).arg("&deg;C");
	}
    else if (oldTriggers & DB::AlarmTrigger::Temperature)
		temperatureStr = QString(alertTemplateStr).arg(utils::k_inRangeColor << 8, 0, 16).arg("Recovered: ").arg(m.descriptor.temperature, 0, 'f', 1).arg("&deg;C");
    else 
        temperatureStr = QString(R"X(%1&deg;C)X").arg(m.descriptor.temperature, 0, 'f', 1);

	QString humidityStr;
	if (newTriggers & DB::AlarmTrigger::Humidity)
	{
		if (newTriggers & DB::AlarmTrigger::HighHumidityHard)
            humidityStr = QString(alertTemplateStr).arg(utils::k_highThresholdHardColor << 8, 0, 16).arg("Very High: ").arg(m.descriptor.humidity, 0, 'f', 1).arg("%");
	    else if (newTriggers & DB::AlarmTrigger::HighHumiditySoft)
            humidityStr = QString(alertTemplateStr).arg(utils::k_highThresholdSoftColor << 8, 0, 16).arg("High: ").arg(m.descriptor.humidity, 0, 'f', 1).arg("%");
	    else if (newTriggers & DB::AlarmTrigger::LowHumidityHard)
            humidityStr = QString(alertTemplateStr).arg(utils::k_lowThresholdHardColor << 8, 0, 16).arg("Very Low: ").arg(m.descriptor.humidity, 0, 'f', 1).arg("%");
	    else if (newTriggers & DB::AlarmTrigger::LowHumiditySoft)
            humidityStr = QString(alertTemplateStr).arg(utils::k_lowThresholdSoftColor << 8, 0, 16).arg("Low: ").arg(m.descriptor.humidity, 0, 'f', 1).arg("%");
	}
	else if (oldTriggers & DB::AlarmTrigger::Humidity)
        humidityStr = QString(alertTemplateStr).arg(utils::k_inRangeColor << 8, 0, 16).arg("Recovered: ").arg(m.descriptor.humidity, 0, 'f', 1).arg("%");
	else
        humidityStr = QString(R"X(%1&deg;C)X").arg(m.descriptor.humidity, 0, 'f', 1);

	QString batteryStr;
    int batteryPercentage = static_cast<int>(utils::getBatteryLevel(m.descriptor.vcc) * 100.f);
	if (newTriggers & DB::AlarmTrigger::LowVcc)
        batteryStr = QString(alertTemplateStr).arg(utils::k_highThresholdHardColor << 8, 0, 16).arg("Very Low: ").arg(batteryPercentage).arg("%");
	else if (oldTriggers & DB::AlarmTrigger::LowVcc)
        batteryStr = QString(alertTemplateStr).arg(utils::k_inRangeColor << 8, 0, 16).arg("Recovered: ").arg(batteryPercentage).arg("%");
	else
        batteryStr = QString(R"X(%1%)X").arg(batteryPercentage);

	QString signalStrengthStr;
    int signalStrengthPercentage = static_cast<int>(utils::getSignalLevel(std::min(m.descriptor.signalStrength.s2b, m.descriptor.signalStrength.b2s)) * 100.f);
	if (newTriggers & DB::AlarmTrigger::LowSignal)
        signalStrengthStr = QString(alertTemplateStr).arg(utils::k_highThresholdHardColor << 8, 0, 16).arg("Very Low: ").arg(signalStrengthPercentage).arg("%");
	else if (oldTriggers & DB::AlarmTrigger::LowSignal)
        signalStrengthStr = QString(alertTemplateStr).arg(utils::k_inRangeColor << 8, 0, 16).arg("Recovered: ").arg(signalStrengthPercentage).arg("%");
	else
        signalStrengthStr = QString(R"X(%1%)X").arg(signalStrengthPercentage);

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
</tbody>
</table>
    )X").arg(sensor.descriptor.name.c_str())
        .arg(sensor.serialNumber, 8, 16, QChar('0'))
        .arg(utils::toString<DB::Clock>(m.timePoint, m_settings.getGeneralSettings().dateTimeFormat))
        .arg(utils::toString<DB::Clock>(m.receivedTimePoint, m_settings.getGeneralSettings().dateTimeFormat))
        .arg(temperatureStr)
        .arg(humidityStr)
        .arg(batteryStr)
        .arg(signalStrengthStr)
        .toUtf8().data();


    if (alarm.descriptor.lowTemperatureWatch || alarm.descriptor.highTemperatureWatch)
	{
        QString lowTemperatureStr;
        if (alarm.descriptor.lowTemperatureWatch)
		{
            lowTemperatureStr = QString(R"X(<strong><span style="color: #011fff;">%1</span>&nbsp;|&nbsp;<span style="color: #004a96;">%2</span>&nbsp;|&nbsp;)X").arg(alarm.descriptor.lowTemperatureHard, 0, 'f', 1).arg(alarm.descriptor.lowTemperatureSoft, 0, 'f', 1);
		}
		QString highTemperatureStr;
		if (alarm.descriptor.highTemperatureWatch)
		{
			highTemperatureStr = QString(R"X(&nbsp;|&nbsp;<span style="color: #f37736;">%1</span>&nbsp;|&nbsp;<span style="color: #ff2015;">%2</span>)X").arg(alarm.descriptor.highTemperatureSoft, 0, 'f', 1).arg(alarm.descriptor.highTemperatureHard, 0, 'f', 1);
		}
        email.body += QString(R"X(<p><strong>Temperature Thresholds</strong><strong>&nbsp;(&deg;C):&nbsp;</strong>%1<span style="color: #339966;">OK</span>%2</p>)X").arg(lowTemperatureStr).arg(highTemperatureStr).toUtf8().data();
	}
	if (alarm.descriptor.lowHumidityWatch || alarm.descriptor.highHumidityWatch)
	{
		QString lowHumidityStr;
		if (alarm.descriptor.lowHumidityWatch)
		{
			lowHumidityStr = QString(R"X(<strong><span style="color: #011fff;">%1</span>&nbsp;|&nbsp;<span style="color: #004a96;">%2</span>&nbsp;|&nbsp;)X").arg(alarm.descriptor.lowHumidityHard, 0, 'f', 1).arg(alarm.descriptor.lowHumiditySoft, 0, 'f', 1);
		}
		QString highHumidityStr;
		if (alarm.descriptor.highHumidityWatch)
		{
			highHumidityStr = QString(R"X(&nbsp;|&nbsp;<span style="color: #f37736;">%1</span>&nbsp;|&nbsp;<span style="color: #ff2015;">%2</span>)X").arg(alarm.descriptor.highHumiditySoft, 0, 'f', 1).arg(alarm.descriptor.highHumidityHard, 0, 'f', 1);
		}
		email.body += QString(R"X(<p><strong>Humidity Thresholds</strong><strong>&nbsp;(&deg;C):&nbsp;</strong>%1<span style="color: #339966;">OK</span>%2</p>)X").arg(lowHumidityStr).arg(highHumidityStr).toUtf8().data();
	}
    if (alarm.descriptor.lowVccWatch)
	{
		email.body += QString(R"X(<p><strong>Battery Threshold</strong><strong>&nbsp;(%):&nbsp;</strong><strong><span style="color: #339966;">OK</span>&nbsp;|&nbsp;<span style="color: #ff2015;">%1</span></strong></p>)X")
            .arg(static_cast<int>(m_db.getSensorSettings().alertBatteryLevel * 100.f))
            .toUtf8().data();
	}
	if (alarm.descriptor.lowSignalWatch)
	{
		email.body += QString(R"X(<p><strong>Signal Strength Threshold</strong><strong>&nbsp;(%):&nbsp;</strong><strong><span style="color: #339966;">OK</span>&nbsp;|&nbsp;<span style="color: #ff2015;">%1</span></strong></p>)X")
			.arg(static_cast<int>(m_db.getSensorSettings().alertSignalStrengthLevel * 100.f))
			.toUtf8().data();
	}

	QString dateTimeFormatStr = utils::getQDateTimeFormatString(m_settings.getGeneralSettings().dateTimeFormat);
	email.body += QString(R"X(
<p>&nbsp;</p>
<p><strong>Date/Time Format: %1</strong></p>
<p>&nbsp;</p>)X").arg(dateTimeFormatStr.toUpper()).toUtf8().data();

	email.settings = m_settings.getEmailSettings();

	s_logger.logInfo(QString("Sending alarm email"));

    sendEmail(email);
}

//////////////////////////////////////////////////////////////////////////

void Emailer::sendAlarmRetriggerEmail(DB::Alarm const& alarm)
{
	auto toString = [](uint32_t triggers)
	{
		std::string str;
		str += (triggers & DB::AlarmTrigger::LowTemperatureSoft) ? "Low Temperature, " : "";
		str += (triggers & DB::AlarmTrigger::LowTemperatureHard) ? "Low Temperature, " : "";
		str += (triggers & DB::AlarmTrigger::HighTemperatureSoft) ? "High Temperature, " : "";
		str += (triggers & DB::AlarmTrigger::HighTemperatureHard) ? "High Temperature, " : "";
		str += (triggers & DB::AlarmTrigger::LowHumiditySoft) ? "Low Humidity, " : "";
		str += (triggers & DB::AlarmTrigger::LowHumidityHard) ? "Low Humidity, " : "";
		str += (triggers & DB::AlarmTrigger::HighHumiditySoft) ? "High Humidity, " : "";
		str += (triggers & DB::AlarmTrigger::HighHumidityHard) ? "High Humidity, " : "";
		str += (triggers & DB::AlarmTrigger::LowVcc) ? "Low Battery, " : "";
		str += (triggers & DB::AlarmTrigger::LowSignal) ? "Low Signal, " : "";
		if (!str.empty())
		{
			str.pop_back(); //' '
			str.pop_back(); //','
		}
		return str;
	};

    uint32_t triggers = 0;
    for (auto p: alarm.triggersPerSensor)
    { 
        triggers |= p.second;
    }

	Email email;

	///////////////////////////////
	// SUBJECT
	email.subject = "ALARM '" + alarm.descriptor.name + "' still triggered: " + toString(triggers);

	///////////////////////////////
	// BODY
	for (auto p : alarm.triggersPerSensor)
	{
        DB::SensorId sensorId = p.first;
        int32_t sensorIndex = m_db.findSensorIndexById(sensorId);
        if (sensorIndex >= 0)
		{
            DB::Sensor const& sensor = m_db.getSensor((size_t)sensorIndex);
			email.body += "\n<p>Sensor '<strong>" + sensor.descriptor.name + "</strong>': " + toString(p.second) + "</p>";
		}
	}
	
	email.settings = m_settings.getEmailSettings();

	s_logger.logInfo(QString("Sending alarm email'"));

	sendEmail(email);
}

//////////////////////////////////////////////////////////////////////////

void Emailer::sendReportEmail(DB::Report const& report)
{
    QString dateTimeFormatStr = utils::getQDateTimeFormatString(m_settings.getGeneralSettings().dateTimeFormat);

    Email email;
    email.settings = m_settings.getEmailSettings();

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

    QDateTime startDt;
    startDt.setTime_t(DB::Clock::to_time_t(report.lastTriggeredTimePoint));
    QDateTime endDt = QDateTime::currentDateTime();

    DB::Filter filter;
    filter.useTimePointFilter = true;
    filter.timePointFilter.min = report.lastTriggeredTimePoint;
    filter.timePointFilter.max = DB::Clock::now();
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


    if (report.descriptor.data == DB::ReportDescriptor::Data::Summary ||
            report.descriptor.data == DB::ReportDescriptor::Data::All)
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
            uint32_t alarmTriggers = 0;
        };

        std::vector<SensorData> sensorDatas;
        for (DB::Measurement const& m: measurements)
        {
            QDateTime dt;
            dt.setTime_t(DB::Clock::to_time_t(m.timePoint));
            std::string sensorName = "N/A";
            int32_t _sensorIndex = m_db.findSensorIndexById(m.descriptor.sensorId);
            if (_sensorIndex < 0)
            {
                continue;
            }
            size_t sensorIndex = static_cast<size_t>(_sensorIndex);
            if (sensorIndex >= sensorDatas.size())
            {
                sensorDatas.resize(sensorIndex + 1);
            }
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
                        .arg(sd.alarmTriggers == 0 ? "no" : "yes")
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

    if (report.descriptor.data == DB::ReportDescriptor::Data::All)
    {
        email.body += QString(R"X(
                              <table id="hor-minimalist-a">
                              <tbody>
                                  <tr>
                                      <th style="width: 80.6667px; text-align: center; white-space: nowrap;"><strong>Sensor</strong></th>
                                      <th style="width: 80.6667px; text-align: center; white-space: nowrap;"><strong>Timestamp</strong></th>
                                      <th style="width: 80.6667px; text-align: center; white-space: nowrap;"><strong>Temperature</strong></th>
                                      <th style="width: 80.6667px; text-align: center; white-space: nowrap;"><strong>Humidity</strong></th>
                                      <th style="width: 81.3333px; text-align: center; white-space: nowrap;"><strong>Alerts</strong></th>
                                  </tr>)X").toUtf8().data();

        for (DB::Measurement const& m: measurements)
        {
            QDateTime dt;
            dt.setTime_t(DB::Clock::to_time_t(m.timePoint));
            std::string sensorName = "N/A";
            int32_t sensorIndex = m_db.findSensorIndexById(m.descriptor.sensorId);
            if (sensorIndex >= 0)
            {
                sensorName = m_db.getSensor(static_cast<size_t>(sensorIndex)).descriptor.name;
            }
            email.body += QString(R"X(
                                  <tr>
                                      <td style="width: 80.6667px;">%1</td>
                                      <td style="width: 80.6667px; text-align: right; white-space: nowrap;">%2</td>
                                      <td style="width: 80.6667px; text-align: right; white-space: nowrap;">%3 &deg;C</td>
                                      <td style="width: 80.6667px; text-align: right; white-space: nowrap;">%4 %</td>
                                      <td style="width: 81.3333px; text-align: right; white-space: nowrap;">%5</td>
                                  </tr>)X").arg(sensorName.c_str())
                    .arg(dt.toString(dateTimeFormatStr))
                    .arg(m.descriptor.temperature, 0, 'f', 1)
                    .arg(m.descriptor.humidity, 0, 'f', 1)
                    .arg(m.alarmTriggers == 0 ? "no" : "yes")
                    .toUtf8().data();
        }
        email.body += R"X(
                      </tbody>
                      </table>
                      <p>&nbsp;</p>
                      <p><em>- Sense -</em></p>
                      </body>
                      </html>)X";
    }

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
        {
            message.addRecipient(new EmailAddress(QString::fromUtf8(recipient.c_str())));
        }
        message.setSubject(QString::fromUtf8(email.subject.c_str()));

        MimeHtml body;
        body.setHtml(QString::fromUtf8(email.body.c_str()));

        message.addPart(&body);

        if (smtp.connectToHost() && smtp.login() && smtp.sendMail(message))
        {
            s_logger.logInfo(QString("Successfully sent email"));
        }
        else
        {
            s_logger.logCritical(QString("Failed to send email: %2").arg(errorMsg.c_str()));
        }
        smtp.quit();
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
            {
                m_emailCV.wait(lg, [this]{ return !m_emails.empty() || m_threadsExit; });
            }
            if (m_threadsExit)
            {
                break;
            }

            emails = m_emails;
            m_emails.clear();
        }

        sendEmails(emails);
    }
}

//////////////////////////////////////////////////////////////////////////
