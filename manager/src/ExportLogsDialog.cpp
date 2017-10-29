#include "ExportLogsDialog.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cstring>

extern float getBatteryLevel(float vcc);


ExportLogsDialog::ExportLogsDialog(LogsModel& model)
    : m_model(model)
{
    m_ui.setupUi(this);

    connect(m_ui.dateFormat, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &ExportLogsDialog::refreshPreview);
    connect(m_ui.exportIndex, &QCheckBox::stateChanged, this, &ExportLogsDialog::refreshPreview);
    connect(m_ui.exportTimePoint, &QCheckBox::stateChanged, this, &ExportLogsDialog::refreshPreview);
    connect(m_ui.exportType, &QCheckBox::stateChanged, this, &ExportLogsDialog::refreshPreview);
    connect(m_ui.exportMessage, &QCheckBox::stateChanged, this, &ExportLogsDialog::refreshPreview);
    connect(m_ui.separator, &QLineEdit::textChanged, this, &ExportLogsDialog::refreshPreview);
    connect(m_ui.tabSeparator, &QCheckBox::stateChanged, this, &ExportLogsDialog::refreshPreview);

    refreshPreview();
}

//////////////////////////////////////////////////////////////////////////

void ExportLogsDialog::refreshPreview()
{
    std::stringstream stream;
    exportTo(stream, 100);

    m_ui.previewText->setText((stream.str() + "\n.....\n").c_str());
}

//////////////////////////////////////////////////////////////////////////

void ExportLogsDialog::accept()
{
    QString extension = "Export Files (*.csv)";

    QString fileName = QFileDialog::getSaveFileName(this, "Select export file", QString(), extension);
    if (fileName.isEmpty())
    {
        return;
    }

    {
        std::ofstream file(fileName.toUtf8().data());
        if (!file.is_open())
        {
            QMessageBox::critical(this, "Error", QString("Cannot open file '%1' for writing:\n%2").arg(fileName).arg(std::strerror(errno)));
            return;
        }

        exportTo(file, size_t(-1));

        file.close();
    }

    QMessageBox::information(this, "Success", QString("Data was exported to file '%1'").arg(fileName));

    QDialog::accept();
}

//////////////////////////////////////////////////////////////////////////

void ExportLogsDialog::exportTo(std::ostream& stream, size_t maxCount)
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
    bool exportIndex = m_ui.exportIndex->isChecked();
    bool exportTimePoint = m_ui.exportTimePoint->isChecked();
    bool exportType = m_ui.exportType->isChecked();
    bool exportMessage = m_ui.exportMessage->isChecked();

    //header
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
    if (exportType)
    {
        stream << "Type";
        stream << separator;
    }
    if (exportMessage)
    {
        stream << "Message";
        stream << separator;
    }
    stream << std::endl;

    //data
    size_t size = m_model.getLineCount();
    for (size_t i = 0; i < std::min(size, maxCount); i++)
    {
        Logger::LogLine const& l = m_model.getLine(i);
        if (exportIndex)
        {
            stream << l.index;
            stream << separator;
        }
        if (exportTimePoint)
        {
            char buf[128];
            time_t t = Logger::Clock::to_time_t(l.timePoint);
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
        if (exportType)
        {
            switch (l.type)
            {
            case Logger::Type::VERBOSE: stream << "Verbose"; break;
            case Logger::Type::INFO: stream << "Info"; break;
            case Logger::Type::WARNING: stream << "Warning"; break;
            case Logger::Type::CRITICAL: stream << "Error"; break;
            default: stream << "N/A"; break;
            }
            stream << separator;
        }
        if (exportMessage)
        {
            stream << l.message;
            stream << separator;
        }
        stream << std::endl;
    }
}
