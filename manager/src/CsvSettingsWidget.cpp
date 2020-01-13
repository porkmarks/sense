#include "CsvSettingsWidget.h"
#include "ConfigureAlarmDialog.h"
#include <QMessageBox>
#include <QInputDialog>
#include "qftp.h"

#include "DB.h"
#include "Utils.h"
#include <sstream>

//////////////////////////////////////////////////////////////////////////

CsvSettingsWidget::CsvSettingsWidget(QWidget *parent)
    : QWidget(parent)
{
    m_ui.setupUi(this);
}

//////////////////////////////////////////////////////////////////////////

CsvSettingsWidget::~CsvSettingsWidget()
{
}

//////////////////////////////////////////////////////////////////////////

void CsvSettingsWidget::init(DB& db)
{
    m_db = &db;

	connect(m_ui.csvDateTimeFormatOverride, &QCheckBox::stateChanged, this, &CsvSettingsWidget::refreshPreview);
	connect(m_ui.csvDateTimeFormat, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &CsvSettingsWidget::refreshPreview);
	connect(m_ui.csvExportId, &QCheckBox::stateChanged, this, &CsvSettingsWidget::refreshPreview);
	connect(m_ui.csvExportIndex, &QCheckBox::stateChanged, this, &CsvSettingsWidget::refreshPreview);
	connect(m_ui.csvExportSensorName, &QCheckBox::stateChanged, this, &CsvSettingsWidget::refreshPreview);
	connect(m_ui.csvExportSensorSN, &QCheckBox::stateChanged, this, &CsvSettingsWidget::refreshPreview);
	connect(m_ui.csvExportTimePoint, &QCheckBox::stateChanged, this, &CsvSettingsWidget::refreshPreview);
	connect(m_ui.csvExportReceivedTimePoint, &QCheckBox::stateChanged, this, &CsvSettingsWidget::refreshPreview);
	connect(m_ui.csvExportTemperature, &QCheckBox::stateChanged, this, &CsvSettingsWidget::refreshPreview);
	connect(m_ui.csvExportHumidity, &QCheckBox::stateChanged, this, &CsvSettingsWidget::refreshPreview);
	connect(m_ui.csvExportBattery, &QCheckBox::stateChanged, this, &CsvSettingsWidget::refreshPreview);
	connect(m_ui.csvExportSignal, &QCheckBox::stateChanged, this, &CsvSettingsWidget::refreshPreview);
	connect(m_ui.csvUnitFormat, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &CsvSettingsWidget::refreshPreview);
	connect(m_ui.csvDecimalPlaces, static_cast<void(QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &CsvSettingsWidget::refreshPreview);
	connect(m_ui.csvSeparator, &QLineEdit::textChanged, this, &CsvSettingsWidget::refreshPreview);
	connect(m_ui.csvTabSeparator, &QCheckBox::stateChanged, this, &CsvSettingsWidget::refreshPreview);

    setCsvSettings(m_db->getGeneralSettings(), m_db->getCsvSettings());
}

//////////////////////////////////////////////////////////////////////////

void CsvSettingsWidget::setPreviewVisible(bool value)
{
	m_ui.csvPreview->setVisible(value);
}

//////////////////////////////////////////////////////////////////////////

void CsvSettingsWidget::refreshPreview()
{
	std::stringstream stream;
    DB::CsvSettings csvSettings = getCsvSettings();

	m_ui.csvDateTimeFormat->setCurrentIndex((int)(csvSettings.dateTimeFormatOverride.has_value() ? *csvSettings.dateTimeFormatOverride : m_generalSettings.dateTimeFormat));

    static std::vector<utils::CsvData> s_csvData;
    if (s_csvData.empty())
    {
        utils::CsvData data;
        data.measurement.id = 1000;
        data.measurement.timePoint = DB::Clock::now() - std::chrono::hours(24);
        data.measurement.receivedTimePoint = data.measurement.timePoint + std::chrono::minutes(1);
        data.measurement.alarmTriggers = 0;
        data.measurement.descriptor.temperature = 22.f;
        data.measurement.descriptor.humidity = 32.f;
        data.measurement.descriptor.index = 2000;
        data.measurement.descriptor.sensorId = 1;
        data.measurement.descriptor.signalStrength.b2s = -77;
        data.measurement.descriptor.signalStrength.s2b = -78;
        data.measurement.descriptor.vcc = 2.88f;
        data.sensor.address = 1003;
        data.sensor.averageSignalStrength.b2s = -77;
        data.sensor.averageSignalStrength.s2b = -76;
        data.sensor.descriptor.name = "sensor1";
        s_csvData.push_back(data);
        for (size_t i = 0; i < 30; i++)
		{
			data.measurement.id++;
			data.measurement.descriptor.index++;
			data.measurement.timePoint += std::chrono::minutes(15);
			data.measurement.receivedTimePoint = data.measurement.timePoint + std::chrono::minutes(1);
			data.measurement.descriptor.temperature += 1.f;
			data.measurement.descriptor.humidity += 0.5f;
			s_csvData.push_back(data);
		}
    }

    utils::exportCsvTo(stream, m_generalSettings, csvSettings, [](size_t index) { return s_csvData[index]; }, s_csvData.size(), true);

	m_ui.csvPreview->setPlainText((stream.str() + "\n.....\n").c_str());

	emit settingsChanged();
}

//////////////////////////////////////////////////////////////////////////

void CsvSettingsWidget::setCsvSettings(DB::GeneralSettings const& generalSettings, DB::CsvSettings const& settings)
{
    m_generalSettings = generalSettings;
    m_ui.csvDateTimeFormatOverride->setChecked(settings.dateTimeFormatOverride.has_value());
    m_ui.csvDateTimeFormat->setCurrentIndex((int)(settings.dateTimeFormatOverride.has_value() ? *settings.dateTimeFormatOverride : generalSettings.dateTimeFormat));
    m_ui.csvExportId->setChecked(settings.exportId);
	m_ui.csvExportIndex->setChecked(settings.exportIndex);
	m_ui.csvExportSensorName->setChecked(settings.exportSensorName);
	m_ui.csvExportSensorSN->setChecked(settings.exportSensorSN);
	m_ui.csvExportTimePoint->setChecked(settings.exportTimePoint);
	m_ui.csvExportReceivedTimePoint->setChecked(settings.exportReceivedTimePoint);
	m_ui.csvExportTemperature->setChecked(settings.exportTemperature);
	m_ui.csvExportHumidity->setChecked(settings.exportHumidity);
	m_ui.csvExportBattery->setChecked(settings.exportBattery);
	m_ui.csvExportSignal->setChecked(settings.exportSignal);
    m_ui.csvUnitFormat->setCurrentIndex((int)settings.unitsFormat);
    m_ui.csvDecimalPlaces->setValue(settings.decimalPlaces);
    m_ui.csvSeparator->setText(settings.separator.c_str());
    m_ui.csvTabSeparator->setChecked(settings.separator == "\t");
}

//////////////////////////////////////////////////////////////////////////

DB::CsvSettings CsvSettingsWidget::getCsvSettings() const
{
	DB::CsvSettings settings = m_db->getCsvSettings();

    if (m_ui.csvDateTimeFormatOverride->isChecked())
	{
        settings.dateTimeFormatOverride = (DB::DateTimeFormat)m_ui.csvDateTimeFormat->currentIndex();
	}
	settings.exportId = m_ui.csvExportId->isChecked();
	settings.exportIndex = m_ui.csvExportIndex->isChecked();
	settings.exportSensorName = m_ui.csvExportSensorName->isChecked();
	settings.exportSensorSN = m_ui.csvExportSensorSN->isChecked();
	settings.exportTimePoint = m_ui.csvExportTimePoint->isChecked();
	settings.exportReceivedTimePoint = m_ui.csvExportReceivedTimePoint->isChecked();
	settings.exportTemperature = m_ui.csvExportTemperature->isChecked();
	settings.exportHumidity = m_ui.csvExportHumidity->isChecked();
	settings.exportBattery = m_ui.csvExportBattery->isChecked();
	settings.exportSignal = m_ui.csvExportSignal->isChecked();
	settings.unitsFormat = (DB::CsvSettings::UnitsFormat)m_ui.csvUnitFormat->currentIndex();
    settings.decimalPlaces = m_ui.csvDecimalPlaces->value();
    if (m_ui.csvTabSeparator->isChecked())
    {
        settings.separator = "\t";
    }
    else
    {
		settings.separator = m_ui.csvSeparator->text().toUtf8().data();
	}

	return settings;
}


