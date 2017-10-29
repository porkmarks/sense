#include "SensorsWidget.h"
#include "Settings.h"
#include <QSettings>

#include <QMessageBox>
#include <QInputDialog>

extern QIcon getBatteryIcon(float vcc);


//////////////////////////////////////////////////////////////////////////

SensorsWidget::SensorsWidget(QWidget *parent)
    : QWidget(parent)
{
    m_ui.setupUi(this);
    setEnabled(false);

    connect(m_ui.add, &QPushButton::released, this, &SensorsWidget::bindSensor);
    connect(m_ui.remove, &QPushButton::released, this, &SensorsWidget::unbindSensor);
    connect(m_ui.list, &QTreeView::activated, this, &SensorsWidget::configureSensor);
}

//////////////////////////////////////////////////////////////////////////

SensorsWidget::~SensorsWidget()
{
    shutdown();
}

//////////////////////////////////////////////////////////////////////////

void SensorsWidget::init(Settings& settings, DB& db)
{
    for (const QMetaObject::Connection& connection: m_uiConnections)
    {
        QObject::disconnect(connection);
    }
    m_uiConnections.clear();

    setEnabled(true);

    m_settings = &settings;
    m_db = &db;
    m_model.reset(new SensorsModel(db));
    m_sortingModel.setSourceModel(m_model.get());
    m_delegate.reset(new SensorsDelegate(m_sortingModel));

    m_ui.list->setModel(&m_sortingModel);
    m_ui.list->setItemDelegate(m_delegate.get());
    m_ui.list->setUniformRowHeights(true);

    connect(m_ui.list->header(), &QHeaderView::sectionResized, [this]()
    {
        QSettings settings;
        settings.setValue("sensors/list/state", m_ui.list->header()->saveState());
    });

    {
        QSettings settings;
        m_ui.list->header()->restoreState(settings.value("sensors/list/state").toByteArray());
    }

    setRW();
    m_uiConnections.push_back(connect(&settings, &Settings::userLoggedIn, this, &SensorsWidget::setRW));
}

//////////////////////////////////////////////////////////////////////////

void SensorsWidget::shutdown()
{
    setEnabled(false);
    m_ui.list->setModel(nullptr);
    m_ui.list->setItemDelegate(nullptr);
    m_sortingModel.setSourceModel(nullptr);
    m_model.reset();
    m_delegate.reset();
}

//////////////////////////////////////////////////////////////////////////

void SensorsWidget::setRW()
{
    m_ui.add->setEnabled(m_settings->isLoggedInAsAdmin());
    m_ui.remove->setEnabled(m_settings->isLoggedInAsAdmin());
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
    QModelIndexList selected = m_ui.list->selectionModel()->selectedIndexes();
    if (selected.isEmpty())
    {
        QMessageBox::critical(this, "Error", "Please select the sensor you want to remove.");
        return;
    }

    QModelIndex mi = m_sortingModel.index(selected.at(0).row(), static_cast<int>(SensorsModel::Column::Id));
    DB::SensorId id = m_sortingModel.data(mi).toUInt();
    int32_t index = m_db->findSensorIndexById(id);
    if (index < 0)
    {
        QMessageBox::critical(this, "Error", "Invalid sensor selected.");
        return;
    }

    DB::Sensor const& sensor = m_db->getSensor(index);

    int response = QMessageBox::question(this, "Confirmation", QString("Are you sure you want to delete sensor '%1'").arg(sensor.descriptor.name.c_str()));
    if (response != QMessageBox::Yes)
    {
        return;
    }

    m_db->removeSensor(index);
}

//////////////////////////////////////////////////////////////////////////

void SensorsWidget::configureSensor(QModelIndex const& index)
{
    QModelIndex mi = m_sortingModel.index(index.row(), static_cast<int>(SensorsModel::Column::Id));
    DB::SensorId id = m_sortingModel.data(mi).toUInt();
    int32_t sensorIndex = m_db->findSensorIndexById(id);
    if (sensorIndex < 0)
    {
        QMessageBox::critical(this, "Error", "Invalid sensor selected.");
        return;
    }

    DB::Sensor sensor = m_db->getSensor(sensorIndex);

    while (true)
    {
        bool ok = false;
        QString text = QInputDialog::getText(this, tr("Input Sensor Name"), tr("Name:"), QLineEdit::Normal, sensor.descriptor.name.c_str(), &ok);
        if (!ok)
        {
            return;
        }
        if (text.isEmpty())
        {
            QMessageBox::critical(this, "Error", "Please enter a name.");
            continue;
        }
        sensor.descriptor.name = text.toUtf8().data();

        if (!m_db->setSensor(sensor.id, sensor.descriptor))
        {
            QMessageBox::critical(this, "Error", QString("Cannot rename sensor to '%1'.").arg(sensor.descriptor.name.c_str()));
            continue;
        }
        break;
    }
}
//////////////////////////////////////////////////////////////////////////

