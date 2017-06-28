#include "SensorsWidget.h"

#include <QMessageBox>
#include <QInputDialog>

extern QIcon getBatteryIcon(float vcc);



SensorsWidget::SensorsWidget(QWidget *parent)
    : QWidget(parent)
{
    m_ui.setupUi(this);
    setEnabled(false);

    m_model.setColumnCount(10);
    m_model.setHorizontalHeaderLabels({"Name", "Id", "Address", "Temperature", "Humidity", "Battery", "State", "Next Measurement", "Next Comms", "Errors"});

    m_ui.list->setModel(&m_model);

    m_ui.list->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_ui.list->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_ui.list->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_ui.list->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_ui.list->header()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_ui.list->header()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    m_ui.list->header()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
    m_ui.list->header()->setSectionResizeMode(7, QHeaderView::ResizeToContents);
    m_ui.list->header()->setSectionResizeMode(8, QHeaderView::ResizeToContents);
    m_ui.list->header()->setSectionResizeMode(9, QHeaderView::Stretch);

    //QObject::connect(m_ui.list, &QTreeView::doubleClicked, this, &BaseStationsWidget::activateBaseStation);

    QObject::connect(m_ui.add, &QPushButton::released, this, &SensorsWidget::bindSensor);
    QObject::connect(m_ui.remove, &QPushButton::released, this, &SensorsWidget::unbindSensor);
}

void SensorsWidget::init(Comms& comms, DB& db)
{
    m_comms = &comms;
    m_db = &db;

    QObject::connect(m_comms, &Comms::sensorAdded, this, &SensorsWidget::sensorAdded);
    QObject::connect(m_comms, &Comms::baseStationConnected, this, &SensorsWidget::baseStationConnected);
    QObject::connect(m_comms, &Comms::baseStationDisconnected, this, &SensorsWidget::baseStationDisconnected);
}

void SensorsWidget::bindSensor()
{
    if (!m_comms)
    {
        return;
    }

    while (1)
    {
        bool ok = false;
        QString text = QInputDialog::getText(this, tr("Input Sensor Name"), tr("Name:"), QLineEdit::Normal, "", &ok);
        if (!ok)
        {
            return;
        }
        std::string name = text.toUtf8().data();

        if (name.empty())
        {
            QMessageBox::critical(this, "Error", "You need to specify a sensor name.");
            continue;
        }

        std::vector<Comms::Sensor> const& sensors = m_comms->getLastSensors();
        auto it = std::find_if(sensors.begin(), sensors.end(), [&name](Comms::Sensor const& sensor) { return sensor.name == name; });
        if (it != sensors.end())
        {
            QMessageBox::critical(this, "Error", QString("There is already a sensor called '%1'").arg(text));
            continue;
        }

        m_comms->requestBindSensor(name);
        return;
    }
}

void SensorsWidget::unbindSensor()
{

}

void SensorsWidget::baseStationConnected(Comms::BaseStation const& bs)
{
    setEnabled(true);
}

void SensorsWidget::baseStationDisconnected(Comms::BaseStation const& bs)
{
    setEnabled(false);
}

void SensorsWidget::sensorAdded(Comms::Sensor const& sensor)
{
    auto it = std::find_if(m_sensors.begin(), m_sensors.end(), [&sensor](Comms::Sensor const& _s) { return _s.id == sensor.id; });
    if (it != m_sensors.end())
    {
        refreshSensor(sensor);
        return;
    }

    m_sensors.push_back(sensor);

    QStandardItem* nameItem = new QStandardItem(QIcon(":/icons/ui/sensor.png"), sensor.name.c_str());
    QStandardItem* idItem = new QStandardItem();
    QStandardItem* addressItem = new QStandardItem();
    QStandardItem* temperatureItem = new QStandardItem();
    QStandardItem* humidityItem = new QStandardItem();
    QStandardItem* batteryItem = new QStandardItem();
    QStandardItem* stateItem = new QStandardItem();
    QStandardItem* nextMeasurementItem = new QStandardItem();
    QStandardItem* nextCommsItem = new QStandardItem();
    QStandardItem* errorsItem = new QStandardItem();
    m_model.appendRow({ nameItem, idItem, addressItem, temperatureItem, humidityItem, batteryItem, stateItem, nextMeasurementItem, nextCommsItem, errorsItem });

    refreshSensor(sensor);
}

void SensorsWidget::refreshSensor(Comms::Sensor const& sensor)
{
    auto it = std::find_if(m_sensors.begin(), m_sensors.end(), [&sensor](Comms::Sensor const& _s) { return _s.id == sensor.id; });
    if (it == m_sensors.end())
    {
        assert(false);
        return;
    }

    int index = std::distance(m_sensors.begin(), it);

    m_model.setItemData(m_model.index(index, 0), {{ Qt::DisplayRole, QVariant(sensor.name.c_str()) }} );
    m_model.setItemData(m_model.index(index, 1), {{ Qt::DisplayRole, QVariant(sensor.id) }} );
    m_model.setItemData(m_model.index(index, 2), {{ Qt::DisplayRole, QVariant(sensor.address) }} );

    DB::Measurement measurement;
    if (m_db->get_last_measurement_for_sensor(sensor.id, measurement))
    {
        m_model.setItemData(m_model.index(index, 3), {{ Qt::DisplayRole, measurement.temperature }} );
        m_model.setItemData(m_model.index(index, 4), {{ Qt::DisplayRole, measurement.humidity }} );
        m_model.setItemData(m_model.index(index, 5), {{ Qt::DecorationRole, getBatteryIcon(measurement.vcc) }} );
        m_model.setItemData(m_model.index(index, 6), {{ Qt::DisplayRole, "N/A" }} );
        m_model.setItemData(m_model.index(index, 7), {{ Qt::DisplayRole, QVariant("N/A") }} );
        m_model.setItemData(m_model.index(index, 8), {{ Qt::DisplayRole, QVariant("N/A") }} );
        m_model.setItemData(m_model.index(index, 9), {{ Qt::DisplayRole, QVariant("N/A") }} );
    }
    else
    {
        m_model.setItemData(m_model.index(index, 3), {{ Qt::DisplayRole, QVariant("N/A") }} );
        m_model.setItemData(m_model.index(index, 4), {{ Qt::DisplayRole, QVariant("N/A") }} );
        m_model.setItemData(m_model.index(index, 5), {{ Qt::DisplayRole, QVariant("N/A") }} );
        m_model.setItemData(m_model.index(index, 6), {{ Qt::DisplayRole, QVariant("N/A") }} );
        m_model.setItemData(m_model.index(index, 7), {{ Qt::DisplayRole, QVariant("N/A") }} );
        m_model.setItemData(m_model.index(index, 8), {{ Qt::DisplayRole, QVariant("N/A") }} );
        m_model.setItemData(m_model.index(index, 9), {{ Qt::DisplayRole, QVariant("N/A") }} );
    }
}
