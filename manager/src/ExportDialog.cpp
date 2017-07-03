#include "ExportDialog.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cstring>

ExportDialog::ExportDialog(DB& db, MeasurementsModel& model)
    : m_db(db)
    , m_model(model)
{
    m_ui.setupUi(this);

    connect(m_ui.showPreview, &QPushButton::released, this, &ExportDialog::showPreview);
}

//////////////////////////////////////////////////////////////////////////

void ExportDialog::showPreview()
{
    std::stringstream stream;
    exportTo(stream, 100);

    m_ui.preview->setText((stream.str() + "\n.....\n").c_str());
}

//////////////////////////////////////////////////////////////////////////

void ExportDialog::accept()
{
    QString extension = "Export Files (*.csv)";

    QString fileName = QFileDialog::getSaveFileName(this, "Select export file", QString(), extension);
    if (fileName.isEmpty())
    {
        return;
    }

    std::ofstream file(fileName.toUtf8().data());
    if (!file.is_open())
    {
        QMessageBox::critical(this, "Error", QString("Cannot open file '%1' for writing:\n%2").arg(fileName).arg(std::strerror(errno)));
        return;
    }

    exportTo(file, size_t(-1));

    file.close();

    QMessageBox::information(this, "Success", QString("Data was exported to file '%1'").arg(fileName));

    QDialog::accept();
}

//////////////////////////////////////////////////////////////////////////

void ExportDialog::exportTo(std::ostream& stream, size_t maxCount)
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
    bool exportTimePoint = m_ui.exportTimePoint->isChecked();
    bool exportTemperature = m_ui.exportTemperature->isChecked();
    bool exportHumidity = m_ui.exportHumidity->isChecked();
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
    }
    if (unitsFormat == UnitsFormat::SeparateColumn)
    {
        stream << "Temperature Unit";
        stream << separator;
    }
    if (exportHumidity)
    {
        stream << "Humidity";
        stream << separator;
    }
    if (unitsFormat == UnitsFormat::SeparateColumn)
    {
        stream << "Humidity Unit";
        stream << separator;
    }
    stream << std::endl;

    //data
    size_t size = m_model.getMeasurementCount();
    for (size_t i = 0; i < std::min(size, maxCount); i++)
    {
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
                stream << m_db.getSensor(sensorIndex).descriptor.name;
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
                stream << u8" °C";
            }
            stream << separator;
        }
        if (unitsFormat == UnitsFormat::SeparateColumn)
        {
            stream << u8"°C";
            stream << separator;
        }
        if (exportHumidity)
        {
            stream << std::fixed << std::setprecision(decimalPlaces) << m.descriptor.humidity;
            if (unitsFormat == UnitsFormat::Embedded)
            {
                stream << " % RH";
            }
            stream << separator;
        }
        if (unitsFormat == UnitsFormat::SeparateColumn)
        {
            stream << "% RH";
            stream << separator;
        }
        stream << std::endl;
    }
}
