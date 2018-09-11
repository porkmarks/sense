#include "ConfigureReportDialog.h"
#include "Emailer.h"
#include <QMessageBox>

//////////////////////////////////////////////////////////////////////////

ConfigureReportDialog::ConfigureReportDialog(Settings& settings, DB& db)
    : m_settings(settings)
    , m_db(db)
{
    m_ui.setupUi(this);
    m_ui.sensorFilter->init(db);

    connect(m_ui.sendNow, &QPushButton::released, this, &ConfigureReportDialog::sendReportNow);
}

//////////////////////////////////////////////////////////////////////////

void ConfigureReportDialog::sendReportNow()
{
    DB::Report report = m_report;
    if (getDescriptor(report.descriptor))
    {
        Emailer emailer(m_settings, m_db);
        emailer.sendReportEmail(report);
        QMessageBox::information(this, "Done", "The report email was scheduled to be sent.");
    }
}

//////////////////////////////////////////////////////////////////////////

DB::Report const& ConfigureReportDialog::getReport() const
{
    return m_report;
}

//////////////////////////////////////////////////////////////////////////

void ConfigureReportDialog::setReport(DB::Report const& report)
{
    m_report = report;
    DB::ReportDescriptor descriptor = m_report.descriptor;

    m_ui.name->setText(descriptor.name.c_str());

    m_ui.allData->setChecked(descriptor.data == DB::ReportDescriptor::Data::All);

    m_ui.sensorGroup->setChecked(descriptor.filterSensors);
    if (descriptor.filterSensors)
    {
        size_t sensorCount = m_db.getSensorCount();
        for (size_t i = 0; i < sensorCount; i++)
        {
            m_ui.sensorFilter->getSensorModel().setSensorChecked(m_db.getSensor(i).id, false);
        }

        for (DB::SensorId id: descriptor.sensors)
        {
            m_ui.sensorFilter->getSensorModel().setSensorChecked(id, true);
        }
    }
    else
    {
        size_t sensorCount = m_db.getSensorCount();
        for (size_t i = 0; i < sensorCount; i++)
        {
            m_ui.sensorFilter->getSensorModel().setSensorChecked(m_db.getSensor(i).id, true);
        }
    }

    if (descriptor.period == DB::ReportDescriptor::Period::Custom)
    {
        m_ui.useCustomPeriod->setChecked(true);
        int days = static_cast<int>(std::chrono::duration_cast<std::chrono::hours>(descriptor.customPeriod).count() / 24);
        m_ui.customPeriod->setValue(days);
    }
    else
    {
        m_ui.period->setCurrentIndex(static_cast<int>(descriptor.period));
        m_ui.useCustomPeriod->setChecked(false);
    }
}

//////////////////////////////////////////////////////////////////////////

bool ConfigureReportDialog::getDescriptor(DB::ReportDescriptor& descriptor)
{
    descriptor.name = m_ui.name->text().toUtf8().data();
    descriptor.data = m_ui.allData->isChecked() ? DB::ReportDescriptor::Data::All : DB::ReportDescriptor::Data::Summary;

    descriptor.filterSensors = m_ui.sensorGroup->isChecked();
    descriptor.sensors.clear();
    if (descriptor.filterSensors)
    {
        size_t sensorCount = m_db.getSensorCount();
        for (size_t i = 0; i < sensorCount; i++)
        {
            DB::Sensor const& sensor = m_db.getSensor(i);
            if (m_ui.sensorFilter->getSensorModel().isSensorChecked(sensor.id))
            {
                descriptor.sensors.insert(sensor.id);
            }
        }
    }

    if (m_ui.useCustomPeriod->isChecked())
    {
        descriptor.period = DB::ReportDescriptor::Period::Custom;
        descriptor.customPeriod = std::chrono::hours(m_ui.customPeriod->value());
        if (std::chrono::duration_cast<std::chrono::hours>(descriptor.customPeriod).count() / 24 == 0)
        {
            descriptor.customPeriod = std::chrono::hours(24);
        }
    }
    else
    {
        descriptor.period = static_cast<DB::ReportDescriptor::Period>(m_ui.period->currentIndex());
    }

    if (descriptor.name.empty())
    {
        QMessageBox::critical(this, "Error", "You need to specify a name for this report.");
        return false;
    }

    int32_t reportIndex = m_db.findReportIndexByName(descriptor.name);
    if (reportIndex >= 0 && m_db.getReport(static_cast<size_t>(reportIndex)).id != m_report.id)
    {
        QMessageBox::critical(this, "Error", QString("Report '%1' already exists.").arg(descriptor.name.c_str()));
        return false;
    }
    if (descriptor.filterSensors && descriptor.sensors.empty())
    {
        QMessageBox::critical(this, "Error", "You need to specify at least one sensor.");
        return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////

void ConfigureReportDialog::accept()
{
    DB::ReportDescriptor descriptor;
    if (!getDescriptor(descriptor))
    {
        return;
    }

    m_report.descriptor = descriptor;
    QDialog::accept();
}

//////////////////////////////////////////////////////////////////////////

