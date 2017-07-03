#include "AlarmsWidget.h"
#include "ConfigureAlarmDialog.h"

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

void AlarmsWidget::configureAlarm(QModelIndex const& index)
{
    if (!index.isValid() || static_cast<size_t>(index.row()) >= m_db->getAlarmCount())
    {
        return;
    }

    DB::Alarm alarm = m_db->getAlarm(index.row());

    ConfigureAlarmDialog dialog(*m_db);
    dialog.setAlarm(alarm);

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

    int result = dialog.exec();
    if (result == QDialog::Accepted)
    {
        DB::Alarm alarm = dialog.getAlarm();
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
    for (int i = 0; i < m_model->columnCount(QModelIndex()); i++)
    {
        m_ui.list->resizeColumnToContents(i);
    }
}

//////////////////////////////////////////////////////////////////////////

