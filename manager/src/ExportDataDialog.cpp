#include "ExportDataDialog.h"

#include <QMessageBox>
#include <QFileDialog>
#include <QSettings>
#include <QProgressDialog>
#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cstring>
#include "Logger.h"

extern Logger s_logger;
extern float getBatteryLevel(float vcc);
extern float getSignalLevel(int8_t dBm);

ExportDataDialog::ExportDataDialog(DB& db, MeasurementsModel& model)
    : m_db(db)
    , m_model(model)
{
    m_ui.setupUi(this);

    connect(m_ui.dateFormat, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &ExportDataDialog::refreshPreview);
    connect(m_ui.exportId, &QCheckBox::stateChanged, this, &ExportDataDialog::refreshPreview);
    connect(m_ui.exportIndex, &QCheckBox::stateChanged, this, &ExportDataDialog::refreshPreview);
    connect(m_ui.exportSensorName, &QCheckBox::stateChanged, this, &ExportDataDialog::refreshPreview);
    connect(m_ui.exportSensorSN, &QCheckBox::stateChanged, this, &ExportDataDialog::refreshPreview);
    connect(m_ui.exportTimePoint, &QCheckBox::stateChanged, this, &ExportDataDialog::refreshPreview);
    connect(m_ui.exportTemperature, &QCheckBox::stateChanged, this, &ExportDataDialog::refreshPreview);
    connect(m_ui.exportHumidity, &QCheckBox::stateChanged, this, &ExportDataDialog::refreshPreview);
    connect(m_ui.exportBattery, &QCheckBox::stateChanged, this, &ExportDataDialog::refreshPreview);
    connect(m_ui.exportSignal, &QCheckBox::stateChanged, this, &ExportDataDialog::refreshPreview);
    connect(m_ui.units, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &ExportDataDialog::refreshPreview);
    connect(m_ui.decimalPlaces, static_cast<void(QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &ExportDataDialog::refreshPreview);
    connect(m_ui.separator, &QLineEdit::textChanged, this, &ExportDataDialog::refreshPreview);
    connect(m_ui.tabSeparator, &QCheckBox::stateChanged, this, &ExportDataDialog::refreshPreview);

    loadSettings();

    refreshPreview();
}

//////////////////////////////////////////////////////////////////////////

void ExportDataDialog::refreshPreview()
{
    std::stringstream stream;
    exportTo(stream, 100, false);

    m_ui.previewText->setText((stream.str() + "\n.....\n").c_str());
}

//////////////////////////////////////////////////////////////////////////

void ExportDataDialog::loadSettings()
{
    QSettings settings;
    m_ui.dateFormat->setCurrentIndex(settings.value("exportData/dateFormat", 0).toInt());
    m_ui.exportId->setChecked(settings.value("exportData/exportId", true).toBool());
    m_ui.exportIndex->setChecked(settings.value("exportData/exportIndex", true).toBool());
    m_ui.exportSensorName->setChecked(settings.value("exportData/exportSensorName", true).toBool());
    m_ui.exportSensorSN->setChecked(settings.value("exportData/exportSensorSN", true).toBool());
    m_ui.exportTimePoint->setChecked(settings.value("exportData/exportTimePoint", true).toBool());
    m_ui.exportTemperature->setChecked(settings.value("exportData/exportTemperature", true).toBool());
    m_ui.exportHumidity->setChecked(settings.value("exportData/exportHumidity", true).toBool());
    m_ui.exportBattery->setChecked(settings.value("exportData/exportBattery", false).toBool());
    m_ui.exportSignal->setChecked(settings.value("exportData/exportSignal", false).toBool());
    m_ui.units->setCurrentIndex(settings.value("exportData/units", 1).toInt());
    m_ui.decimalPlaces->setValue(settings.value("exportData/decimalPlaces", 2).toInt());
    m_ui.separator->setText(settings.value("exportData/separator", ";").toString());
    m_ui.tabSeparator->setChecked(settings.value("exportData/tabSeparator", false).toBool());
}

//////////////////////////////////////////////////////////////////////////

void ExportDataDialog::saveSettings()
{
    QSettings settings;
    settings.setValue("exportData/dateFormat", m_ui.dateFormat->currentIndex());
    settings.setValue("exportData/exportId", m_ui.exportId->isChecked());
    settings.setValue("exportData/exportIndex", m_ui.exportIndex->isChecked());
    settings.setValue("exportData/exportSensorName", m_ui.exportSensorName->isChecked());
    settings.setValue("exportData/exportSensorSN", m_ui.exportSensorSN->isChecked());
    settings.setValue("exportData/exportTimePoint", m_ui.exportTimePoint->isChecked());
    settings.setValue("exportData/exportTemperature", m_ui.exportTemperature->isChecked());
    settings.setValue("exportData/exportHumidity", m_ui.exportHumidity->isChecked());
    settings.setValue("exportData/exportBattery", m_ui.exportBattery->isChecked());
    settings.setValue("exportData/exportSignal", m_ui.exportSignal->isChecked());
    settings.setValue("exportData/units", m_ui.units->currentIndex());
    settings.setValue("exportData/decimalPlaces", m_ui.decimalPlaces->value());
    settings.setValue("exportData/separator", m_ui.separator->text());
    settings.setValue("exportData/tabSeparator", m_ui.tabSeparator->isChecked());
}

//////////////////////////////////////////////////////////////////////////

void ExportDataDialog::accept()
{
    saveSettings();

    QString extension = "Export Files (*.csv)";

    QString fileName = QFileDialog::getSaveFileName(this, "Select export file", QString(), extension);
    if (fileName.isEmpty())
    {
        return;
    }

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

        finished = exportTo(file, size_t(-1), true);
        file.close();

        if (!finished)
        {
            //cancelled
            std::remove(fileName.toUtf8().data());
        }
    }

    QString msg = finished ? QString("Data was exported to file '%1'").arg(fileName) :
                             QString("Exporting data to file '%1' was cancelled").arg(fileName);
    s_logger.logInfo(msg);
    QMessageBox::information(this, "Success", msg);

    QDialog::accept();
}

