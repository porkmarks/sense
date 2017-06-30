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

