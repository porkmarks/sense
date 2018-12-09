#include "SensorsWidget.h"
#include "Settings.h"
#include "SensorDetailsDialog.h"
#include "AdminCheck.h"

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

        Result<void> result = m_db->addSensor(descriptor);
        if (result != success)
        {
            QMessageBox::critical(this, "Error", QString("Cannot add sensor '%1': %2").arg(text).arg(result.error().what().c_str()));
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

    QModelIndex mi = m_sortingModel.mapToSource(m_sortingModel.index(selected.at(0).row(), 0));
    int32_t _index = m_model->getSensorIndex(mi);
    if (_index < 0)
    {
        QMessageBox::critical(this, "Error", "Invalid sensor selected.");
        return;
    }

    size_t index = static_cast<size_t>(_index);
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
    if (!adminCheck(*m_settings))
    {
        return;
    }

    QModelIndex mi = m_sortingModel.mapToSource(index);
    int32_t _sensorIndex = m_model->getSensorIndex(mi);
    if (_sensorIndex < 0)
    {
        QMessageBox::critical(this, "Error", "Invalid sensor selected.");
        return;
    }

    size_t sensorIndex = static_cast<size_t>(_sensorIndex);
    DB::Sensor sensor = m_db->getSensor(sensorIndex);

    SensorDetailsDialog dialog(*m_db);
    dialog.setSensor(sensor);

    do
    {
        int result = dialog.exec();
        if (result == QDialog::Accepted)
        {
            sensor = dialog.getSensor();
            Result<void> result = m_db->setSensor(sensor.id, sensor.descriptor);
            if (result != success)
            {
                QMessageBox::critical(this, "Error", QString("Cannot change sensor '%1': %2").arg(sensor.descriptor.name.c_str()).arg(result.error().what().c_str()));
                continue;
            }
            result = m_db->setSensorCalibration(sensor.id, sensor.calibration);
            if (result != success)
            {
                QMessageBox::critical(this, "Error", QString("Sensor '%1' calibration failed: %2").arg(sensor.descriptor.name.c_str()).arg(result.error().what().c_str()));
                continue;
            }
            result = m_db->setSensorSleep(sensor.id, sensor.shouldSleep);
            if (result != success)
            {
                QMessageBox::critical(this, "Error", QString("Sensor '%1' sleep failed: %2").arg(sensor.descriptor.name.c_str()).arg(result.error().what().c_str()));
                continue;
            }
        }
        break;
    } while (true);
}
//////////////////////////////////////////////////////////////////////////

