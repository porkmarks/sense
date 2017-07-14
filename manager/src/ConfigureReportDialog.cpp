#include "ConfigureReportDialog.h"
#include "Emailer.h"

//////////////////////////////////////////////////////////////////////////

ConfigureReportDialog::ConfigureReportDialog(DB& db)
    : m_db(db)
    , m_model(db)
    , m_delegate(m_sortingModel)
{
    m_ui.setupUi(this);

    m_model.setShowCheckboxes(true);
    m_sortingModel.setSourceModel(&m_model);

    size_t sensorCount = m_db.getSensorCount();
    for (size_t i = 0; i < sensorCount; i++)
    {
        m_model.setSensorChecked(m_db.getSensor(i).id, true);
    }

    m_ui.sensorList->setModel(&m_sortingModel);
    m_ui.sensorList->setItemDelegate(&m_delegate);

    for (int i = 0; i < m_model.columnCount(); i++)
    {
        m_ui.sensorList->header()->setSectionResizeMode(i, QHeaderView::ResizeToContents);
    }
    m_ui.sensorList->header()->setStretchLastSection(true);

    connect(m_ui.sendNow, &QPushButton::released, this, &ConfigureReportDialog::sendReportNow);
}

//////////////////////////////////////////////////////////////////////////

void ConfigureReportDialog::sendReportNow()
{
    DB::Report report = m_report;
    if (getDescriptor(report.descriptor))
    {
        if (!report.descriptor.sendEmailAction || report.descriptor.emailRecipient.empty())
        {
            QMessageBox::critical(this, "Error", "You need to specify an email recipient to send a test email.");
        }
        else
        {
            Emailer emailer;
            emailer.init(m_db);

            emailer.sendReportEmail(report);
            QMessageBox::information(this, "Success", QString("Report sent to %1.").arg(report.descriptor.emailRecipient.c_str()));
        }
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
        {
            m_model.setSensorChecked(m_db.getSensor(i).id, false);
        }

        for (DB::SensorId id: descriptor.sensors)
        {
            m_model.setSensorChecked(id, true);
        }
    }
    else
    {
        size_t sensorCount = m_db.getSensorCount();
        for (size_t i = 0; i < sensorCount; i++)
        {
            m_model.setSensorChecked(m_db.getSensor(i).id, true);
        }
    }

    if (descriptor.period == DB::ReportDescriptor::Period::Custom)
    {
        m_ui.useCustomPeriod->setChecked(true);
        size_t days = std::chrono::duration_cast<std::chrono::hours>(descriptor.customPeriod).count() / 24;
        m_ui.customPeriod->setValue(days);
    }
    else
    {
        m_ui.period->setCurrentIndex(static_cast<int>(descriptor.period));
        m_ui.useCustomPeriod->setChecked(false);
    }

    m_ui.sendEmailAction->setChecked(descriptor.sendEmailAction);
    m_ui.emailRecipient->setText(descriptor.emailRecipient.c_str());

    m_ui.uploadToFtpAction->setChecked(descriptor.uploadToFtpAction);
    m_ui.ftpFolder->setText(descriptor.ftpFolder.c_str());
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
            if (m_model.isSensorChecked(sensor.id))
            {
                descriptor.sensors.push_back(sensor.id);
            }
        }
    }

    if (m_ui.useCustomPeriod->isChecked())
    {
        descriptor.period = DB::ReportDescriptor::Period::Custom;
        descriptor.customPeriod = std::chrono::hours(m_ui.customPeriod->value());
        size_t days = std::chrono::duration_cast<std::chrono::hours>(descriptor.customPeriod).count() / 24;
        if (days == 0)
        {
            descriptor.customPeriod = std::chrono::hours(24);
        }
    }
    else
    {
        descriptor.period = static_cast<DB::ReportDescriptor::Period>(m_ui.period->currentIndex());
    }

    descriptor.sendEmailAction = m_ui.sendEmailAction->isChecked();
    descriptor.emailRecipient = m_ui.emailRecipient->text().toUtf8().data();

    descriptor.uploadToFtpAction = m_ui.uploadToFtpAction->isChecked();
    descriptor.ftpFolder = m_ui.ftpFolder->text().toUtf8().data();

    if (descriptor.name.empty())
    {
        QMessageBox::critical(this, "Error", "You need to specify a name for this report.");
        return false;
    }
    if (descriptor.uploadToFtpAction && descriptor.ftpFolder.empty())
    {
        QMessageBox::critical(this, "Error", "You need to specify the ftp folder.");
        return false;
    }
    if (descriptor.sendEmailAction && descriptor.emailRecipient.empty())
    {
        QMessageBox::critical(this, "Error", "You need to specify the email recipient.");
        return false;
    }

    int32_t reportIndex = m_db.findReportIndexByName(descriptor.name);
    if (reportIndex >= 0 && m_db.getReport(reportIndex).id != m_report.id)
    {
        QMessageBox::critical(this, "Error", QString("Report '%1' already exists.").arg(descriptor.name.c_str()));
        return false;
    }
    if (descriptor.filterSensors && descriptor.sensors.empty())
    {
        QMessageBox::critical(this, "Error", "You need to specify at least one sensor.");
        return false;
    }
    if (!descriptor.sendEmailAction && !descriptor.uploadToFtpAction)
    {
        QMessageBox::critical(this, "Error", "You need to enable at least one action.");
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

