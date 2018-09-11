#include "SensorsFilterWidget.h"
#include <QSettings>
#include <QAbstractItemModel>

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

    updateSelectionCheckboxes();

    connect(m_model.get(), &SensorsModel::sensorCheckedChanged, this, &SensorsFilterWidget::updateSelectionCheckboxes);
    connect(m_ui.selectAll, &QCheckBox::stateChanged, [sensorCount, this]()
    {
        if (m_ui.selectAll->checkState() != Qt::PartiallyChecked)
        {
            for (size_t i = 0; i < sensorCount; i++)
            {
                m_model->setSensorChecked(m_db->getSensor(i).id, m_ui.selectAll->isChecked());
            }
        }
    });

    m_ui.list->setModel(m_sortingModel.get());
    m_ui.list->setItemDelegate(m_delegate.get());

    connect(m_ui.list->header(), &QHeaderView::sectionResized, [this]()
    {
        QSettings settings;
        settings.setValue("sensorFilter/list/state", m_ui.list->header()->saveState());
    });

    {
        QSettings settings;
        m_ui.list->header()->restoreState(settings.value("sensorFilter/list/state").toByteArray());
    }
}

void SensorsFilterWidget::updateSelectionCheckboxes()
{
    size_t sensorCount = m_db->getSensorCount();

    bool allSensorsChecked = true;
    bool anySensorsChecked = false;
    for (size_t i = 0; i < sensorCount; i++)
    {
        bool isChecked = m_model->isSensorChecked(m_db->getSensor(i).id);
        allSensorsChecked &= isChecked;
        anySensorsChecked |= isChecked;
    }
    m_ui.selectAll->blockSignals(true);
    m_ui.selectAll->setCheckState(allSensorsChecked ? Qt::Checked : (anySensorsChecked ? Qt::PartiallyChecked : Qt::Unchecked));
    m_ui.selectAll->blockSignals(false);
}

SensorsModel& SensorsFilterWidget::getSensorModel()
{
    return *m_model;
}
