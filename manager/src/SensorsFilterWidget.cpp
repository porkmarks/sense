#include "SensorsFilterWidget.h"
#include <QSettings>
#include <QAbstractItemModel>
#include <QCheckBox>
#include <QTimer>

SensorsFilterWidget::SensorsFilterWidget(QWidget *parent) : QWidget(parent)
{
    m_ui.setupUi(this);
}

void SensorsFilterWidget::init(DB& db)
{
    m_db = &db;

    m_model.reset(new SensorsModel(*m_db));
    m_model->setShowCheckboxes(true);
    
    m_sortingModel.reset(new QSortFilterProxyModel(this));
    m_sortingModel->setSourceModel(m_model.get());

    m_delegate.reset(new SensorsDelegate(*m_sortingModel));

    size_t sensorCount = m_db->getSensorCount();

    connect(m_ui.selectAll, &QPushButton::released, [sensorCount, this]()
    {
        for (size_t i = 0; i < sensorCount; i++)
        {
            m_model->setSensorChecked(m_db->getSensor(i).id, true);
        }
    });
    connect(m_ui.selectNone, &QPushButton::released, [sensorCount, this]()
    {
        for (size_t i = 0; i < sensorCount; i++)
        {
            m_model->setSensorChecked(m_db->getSensor(i).id, false);
        }
    });

    m_ui.list->setModel(m_sortingModel.get());
    m_ui.list->setItemDelegate(m_delegate.get());

    connect(m_ui.list->header(), &QHeaderView::sectionResized, [this]()
    {
        if (!m_sectionSaveScheduled)
        {
            m_sectionSaveScheduled = true;
            QTimer::singleShot(500, [this]()
            {
                m_sectionSaveScheduled = false;
                QSettings settings;
                settings.setValue("sensorFilter/list/state", m_ui.list->header()->saveState());
            });
        }
    });

    {
        QSettings settings;
        m_ui.list->header()->restoreState(settings.value("sensorFilter/list/state").toByteArray());
    }

	m_ui.list->header()->setSectionHidden((int)SensorsModel::Column::Battery, true);
	m_ui.list->header()->setSectionHidden((int)SensorsModel::Column::Signal, true);
	m_ui.list->header()->setSectionHidden((int)SensorsModel::Column::NextComms, true);
	m_ui.list->header()->setSectionHidden((int)SensorsModel::Column::Stored, true);
	m_ui.list->header()->setSectionHidden((int)SensorsModel::Column::Alarms, true);
}

SensorsModel& SensorsFilterWidget::getSensorModel()
{
    return *m_model;
}
