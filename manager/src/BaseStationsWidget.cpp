#include "BaseStationsWidget.h"

BaseStationsWidget::BaseStationsWidget(QWidget *parent)
    : QWidget(parent)
{
    m_ui.setupUi(this);
    m_ui.list->setModel(&m_model);

    m_model.setColumnCount(4);
    m_model.setHorizontalHeaderLabels({"", "MAC", "IP", "Port"});

    m_ui.list->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_ui.list->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_ui.list->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_ui.list->header()->setSectionResizeMode(3, QHeaderView::Stretch);
}

void BaseStationsWidget::init(Comms& comms)
{
    m_comms = &comms;

    QObject::connect(m_comms, &Comms::baseStationDiscovered, this, &BaseStationsWidget::baseStationDiscovered);
}

void BaseStationsWidget::baseStationDiscovered(std::array<uint8_t, 6> const& mac, QHostAddress const& address, uint16_t port)
{
//    QList<QListWidgetItem*> items = m_ui.list->findItems(QString(buf), Qt::MatchExactly);
//    if (!items.empty())
//    {
//        return;
//    }

    auto it = std::find_if(m_baseStations.begin(), m_baseStations.end(), [mac](BaseStation const& bs) { return bs.mac == mac; });
    if (it != m_baseStations.end())
    {
        return;
    }

    BaseStation bs = { mac, address, port };
    m_baseStations.push_back(bs);

    QStandardItem* selectedItem = new QStandardItem();
    QStandardItem* macItem = new QStandardItem();
    QStandardItem* ipItem = new QStandardItem();
    QStandardItem* portItem = new QStandardItem();

    selectedItem->setIcon(QIcon(":/icons/ui/arrow-right.png"));

    {
        char buf[128];
        sprintf(buf, "  %X:%X:%X:%X:%X:%X  ", mac[0]&0xFF, mac[1]&0xFF, mac[2]&0xFF, mac[3]&0xFF, mac[4]&0xFF, mac[5]&0xFF);
        macItem->setText(buf);
        macItem->setIcon(QIcon(":/icons/ui/station.png"));
        macItem->setEditable(false);
    }

    {
        char buf[128];
        sprintf(buf, "  %s  ", address.toString().toLatin1().data());
        ipItem->setText(buf);
        ipItem->setIcon(QIcon(":/icons/ui/ip.png"));
        ipItem->setEditable(false);
    }

    {
        portItem->setData(port, Qt::DisplayRole);
        portItem->setIcon(QIcon(":/icons/ui/port.png"));
        portItem->setEditable(false);
    }

    m_model.appendRow({ selectedItem, macItem, ipItem, portItem});
}

