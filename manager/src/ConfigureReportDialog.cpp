#include "ConfigureReportDialog.h"
#include "Emailer.h"
#include <QMessageBox>
#include <QSettings>

//////////////////////////////////////////////////////////////////////////

ConfigureReportDialog::ConfigureReportDialog(DB& db, QWidget* parent)
    : QDialog(parent)
    , m_db(db)
{
    m_ui.setupUi(this);
    m_ui.sensorFilter->init(db);

    adjustSize();
    loadSettings();

    connect(m_ui.sendNow, &QPushButton::released, this, &ConfigureReportDialog::sendReportNow);
}

//////////////////////////////////////////////////////////////////////////

void ConfigureReportDialog::loadSettings()
{
    QSettings settings;
    resize(settings.value("ConfigureReportDialog/size", size()).toSize());
    QPoint defaultPos = parentWidget()->mapToGlobal(parentWidget()->rect().center()) - QPoint(width() / 2, height() / 2);
    move(settings.value("ConfigureReportDialog/pos", defaultPos).toPoint());
}

//////////////////////////////////////////////////////////////////////////

void ConfigureReportDialog::saveSettings()
{
    QSettings settings;
    settings.setValue("ConfigureReportDialog/size", size());
    settings.setValue("ConfigureReportDialog/pos", pos());
}

//////////////////////////////////////////////////////////////////////////

void ConfigureReportDialog::sendReportNow()
{
    DB::Report report = m_report;
    if (getDescriptor(report.descriptor))
    {
        Emailer emailer(m_db);
        emailer.sendReportEmail(report, IClock::rtNow() - std::chrono::hours(24), IClock::rtNow());
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

    m_ui.sensorGroup->setChecked(descriptor.filterSensors);
    if (descriptor.filterSensors)
    {
        size_t sensorCount = m_db.getSensorCount();
        for (size_t i = 0; i < sensorCount; i++)
            m_ui.sensorFilter->getSensorModel().setSensorChecked(m_db.getSensor(i).id, false);

        for (DB::SensorId id: descriptor.sensors)
            m_ui.sensorFilter->getSensorModel().setSensorChecked(id, true);

        if (descriptor.sensors.empty()) //if no sensors were selected, select all
        {
            for (size_t i = 0; i < sensorCount; i++)
                m_ui.sensorFilter->getSensorModel().setSensorChecked(m_db.getSensor(i).id, true);
        }
    }
    else
    {
        size_t sensorCount = m_db.getSensorCount();
        for (size_t i = 0; i < sensorCount; i++)
            m_ui.sensorFilter->getSensorModel().setSensorChecked(m_db.getSensor(i).id, true);
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

    descriptor.filterSensors = m_ui.sensorGroup->isChecked();
    descriptor.sensors.clear();
    if (descriptor.filterSensors)
    {
        size_t sensorCount = m_db.getSensorCount();
        for (size_t i = 0; i < sensorCount; i++)
        {
            DB::Sensor sensor = m_db.getSensor(i);
            if (m_ui.sensorFilter->getSensorModel().isSensorChecked(sensor.id))
                descriptor.sensors.insert(sensor.id);
        }
        if (descriptor.sensors.empty()) //if no sensors were selected, select all
        {
            for (size_t i = 0; i < sensorCount; i++)
                descriptor.sensors.insert(m_db.getSensor(i).id);
        }
    }

    if (m_ui.useCustomPeriod->isChecked())
    {
        descriptor.period = DB::ReportDescriptor::Period::Custom;
        descriptor.customPeriod = std::chrono::hours(m_ui.customPeriod->value());
        if (std::chrono::duration_cast<std::chrono::hours>(descriptor.customPeriod).count() / 24 == 0)
            descriptor.customPeriod = std::chrono::hours(24);
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

    std::optional<DB::Report> report = m_db.findReportByName(descriptor.name);
    if (report.has_value() && report->id != m_report.id)
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

void ConfigureReportDialog::done(int result)
{
    saveSettings();
    QDialog::done(result);
}

//////////////////////////////////////////////////////////////////////////

void ConfigureReportDialog::accept()
{
    DB::ReportDescriptor descriptor;
    if (!getDescriptor(descriptor))
        return;

    m_report.descriptor = descriptor;
    QDialog::accept();
}

//////////////////////////////////////////////////////////////////////////

