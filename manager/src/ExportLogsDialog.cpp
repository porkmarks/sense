#include "ExportLogsDialog.h"

#include <QFileDialog>
#include <QMessageBox>
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


ExportLogsDialog::ExportLogsDialog(LogsModel& model, QWidget* parent)
    : QDialog(parent)
    , m_model(model)
{
    m_ui.setupUi(this);

    connect(m_ui.dateFormat, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &ExportLogsDialog::refreshPreview);
    connect(m_ui.exportIndex, &QCheckBox::stateChanged, this, &ExportLogsDialog::refreshPreview);
    connect(m_ui.exportTimePoint, &QCheckBox::stateChanged, this, &ExportLogsDialog::refreshPreview);
    connect(m_ui.exportType, &QCheckBox::stateChanged, this, &ExportLogsDialog::refreshPreview);
    connect(m_ui.exportMessage, &QCheckBox::stateChanged, this, &ExportLogsDialog::refreshPreview);
    connect(m_ui.separator, &QLineEdit::textChanged, this, &ExportLogsDialog::refreshPreview);
    connect(m_ui.tabSeparator, &QCheckBox::stateChanged, this, &ExportLogsDialog::refreshPreview);

    loadSettings();
    refreshPreview();
}

//////////////////////////////////////////////////////////////////////////

void ExportLogsDialog::loadSettings()
{
    QSettings settings;
    m_ui.dateFormat->setCurrentIndex(settings.value("exportLogs/dateFormat", 0).toInt());
    m_ui.exportIndex->setChecked(settings.value("exportLogs/exportIndex", true).toBool());
    m_ui.exportTimePoint->setChecked(settings.value("exportLogs/exportTimePoint", true).toBool());
    m_ui.exportType->setChecked(settings.value("exportLogs/exportType", true).toBool());
    m_ui.exportMessage->setChecked(settings.value("exportLogs/exportMessage", true).toBool());
    m_ui.separator->setText(settings.value("exportLogs/separator", ";").toString());
    m_ui.tabSeparator->setChecked(settings.value("exportLogs/tabSeparator", false).toBool());
}

//////////////////////////////////////////////////////////////////////////

void ExportLogsDialog::saveSettings()
{
    QSettings settings;
    settings.setValue("exportLogs/dateFormat", m_ui.dateFormat->currentIndex());
    settings.setValue("exportLogs/exportIndex", m_ui.exportIndex->isChecked());
    settings.setValue("exportLogs/exportTimePoint", m_ui.exportTimePoint->isChecked());
    settings.setValue("exportLogs/exportType", m_ui.exportType->isChecked());
    settings.setValue("exportLogs/exportMessage", m_ui.exportMessage->isChecked());
    settings.setValue("exportLogs/separator", m_ui.separator->text());
    settings.setValue("exportLogs/tabSeparator", m_ui.tabSeparator->isChecked());
}

//////////////////////////////////////////////////////////////////////////

void ExportLogsDialog::refreshPreview()
{
    std::stringstream stream;
    exportTo(stream, 100, false);

    m_ui.previewText->setText((stream.str() + "\n.....\n").c_str());
}

//////////////////////////////////////////////////////////////////////////

void ExportLogsDialog::accept()
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
            QString msg = QString("Cannot open file '%1' for exporting logs:\n%2").arg(fileName).arg(std::strerror(errno));
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

    QString msg = finished ? QString("Logs were exported to file '%1'").arg(fileName) :
                             QString("Exporting logs to file '%1' was cancelled").arg(fileName);
    s_logger.logInfo(msg);
    QMessageBox::information(this, "Success", msg);

    QDialog::accept();
}

//////////////////////////////////////////////////////////////////////////

bool ExportLogsDialog::exportTo(std::ostream& stream, size_t maxCount, bool showProgress)
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
    size_t size = std::min(m_model.getLineCount(), maxCount);

    std::unique_ptr<QProgressDialog> progressDialog;
    if (showProgress)
    {
        progressDialog.reset(new QProgressDialog("Exporting logs...", "Abort", 0, size / 16, this));
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

    return true;
}
