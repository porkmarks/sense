#include "AlarmsWidget.h"
#include "ConfigureAlarmDialog.h"
#include "Settings.h"
#include <QMessageBox>

#include "DB.h"

//////////////////////////////////////////////////////////////////////////

AlarmsWidget::AlarmsWidget(QWidget *parent)
    : QWidget(parent)
{
    m_ui.setupUi(this);
    setEnabled(false);

    connect(m_ui.add, &QPushButton::released, this, &AlarmsWidget::addAlarm);
    connect(m_ui.remove, &QPushButton::released, this, &AlarmsWidget::removeAlarms);
    connect(m_ui.list, &QTreeView::activated, this, &AlarmsWidget::configureAlarm);
}

//////////////////////////////////////////////////////////////////////////

AlarmsWidget::~AlarmsWidget()
{
    shutdown();
}

//////////////////////////////////////////////////////////////////////////

void AlarmsWidget::init(Settings& settings, DB& db)
{
    setEnabled(true);

    m_db = &db;
    m_settings = &settings;

    m_model.reset(new AlarmsModel(db));
    m_ui.list->setModel(m_model.get());

    for (int i = 0; i < m_model->columnCount(QModelIndex()); i++)
    {
        m_ui.list->resizeColumnToContents(i);
    }

    setRW();
    connect(&settings, &Settings::userLoggedIn, this, &AlarmsWidget::setRW);
}

//////////////////////////////////////////////////////////////////////////

void AlarmsWidget::shutdown()
{
    setEnabled(false);
    m_ui.list->setModel(nullptr);
//    m_ui.list->setItemDelegate(nullptr);
    m_model.reset();
    m_db = nullptr;
}

//////////////////////////////////////////////////////////////////////////

void AlarmsWidget::setRW()
{
    //available to normal users as well
//    m_ui.add->setEnabled(m_settings->isLoggedInAsAdmin());
//    m_ui.remove->setEnabled(m_settings->isLoggedInAsAdmin());
}

//////////////////////////////////////////////////////////////////////////

void AlarmsWidget::configureAlarm(QModelIndex const& index)
{
    if (!index.isValid() || static_cast<size_t>(index.row()) >= m_db->getAlarmCount())
    {
        return;
    }

    DB::Alarm alarm = m_db->getAlarm(index.row());

    ConfigureAlarmDialog dialog(*m_db);
    dialog.setAlarm(alarm);
    dialog.setEnabled(m_settings->isLoggedInAsAdmin());

    int result = dialog.exec();
    if (result == QDialog::Accepted)
    {
        alarm = dialog.getAlarm();
        m_db->setAlarm(alarm.id, alarm.descriptor);
        m_model->refresh();

        for (int i = 0; i < m_model->columnCount(QModelIndex()); i++)
        {
            m_ui.list->resizeColumnToContents(i);
        }
    }
}

//////////////////////////////////////////////////////////////////////////

void AlarmsWidget::addAlarm()
{
    ConfigureAlarmDialog dialog(*m_db);

    DB::Alarm alarm;
    alarm.descriptor.name = "Alarm " + std::to_string(m_db->getAlarmCount());
    dialog.setAlarm(alarm);

    int result = dialog.exec();
    if (result == QDialog::Accepted)
    {
        alarm = dialog.getAlarm();
        m_db->addAlarm(alarm.descriptor);
        m_model->refresh();

        for (int i = 0; i < m_model->columnCount(QModelIndex()); i++)
        {
            m_ui.list->resizeColumnToContents(i);
        }
    }
}

//////////////////////////////////////////////////////////////////////////

void AlarmsWidget::removeAlarms()
{
    QModelIndexList selected = m_ui.list->selectionModel()->selectedIndexes();
    if (selected.isEmpty())
    {
        QMessageBox::critical(this, "Error", "Please select the alarm you want to remove.");
        return;
    }

    QModelIndex mi = m_model->index(selected.at(0).row(), static_cast<int>(AlarmsModel::Column::Id));
    DB::AlarmId id = m_model->data(mi).toUInt();
    int32_t index = m_db->findAlarmIndexById(id);
    if (index < 0)
    {
        QMessageBox::critical(this, "Error", "Invalid alarm selected.");
        return;
    }

    DB::Alarm const& alarm = m_db->getAlarm(index);

    int response = QMessageBox::question(this, "Confirmation", QString("Are you sure you want to delete alarm '%1'").arg(alarm.descriptor.name.c_str()));
    if (response != QMessageBox::Yes)
    {
        return;
    }

    m_db->removeAlarm(index);

    for (int i = 0; i < m_model->columnCount(QModelIndex()); i++)
    {
        m_ui.list->resizeColumnToContents(i);
    }
}

//////////////////////////////////////////////////////////////////////////

