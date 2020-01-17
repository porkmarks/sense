#include "AlarmsWidget.h"
#include "ConfigureAlarmDialog.h"
#include "PermissionsCheck.h"
#include "DB.h"

#include <QMessageBox>
#include <QSettings>
#include "HTMLItemDelegate.h"

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

void AlarmsWidget::init(DB& db)
{
    for (const QMetaObject::Connection& connection: m_uiConnections)
    {
        QObject::disconnect(connection);
    }
    m_uiConnections.clear();

    setEnabled(true);

    m_db = &db;

    m_model.reset(new AlarmsModel(*m_db));
    m_ui.list->setModel(m_model.get());
    m_ui.list->setItemDelegate(new HtmlItemDelegate());

    connect(m_ui.list->header(), &QHeaderView::sectionResized, [this]()
    {
        if (!m_sectionSaveScheduled)
        {
            m_sectionSaveScheduled = true;
            QTimer::singleShot(500, [this]()
            {
                m_sectionSaveScheduled = false;
                QSettings settings;
                settings.setValue("alarms/list/state", m_ui.list->header()->saveState());
            });
        }
    });

    {
        QSettings settings;
        m_ui.list->header()->restoreState(settings.value("alarms/list/state").toByteArray());
    }

	m_ui.list->header()->setSectionHidden((int)AlarmsModel::Column::Id, true);

    setPermissions();
	m_uiConnections.push_back(connect(&db, &DB::userLoggedIn, this, &AlarmsWidget::setPermissions));
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

void AlarmsWidget::setPermissions()
{
}

//////////////////////////////////////////////////////////////////////////

void AlarmsWidget::configureAlarm(QModelIndex const& index)
{
	if (!hasPermissionOrCanLoginAsAdmin(*m_db, DB::UserDescriptor::PermissionChangeAlarms, this))
    {
		QMessageBox::critical(this, "Error", "You don't have permission to configure alarms.");
        return;
    }

    size_t indexRow = static_cast<size_t>(index.row());
    if (!index.isValid() || indexRow >= m_db->getAlarmCount())
    {
        return;
    }

    DB::Alarm alarm = m_db->getAlarm(indexRow);

    ConfigureAlarmDialog dialog(*m_db, this);
    dialog.setAlarm(alarm);
	dialog.setEnabled(m_db->isLoggedInAsAdmin());

    do
    {
        int result = dialog.exec();
        if (result == QDialog::Accepted)
        {
            alarm = dialog.getAlarm();
            Result<void> result = m_db->setAlarm(alarm.id, alarm.descriptor);
            if (result != success)
            {
                QMessageBox::critical(this, "Error", QString("Cannot change alarm '%1': %2").arg(alarm.descriptor.name.c_str()).arg(result.error().what().c_str()));
                continue;
            }
            m_model->refresh();
        }
        break;
    } while (true);
}

//////////////////////////////////////////////////////////////////////////

void AlarmsWidget::addAlarm()
{
	if (!hasPermissionOrCanLoginAsAdmin(*m_db, DB::UserDescriptor::PermissionAddRemoveAlarms, this))
    {
        QMessageBox::critical(this, "Error", "You don't have permission to add alarms.");
        return;
    }

    ConfigureAlarmDialog dialog(*m_db, this);

    DB::Alarm alarm;
    alarm.descriptor.name = "Alarm " + std::to_string(m_db->getAlarmCount());
    dialog.setAlarm(alarm);

    int result = dialog.exec();
    if (result == QDialog::Accepted)
    {
        alarm = dialog.getAlarm();
        Result<void> result = m_db->addAlarm(alarm.descriptor);
        if (result != success)
        {
            QMessageBox::critical(this, "Error", QString("Cannot add alarm '%1': %2").arg(alarm.descriptor.name.c_str()).arg(result.error().what().c_str()));
        }
        m_model->refresh();
    }
}

//////////////////////////////////////////////////////////////////////////

void AlarmsWidget::removeAlarms()
{
	if (!hasPermissionOrCanLoginAsAdmin(*m_db, DB::UserDescriptor::PermissionAddRemoveAlarms, this))
    {
        QMessageBox::critical(this, "Error", "You don't have permission to remove alarms.");
        return;
    }

    QModelIndexList selected = m_ui.list->selectionModel()->selectedIndexes();
    if (selected.isEmpty())
    {
        QMessageBox::critical(this, "Error", "Please select the alarm you want to remove.");
        return;
    }

    QModelIndex mi = m_model->index(selected.at(0).row(), static_cast<int>(AlarmsModel::Column::Id));
    DB::AlarmId id = m_model->data(mi).toUInt();
    int32_t _index = m_db->findAlarmIndexById(id);
    if (_index < 0)
    {
        QMessageBox::critical(this, "Error", "Invalid alarm selected.");
        return;
    }

    size_t index = static_cast<size_t>(_index);
    DB::Alarm alarm = m_db->getAlarm(index);

    int response = QMessageBox::question(this, "Confirmation", QString("Are you sure you want to delete alarm '%1'").arg(alarm.descriptor.name.c_str()));
    if (response != QMessageBox::Yes)
    {
        return;
    }

    m_db->removeAlarm(index);
}

//////////////////////////////////////////////////////////////////////////

