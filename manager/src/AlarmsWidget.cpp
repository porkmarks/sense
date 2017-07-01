#include "AlarmsWidget.h"
#include "ui_NewAlarmDialog.h"


//////////////////////////////////////////////////////////////////////////

AlarmsWidget::AlarmsWidget(QWidget *parent)
    : QWidget(parent)
{
    m_ui.setupUi(this);
    setEnabled(false);

    QObject::connect(m_ui.add, &QPushButton::released, this, &AlarmsWidget::addAlarm);
    QObject::connect(m_ui.remove, &QPushButton::released, this, &AlarmsWidget::removeAlarms);
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

void AlarmsWidget::addAlarm()
{
    QDialog dialog;
    Ui::NewAlarmDialog ui;
    ui.setupUi(&dialog);

    ui.name->setText(QString("Alarm %1").arg(m_db->getAlarmCount()));

    while (1)
    {
        int result = dialog.exec();
        if (result == QDialog::Accepted)
        {
            DB::AlarmDescriptor alarm;
            alarm.name = ui.name->text().toUtf8().data();

            if (!ui.highTemperatureWatch->isChecked() && !ui.lowTemperatureWatch->isChecked() &&
                    !ui.highHumidityWatch->isChecked() && !ui.lowHumidityWatch->isChecked() &&
                    !ui.sensorErrorsWatch->isChecked() &&
                    !ui.lowSignalWatch->isChecked() &&
                    !ui.lowBatteryWatch->isChecked())
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
                alarm.highTemperatureWatch = ui.highTemperatureWatch->isChecked();
                alarm.highTemperature = ui.highTemperature->value();
                alarm.lowTemperatureWatch = ui.lowTemperatureWatch->isChecked();
                alarm.lowTemperature = ui.lowTemperature->value();

                alarm.highHumidityWatch = ui.highHumidityWatch->isChecked();
                alarm.highHumidity = ui.highHumidity->value();
                alarm.lowHumidityWatch = ui.lowHumidityWatch->isChecked();
                alarm.lowHumidity = ui.lowHumidity->value();

                alarm.sensorErrorsWatch = ui.sensorErrorsWatch->isChecked();
                alarm.lowSignalWatch = ui.lowSignalWatch->isChecked();
                alarm.lowVccWatch = ui.lowBatteryWatch->isChecked();

                alarm.sendEmailAction = ui.sendEmailAction->isChecked();
                alarm.emailRecipient = ui.emailRecipient->text().toUtf8().data();

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
}

//////////////////////////////////////////////////////////////////////////

void AlarmsWidget::removeAlarms()
{

}

//////////////////////////////////////////////////////////////////////////
