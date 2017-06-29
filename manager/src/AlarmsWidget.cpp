#include "AlarmsWidget.h"

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

void AlarmsWidget::init(Comms& comms, DB& db, Alarms& alarms)
{
    m_comms = &comms;
    m_alarms = &alarms;

    m_model.reset(new AlarmsModel(alarms));
    m_ui.list->setModel(m_model.get());

    QObject::connect(m_comms, &Comms::baseStationConnected, this, &AlarmsWidget::baseStationConnected);
    QObject::connect(m_comms, &Comms::baseStationDisconnected, this, &AlarmsWidget::baseStationDisconnected);
}

void AlarmsWidget::addAlarm()
{

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

