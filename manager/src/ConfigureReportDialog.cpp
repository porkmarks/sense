#include "ConfigureReportDialog.h"

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

//    m_ui.highTemperatureWatch->setChecked(descriptor.highTemperatureWatch);
//    m_ui.highTemperature->setValue(descriptor.highTemperature);
//    m_ui.lowTemperatureWatch->setChecked(descriptor.lowTemperatureWatch);
//    m_ui.lowTemperature->setValue(descriptor.lowTemperature);

//    m_ui.highHumidityWatch->setChecked(descriptor.highHumidityWatch);
//    m_ui.highHumidity->setValue(descriptor.highHumidity);
//    m_ui.lowHumidityWatch->setChecked(descriptor.lowHumidityWatch);
//    m_ui.lowHumidity->setValue(descriptor.lowHumidity);

//    m_ui.sensorErrorsWatch->setChecked(descriptor.sensorErrorsWatch);
//    m_ui.lowSignalWatch->setChecked(descriptor.lowSignalWatch);
//    m_ui.lowBatteryWatch->setChecked(descriptor.lowVccWatch);

    m_ui.sendEmailAction->setChecked(descriptor.sendEmailAction);
    m_ui.emailRecipient->setText(descriptor.emailRecipient.c_str());
}

//////////////////////////////////////////////////////////////////////////

void ConfigureReportDialog::accept()
{
    DB::ReportDescriptor descriptor;
    descriptor.name = m_ui.name->text().toUtf8().data();

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

//    descriptor.highTemperatureWatch = m_ui.highTemperatureWatch->isChecked();
//    descriptor.highTemperature = m_ui.highTemperature->value();
//    descriptor.lowTemperatureWatch = m_ui.lowTemperatureWatch->isChecked();
//    descriptor.lowTemperature = m_ui.lowTemperature->value();

//    descriptor.highHumidityWatch = m_ui.highHumidityWatch->isChecked();
//    descriptor.highHumidity = m_ui.highHumidity->value();
//    descriptor.lowHumidityWatch = m_ui.lowHumidityWatch->isChecked();
//    descriptor.lowHumidity = m_ui.lowHumidity->value();

//    descriptor.sensorErrorsWatch = m_ui.sensorErrorsWatch->isChecked();
//    descriptor.lowSignalWatch = m_ui.lowSignalWatch->isChecked();
//    descriptor.lowVccWatch = m_ui.lowBatteryWatch->isChecked();

    descriptor.sendEmailAction = m_ui.sendEmailAction->isChecked();
    descriptor.emailRecipient = m_ui.emailRecipient->text().toUtf8().data();

    if (descriptor.name.empty())
    {
        QMessageBox::critical(this, "Error", "You need to specify a name for this report.");
        return;
    }
    int32_t reportIndex = m_db.findReportIndexByName(descriptor.name);
    if (reportIndex >= 0 && m_db.getReport(reportIndex).id == m_report.id)
    {
        QMessageBox::critical(this, "Error", QString("Report '%1' already exists.").arg(descriptor.name.c_str()));
        return;
    }
    if (descriptor.filterSensors && descriptor.sensors.empty())
    {
        QMessageBox::critical(this, "Error", "You need to specify at least one sensor.");
        return;
    }
//    if (!descriptor.highTemperatureWatch && !descriptor.lowTemperatureWatch &&
//            !descriptor.highHumidityWatch && !descriptor.lowHumidityWatch &&
//            !descriptor.sensorErrorsWatch &&
//            !descriptor.lowSignalWatch &&
//            !descriptor.lowVccWatch)
//    {
//        QMessageBox::critical(this, "Error", "You need to specify at least a trigger condition.");
//        return;
//    }

    m_report.descriptor = descriptor;
    QDialog::accept();
}

//////////////////////////////////////////////////////////////////////////

