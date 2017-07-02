#include "SensorsWidget.h"

#include <QMessageBox>
#include <QInputDialog>

extern QIcon getBatteryIcon(float vcc);


//////////////////////////////////////////////////////////////////////////

SensorsWidget::SensorsWidget(QWidget *parent)
    : QWidget(parent)
{
    m_ui.setupUi(this);
    setEnabled(false);

//    m_ui.list->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
//    m_ui.list->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
//    m_ui.list->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
//    m_ui.list->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
//    m_ui.list->header()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
//    m_ui.list->header()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
//    m_ui.list->header()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
//    m_ui.list->header()->setSectionResizeMode(7, QHeaderView::ResizeToContents);
//    m_ui.list->header()->setSectionResizeMode(8, QHeaderView::ResizeToContents);
//    m_ui.list->header()->setSectionResizeMode(9, QHeaderView::Stretch);

    //QObject::connect(m_ui.list, &QTreeView::doubleClicked, this, &BaseStationsWidget::activateBaseStation);

    QObject::connect(m_ui.add, &QPushButton::released, this, &SensorsWidget::bindSensor);
    QObject::connect(m_ui.remove, &QPushButton::released, this, &SensorsWidget::unbindSensor);
}

//////////////////////////////////////////////////////////////////////////

SensorsWidget::~SensorsWidget()
{
    shutdown();
}

//////////////////////////////////////////////////////////////////////////

void SensorsWidget::init(DB& db)
{
    setEnabled(true);
    m_db = &db;
    m_model.reset(new SensorsModel(db));
    m_ui.list->setModel(m_model.get());
    m_ui.list->setItemDelegate(m_model.get());

    for (int i = 0; i < m_model->columnCount(QModelIndex()); i++)
    {
        m_ui.list->resizeColumnToContents(i);
    }
}

//////////////////////////////////////////////////////////////////////////

void SensorsWidget::shutdown()
{
    setEnabled(false);
    m_ui.list->setModel(nullptr);
    m_ui.list->setItemDelegate(nullptr);
    m_model.reset();
}

//////////////////////////////////////////////////////////////////////////

void SensorsWidget::bindSensor()
{
    if (!m_db)
    {
        return;
    }

    while (1)
    {
        DB::SensorDescriptor descriptor;
        bool ok = false;
        QString text = QInputDialog::getText(this, tr("Input Sensor Name"), tr("Name:"), QLineEdit::Normal, "", &ok);
        if (!ok)
        {
            return;
        }
        descriptor.name = text.toUtf8().data();
        if (descriptor.name.empty())
        {
            QMessageBox::critical(this, "Error", "You need to specify a sensor name.");
            continue;
        }

        if (!m_db->addSensor(descriptor))
        {
            QMessageBox::critical(this, "Error", QString("Cannot add sensor '%1'").arg(text));
            continue;
        }

        return;
    }
}

//////////////////////////////////////////////////////////////////////////

void SensorsWidget::unbindSensor()
{

}

//////////////////////////////////////////////////////////////////////////

