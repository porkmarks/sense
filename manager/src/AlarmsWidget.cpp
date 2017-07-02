#include "AlarmsWidget.h"


//////////////////////////////////////////////////////////////////////////

AlarmsWidget::AlarmsWidget(QWidget *parent)
    : QWidget(parent)
{
    m_ui.setupUi(this);
    setEnabled(false);

    connect(m_ui.add, &QPushButton::released, this, &AlarmsWidget::addAlarm);
    connect(m_ui.remove, &QPushButton::released, this, &AlarmsWidget::removeAlarms);
}

//////////////////////////////////////////////////////////////////////////

AlarmsWidget::~AlarmsWidget()
{
    shutdown();
}

//////////////////////////////////////////////////////////////////////////

void AlarmsWidget::init(DB& db)
{
    setEnabled(true);
    m_db = &db;

    m_model.reset(new AlarmsModel(db));
    m_ui.list->setModel(m_model.get());

    for (int i = 0; i < m_model->columnCount(QModelIndex()); i++)
    {
        m_ui.list->resizeColumnToContents(i);
    }
}

//////////////////////////////////////////////////////////////////////////

void AlarmsWidget::shutdown()
{
    setEnabled(false);
    m_ui.list->setModel(nullptr);
    m_ui.list->setItemDelegate(nullptr);
    m_model.reset();
    m_db = nullptr;
}

//////////////////////////////////////////////////////////////////////////

void AlarmsWidget::prepareAlarmDialog(AlarmDialog& dialog)
{
    dialog.ui.setupUi(&dialog.dialog);
    dialog.model.setShowCheckboxes(true);
    dialog.sortingModel.setSourceModel(&dialog.model);

    size_t sensorCount = m_db->getSensorCount();
    for (size_t i = 0; i < sensorCount; i++)
    {
        dialog.model.setSensorChecked(m_db->getSensor(i).id, true);
    }

    dialog.ui.sensorList->setModel(&dialog.sortingModel);
    dialog.ui.sensorList->setItemDelegate(&dialog.delegate);

    for (int i = 0; i < dialog.model.columnCount(); i++)
    {
        dialog.ui.sensorList->header()->setSectionResizeMode(i, QHeaderView::ResizeToContents);
    }
    dialog.ui.sensorList->header()->setStretchLastSection(true);
}

//////////////////////////////////////////////////////////////////////////

void AlarmsWidget::getAlarmData(DB::AlarmDescriptor& alarm, AlarmDialog const& dialog)
{
    alarm.name = dialog.ui.name->text().toUtf8().data();

    alarm.filterSensors = dialog.ui.sensorGroup->isChecked();
    alarm.sensors.clear();
    if (alarm.filterSensors)
    {
        size_t sensorCount = m_db->getSensorCount();
        for (size_t i = 0; i < sensorCount; i++)
        {
            DB::Sensor const& sensor = m_db->getSensor(i);
            if (dialog.model.isSensorChecked(sensor.id))
            {
                alarm.sensors.push_back(sensor.id);
            }
        }
    }

    alarm.highTemperatureWatch = dialog.ui.highTemperatureWatch->isChecked();
    alarm.highTemperature = dialog.ui.highTemperature->value();
    alarm.lowTemperatureWatch = dialog.ui.lowTemperatureWatch->isChecked();
    alarm.lowTemperature = dialog.ui.lowTemperature->value();

    alarm.highHumidityWatch = dialog.ui.highHumidityWatch->isChecked();
    alarm.highHumidity = dialog.ui.highHumidity->value();
    alarm.lowHumidityWatch = dialog.ui.lowHumidityWatch->isChecked();
    alarm.lowHumidity = dialog.ui.lowHumidity->value();

    alarm.sensorErrorsWatch = dialog.ui.sensorErrorsWatch->isChecked();
    alarm.lowSignalWatch = dialog.ui.lowSignalWatch->isChecked();
    alarm.lowVccWatch = dialog.ui.lowBatteryWatch->isChecked();

    alarm.sendEmailAction = dialog.ui.sendEmailAction->isChecked();
    alarm.emailRecipient = dialog.ui.emailRecipient->text().toUtf8().data();
}