//////////////////////////////////////////////////////////////////////////

bool ExportDataDialog::exportTo(std::ostream& stream, size_t maxCount, bool showProgress)
{
    std::string separator = m_ui.separator->text().toUtf8().data();
    if (m_ui.tabSeparator->isChecked())
    {
        separator = "\t";
    }

    enum class DateTimeFormat
    {
        YYYY_MM_DD_Slash,
        YYYY_MM_DD_Dash,
        DD_MM_YYYY_Dash,
        MM_DD_YYYY_Slash
    };

    enum class UnitsFormat
    {
        None,
        Embedded,
        SeparateColumn
    };

    DateTimeFormat dateTimeFormat = static_cast<DateTimeFormat>(m_ui.dateFormat->currentIndex());
    bool exportId = m_ui.exportId->isChecked();
    bool exportIndex = m_ui.exportIndex->isChecked();
    bool exportSensorName = m_ui.exportSensorName->isChecked();
    bool exportSensorSN = m_ui.exportSensorSN->isChecked();
    bool exportTimePoint = m_ui.exportTimePoint->isChecked();
    bool exportTemperature = m_ui.exportTemperature->isChecked();
    bool exportHumidity = m_ui.exportHumidity->isChecked();
    bool exportBattery = m_ui.exportBattery->isChecked();
    bool exportSignal = m_ui.exportSignal->isChecked();
    UnitsFormat unitsFormat = static_cast<UnitsFormat>(m_ui.units->currentIndex());
    int decimalPlaces = m_ui.decimalPlaces->value();

    //header
    if (exportId)
    {
        stream << "Id";
        stream << separator;
    }
    if (exportSensorName)
    {
        stream << "Sensor Name";
        stream << separator;
    }
    if (exportSensorSN)
    {
        stream << "Sensor S/N";
        stream << separator;
    }
    if (exportIndex)
    {
        stream << "Index";
        stream << separator;
    }
    if (exportTimePoint)
    {
        stream << "Date/Time";
        stream << separator;
    }
    if (exportTemperature)
    {
        stream << "Temperature";
        stream << separator;
        if (unitsFormat == UnitsFormat::SeparateColumn)
        {
            stream << "Temperature Unit";
            stream << separator;
        }
    }
    if (exportHumidity)
    {
        stream << "Humidity";
        stream << separator;
        if (unitsFormat == UnitsFormat::SeparateColumn)
        {
            stream << "Humidity Unit";
            stream << separator;
        }
    }
    if (exportBattery)
    {
        stream << "Battery";
        stream << separator;
        if (unitsFormat == UnitsFormat::SeparateColumn)
        {
            stream << "Battery Unit";
            stream << separator;
        }
    }
    if (exportSignal)
    {
        stream << "Signal";
        stream << separator;
        if (unitsFormat == UnitsFormat::SeparateColumn)
        {
            stream << "Signal Unit";
            stream << separator;
        }
    }
    stream << std::endl;

    //data
    size_t size = std::min(m_model.getMeasurementCount(), maxCount);

    std::unique_ptr<QProgressDialog> progressDialog;
    if (showProgress)
    {
        progressDialog.reset(new QProgressDialog("Exporting data...", "Abort", 0, size / 16, this));
        progressDialog->setWindowModality(Qt::WindowModal);
        progressDialog->setMinimumDuration(1000);
    }

    for (size_t i = 0; i < size; i++)
    {
        if (progressDialog && (i & 0xF) == 0)
        {
            progressDialog->setValue(i / 16);
            if (progressDialog->wasCanceled())
            {
                return false;
            }
        }

        DB::Measurement const& m = m_model.getMeasurement(i);
        if (exportId)
        {
            stream << m.id;
            stream << separator;
        }
        if (exportSensorName)
        {
            int32_t sensorIndex = m_db.findSensorIndexById(m.descriptor.sensorId);
            if (sensorIndex < 0)
            {
                stream << "N/A";
            }
            else
            {
                stream << m_db.getSensor(static_cast<size_t>(sensorIndex)).descriptor.name;
            }
            stream << separator;
        }
        if (exportSensorSN)
        {
            int32_t sensorIndex = m_db.findSensorIndexById(m.descriptor.sensorId);
            if (sensorIndex < 0)
            {
                stream << "N/A";
            }
            else
            {
                stream << QString("%1").arg(m_db.getSensor(static_cast<size_t>(sensorIndex)).serialNumber, 8, 16, QChar('0')).toUtf8().data();
            }
            stream << separator;
        }
        if (exportIndex)
        {
            stream << m.descriptor.index;
            stream << separator;
        }
        if (exportTimePoint)
        {
            char buf[128];
            time_t t = DB::Clock::to_time_t(m.descriptor.timePoint);
            if (dateTimeFormat == DateTimeFormat::YYYY_MM_DD_Slash)
            {
                strftime(buf, 128, "%Y/%m/%d %H:%M:%S", localtime(&t));
            }
            else if (dateTimeFormat == DateTimeFormat::YYYY_MM_DD_Dash)
            {
                strftime(buf, 128, "%Y-%m-%d %H:%M:%S", localtime(&t));
            }
            else if (dateTimeFormat == DateTimeFormat::DD_MM_YYYY_Dash)
            {
                strftime(buf, 128, "%d-%m-%Y %H:%M:%S", localtime(&t));
            }
            else
            {
                strftime(buf, 128, "%m/%d/%y %H:%M:%S", localtime(&t));
            }

            stream << buf;
            stream << separator;
        }
        if (exportTemperature)
        {
            stream << std::fixed << std::setprecision(decimalPlaces) << m.descriptor.temperature;
            if (unitsFormat == UnitsFormat::Embedded)
            {
                stream << "°C";
            }
            stream << separator;
            if (unitsFormat == UnitsFormat::SeparateColumn)
            {
                stream << "°C";
                stream << separator;
            }
        }
        if (exportHumidity)
        {
            stream << std::fixed << std::setprecision(decimalPlaces) << m.descriptor.humidity;
            if (unitsFormat == UnitsFormat::Embedded)
            {
                stream << " %RH";
            }
            stream << separator;
            if (unitsFormat == UnitsFormat::SeparateColumn)
            {
                stream << "%RH";
                stream << separator;
            }
        }
        if (exportBattery)
        {
            stream << std::fixed << std::setprecision(decimalPlaces) << getBatteryLevel(m.descriptor.vcc) * 100.f;
            if (unitsFormat == UnitsFormat::Embedded)
            {
                stream << " %";
            }
            stream << separator;
            if (unitsFormat == UnitsFormat::SeparateColumn)
            {
                stream << "%";
                stream << separator;
            }
        }
        if (exportSignal)
        {
            stream << std::fixed << std::setprecision(decimalPlaces) << getSignalLevel(std::min(m.descriptor.signalStrength.b2s, m.descriptor.signalStrength.s2b)) * 100.f;
            if (unitsFormat == UnitsFormat::Embedded)
            {
                stream << " %";
            }
            stream << separator;
            if (unitsFormat == UnitsFormat::SeparateColumn)
            {
                stream << "%";
                stream << separator;
            }
        }
        stream << std::endl;
    }

    return true;
}
