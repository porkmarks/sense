#include "SensorsWidget.h"

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
}

void SensorsWidget::init(Comms& comms)
{
    m_comms = &comms;

    QObject::connect(m_comms, &Comms::sensorsReceived, this, &SensorsWidget::sensorsReceived);
    QObject::connect(m_comms, &Comms::baseStationDisconnected, this, &SensorsWidget::baseStationDisconnected);
}

void SensorsWidget::sensorsReceived(std::vector<Comms::SensorDescriptor> const& sensors)
{
    setEnabled(true);

    for (Comms::SensorDescriptor const& sensor: sensors)
    {
        addSensor(sensor);
    }
}

void SensorsWidget::baseStationDisconnected(Comms::BaseStationDescriptor const& bs)
{
    setEnabled(false);
}

void SensorsWidget::addSensor(Comms::SensorDescriptor const& sensor)
{
    auto it = std::find_if(m_sensors.begin(), m_sensors.end(), [&sensor](Comms::SensorDescriptor const& _s) { return _s.id == sensor.id; });
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

void SensorsWidget::refreshSensor(Comms::SensorDescriptor const& sensor)
{
    auto it = std::find_if(m_sensors.begin(), m_sensors.end(), [&sensor](Comms::SensorDescriptor const& _s) { return _s.id == sensor.id; });
    if (it == m_sensors.end())
    {
        assert(false);
        return;
    }

    int index = std::distance(m_sensors.begin(), it);

    m_model.setItemData(m_model.index(index, 0), {{ Qt::DisplayRole, QVariant(sensor.name.c_str()) }} );
    m_model.setItemData(m_model.index(index, 1), {{ Qt::DisplayRole, QVariant(sensor.id) }} );
    m_model.setItemData(m_model.index(index, 2), {{ Qt::DisplayRole, QVariant(sensor.address) }} );
    m_model.setItemData(m_model.index(index, 3), {{ Qt::DisplayRole, QVariant("N/A") }} );
    m_model.setItemData(m_model.index(index, 4), {{ Qt::DisplayRole, QVariant("N/A") }} );
    m_model.setItemData(m_model.index(index, 5), {{ Qt::DisplayRole, QVariant("N/A") }} );
    m_model.setItemData(m_model.index(index, 6), {{ Qt::DisplayRole, QVariant("N/A") }} );
    m_model.setItemData(m_model.index(index, 7), {{ Qt::DisplayRole, QVariant("N/A") }} );
    m_model.setItemData(m_model.index(index, 8), {{ Qt::DisplayRole, QVariant("N/A") }} );
    m_model.setItemData(m_model.index(index, 9), {{ Qt::DisplayRole, QVariant("N/A") }} );
}