//////////////////////////////////////////////////////////////////////////

void AlarmsWidget::setAlarmData(AlarmDialog& dialog, DB::AlarmDescriptor const& alarm)
{
    dialog.ui.name->setText(alarm.name.c_str());

    dialog.ui.sensorGroup->setChecked(alarm.filterSensors);
    if (alarm.filterSensors)
    {
        size_t sensorCount = m_db->getSensorCount();
        for (size_t i = 0; i < sensorCount; i++)
        {
            dialog.model.setSensorChecked(m_db->getSensor(i).id, false);
        }

        for (DB::SensorId id: alarm.sensors)
        {
            dialog.model.setSensorChecked(id, true);
        }
    }
    else
    {
        size_t sensorCount = m_db->getSensorCount();
        for (size_t i = 0; i < sensorCount; i++)
        {
            dialog.model.setSensorChecked(m_db->getSensor(i).id, true);
        }
    }

    dialog.ui.highTemperatureWatch->setChecked(alarm.highTemperatureWatch);
    dialog.ui.highTemperature->setValue(alarm.highTemperature);
    dialog.ui.lowTemperatureWatch->setChecked(alarm.lowTemperatureWatch);
    dialog.ui.lowTemperature->setValue(alarm.lowTemperature);

    dialog.ui.highHumidityWatch->setChecked(alarm.highHumidityWatch);
    dialog.ui.highHumidity->setValue(alarm.highHumidity);
    dialog.ui.lowHumidityWatch->setChecked(alarm.lowHumidityWatch);
    dialog.ui.lowHumidity->setValue(alarm.lowHumidity);

    dialog.ui.sensorErrorsWatch->setChecked(alarm.sensorErrorsWatch);
    dialog.ui.lowSignalWatch->setChecked(alarm.lowSignalWatch);
    dialog.ui.lowBatteryWatch->setChecked(alarm.lowVccWatch);

    dialog.ui.sendEmailAction->setChecked(alarm.sendEmailAction);
    dialog.ui.emailRecipient->setText(alarm.emailRecipient.c_str());
}

//////////////////////////////////////////////////////////////////////////

void AlarmsWidget::addAlarm()
{
    AlarmDialog dialog(*m_db);

    prepareAlarmDialog(dialog);

    dialog.ui.name->setText(QString("Alarm %1").arg(m_db->getAlarmCount()));

    while (1)
    {
        int result = dialog.dialog.exec();
        if (result == QDialog::Accepted)
        {
            DB::AlarmDescriptor alarm;
            getAlarmData(alarm, dialog);

            if (alarm.filterSensors && alarm.sensors.empty())
            {
                QMessageBox::critical(this, "Error", "You need to specify at least one sensor.");
                continue;
            }

            if (!alarm.highTemperatureWatch && !alarm.lowTemperatureWatch &&
                    !alarm.highHumidityWatch && !alarm.lowHumidityWatch &&
                    !alarm.sensorErrorsWatch &&
                    !alarm.lowSignalWatch &&
                    !alarm.lowVccWatch)
            {
                QMessageBox::critical(this, "Error", "You need to specify at least a trigger condition.");
                continue;
            }
            else if (m_db->findAlarmIndexByName(alarm.name) >= 0)
            {
                QMessageBox::critical(this, "Error", QString("Alarm '%1' already exists.").arg(alarm.name.c_str()));
                continue;
            }
            else
            {
                m_db->addAlarm(alarm);
                m_model->refresh();
                break;
            }
        }
        else
        {
            break;
        }
    }

    for (int i = 0; i < m_model->columnCount(QModelIndex()); i++)
    {
        m_ui.list->resizeColumnToContents(i);
    }
}

//////////////////////////////////////////////////////////////////////////

void AlarmsWidget::removeAlarms()
{
    for (int i = 0; i < m_model->columnCount(QModelIndex()); i++)
    {
        m_ui.list->resizeColumnToContents(i);
    }
}

//////////////////////////////////////////////////////////////////////////

