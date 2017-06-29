#include "SensorsWidget.h"

#include <QMessageBox>
#include <QInputDialog>

extern QIcon getBatteryIcon(float vcc);



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

SensorsWidget::~SensorsWidget()
{
    m_ui.list->setModel(nullptr);
}

void SensorsWidget::init(Comms& comms, DB& db)
{
    m_comms = &comms;

    m_model.reset(new SensorsModel(comms, db));
    m_ui.list->setModel(m_model.get());

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

