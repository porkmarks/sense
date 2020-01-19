#include "ExportDataDialog.h"

#include <QMessageBox>
#include <QFileDialog>
#include <QSettings>
#include <QProgressDialog>
#include <QSettings>
#include <QDateTime>
#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cstring>
#include "Logger.h"
#include "Utils.h"

extern Logger s_logger;
extern std::string s_programFolder;

ExportDataDialog::ExportDataDialog(DB& db, MeasurementsModel& model, QWidget* parent)
    : QDialog(parent)
    , m_db(db)
    , m_model(model)
{
    m_ui.setupUi(this);

    adjustSize();

    connect(m_ui.overrideSettings, &QGroupBox::toggled, this, &ExportDataDialog::refreshPreview);
    connect(m_ui.csvSettings, &CsvSettingsWidget::settingsChanged, this, &ExportDataDialog::refreshPreview);
    m_ui.csvSettings->init(m_db);
    m_ui.csvSettings->setCsvSettings(m_db.getGeneralSettings(), m_db.getCsvSettings());
    m_ui.csvSettings->setPreviewVisible(false);

    loadSettings();

    refreshPreview();
}

//////////////////////////////////////////////////////////////////////////

void ExportDataDialog::refreshPreview()
{
    std::stringstream stream;

    size_t count = std::min<size_t>(m_model.getMeasurementCount(), 100);
    utils::exportCsvTo(stream, m_db.getGeneralSettings(), m_ui.overrideSettings->isChecked() ? m_ui.csvSettings->getCsvSettings() : m_db.getCsvSettings(), 
                       [this](size_t index) { return getCsvData(index); }, count, true);

    m_ui.preview->setPlainText((stream.str() + "\n.....\n").c_str());
}

//////////////////////////////////////////////////////////////////////////

std::optional<utils::CsvData> ExportDataDialog::getCsvData(size_t index) const
{
    utils::CsvData data;
    data.measurement = m_model.getMeasurement(index);
    int32_t sensorIndex = m_db.findSensorIndexById(data.measurement.descriptor.sensorId);
    if (sensorIndex >= 0)
    {
        data.sensor = m_db.getSensor((size_t)sensorIndex);
    }
    return data;
}

//////////////////////////////////////////////////////////////////////////

std::optional<utils::CsvData> ExportDataDialog::getCsvDataWithProgress(size_t index, QProgressDialog* progressDialog) const
{
	if (progressDialog && (index & 63) == 0)
	{
		progressDialog->setValue((int)index / 64);
		if (progressDialog->wasCanceled())
		{
			return std::nullopt;
		}
	}
	return getCsvData(index);
}

//////////////////////////////////////////////////////////////////////////

void ExportDataDialog::loadSettings()
{
    QSettings settings;
    resize(settings.value("ExportDataDialog/size", size()).toSize());
    QPoint defaultPos = parentWidget()->mapToGlobal(parentWidget()->rect().center()) - QPoint(width() / 2, height() / 2);
    move(settings.value("ExportDataDialog/pos", defaultPos).toPoint());
}

//////////////////////////////////////////////////////////////////////////

void ExportDataDialog::saveSettings()
{
    QSettings settings;
    settings.setValue("ExportDataDialog/size", size());
    settings.setValue("ExportDataDialog/pos", pos());
}

//////////////////////////////////////////////////////////////////////////

void ExportDataDialog::done(int result)
{
    saveSettings();
    QDialog::done(result);
}

//////////////////////////////////////////////////////////////////////////

void ExportDataDialog::accept()
{
    QString extension = "Export Files (*.csv)";

    std::string defaultFilename = "report.csv";

    DB::Filter const& filter = m_model.getFilter();
    if (filter.useTimePointFilter)
    {
        defaultFilename = "report";
        defaultFilename += QDateTime::fromTime_t(IClock::to_time_t(filter.timePointFilter.min)).toString("dd_MM_yyyy_HH_mm").toUtf8().data();
        defaultFilename += "-";
        defaultFilename += QDateTime::fromTime_t(IClock::to_time_t(filter.timePointFilter.max)).toString("dd_MM_yyyy_HH_mm").toUtf8().data();
        defaultFilename += ".csv";
    }

    QString fileName = QFileDialog::getSaveFileName(this, "Select export file", 
                                                    (s_programFolder + "/user_reports/" + defaultFilename).c_str(), extension);
    if (fileName.isEmpty())
    {
        return;
    }

	std::unique_ptr<QProgressDialog> progressDialog;
	progressDialog.reset(new QProgressDialog("Exporting data...", "Abort", 0, (int)m_model.getMeasurementCount() / 64, this));
	progressDialog->setWindowModality(Qt::WindowModal);
	progressDialog->setMinimumDuration(1000);

    bool finished = false;
    {
        std::ofstream file(fileName.toUtf8().data());
        if (!file.is_open())
        {
            QString msg = QString("Cannot open file '%1' for exporting data:\n%2").arg(fileName).arg(std::strerror(errno));
            s_logger.logCritical(msg);
            QMessageBox::critical(this, "Error", msg);
            return;
        }

		finished = utils::exportCsvTo(file, m_db.getGeneralSettings(), m_ui.overrideSettings->isChecked() ? m_ui.csvSettings->getCsvSettings() : m_db.getCsvSettings(),
                                      [this, &progressDialog](size_t index) { return getCsvDataWithProgress(index, progressDialog.get()); }, m_model.getMeasurementCount(), false);

        file.close();

        if (!finished)
        {
            //canceled
            std::remove(fileName.toUtf8().data());
        }
    }

    QString msg = finished ? QString("Data was exported to file '%1'").arg(fileName) :
                             QString("Exporting data to file '%1' was canceled").arg(fileName);
    s_logger.logInfo(msg);
    QMessageBox::information(this, "Success", msg);

    QDialog::accept();
}

