#include "AlarmsWidget.h"
#include "ui_NewAlarmDialog.h"

AlarmsWidget::AlarmsWidget(QWidget *parent)
    : QWidget(parent)
{
    m_ui.setupUi(this);
    setEnabled(false);

    QObject::connect(m_ui.add, &QPushButton::released, this, &AlarmsWidget::addAlarm);
    QObject::connect(m_ui.remove, &QPushButton::released, this, &AlarmsWidget::removeAlarms);
}

AlarmsWidget::~AlarmsWidget()
{
    m_ui.list->setModel(nullptr);
}

void AlarmsWidget::init(Comms& comms, DB& db)
{
    m_comms = &comms;
    m_db = &db;

    m_model.reset(new AlarmsModel(db));
    m_ui.list->setModel(m_model.get());

    QObject::connect(m_comms, &Comms::baseStationConnected, this, &AlarmsWidget::baseStationConnected);
    QObject::connect(m_comms, &Comms::baseStationDisconnected, this, &AlarmsWidget::baseStationDisconnected);
}

void AlarmsWidget::addAlarm()
{
    QDialog dialog;
    Ui::NewAlarmDialog ui;
    ui.setupUi(&dialog);

    int result = dialog.exec();
    if (result == QDialog::Accepted)
    {
        DB::Alarm alarm;
        alarm.highTemperatureWatch = ui.highTemperatureWatch->isChecked();
        alarm.highTemperature = ui.highTemperature->value();
        alarm.lowTemperatureWatch = ui.lowTemperatureWatch->isChecked();
        alarm.lowTemperature = ui.lowTemperature->value();

        alarm.highHumidityWatch = ui.highHumidityWatch->isChecked();
        alarm.highHumidity = ui.highHumidity->value();
        alarm.lowHumidityWatch = ui.lowHumidityWatch->isChecked();
        alarm.lowHumidity = ui.lowHumidity->value();

        alarm.errorFlagsWatch = ui.sensorErrorsWatch->isChecked();
        alarm.signalWatch = ui.lowSignalWatch->isChecked();
        alarm.vccWatch = ui.lowBatteryWatch->isChecked();

        alarm.sendEmailAction = ui.sendEmailAction->isChecked();
        alarm.emailRecipient = ui.emailRecipient->text().toUtf8().data();

        m_db->addAlarm(alarm);
        m_model->refresh();
    }
}

void AlarmsWidget::removeAlarms()
{

}

void AlarmsWidget::baseStationConnected(Comms::BaseStation const& bs)
{
    setEnabled(true);
}

void AlarmsWidget::baseStationDisconnected(Comms::BaseStation const& bs)
{
    setEnabled(false);
}

