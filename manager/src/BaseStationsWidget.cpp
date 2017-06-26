#include "BaseStationsWidget.h"

BaseStationsWidget::BaseStationsWidget(QWidget *parent)
    : QWidget(parent)
{
    m_ui.setupUi(this);
    m_ui.list->setModel(&m_model);

    m_model.setColumnCount(3);
    m_model.setHorizontalHeaderLabels({"MAC", "IP", "Port"});

    m_ui.list->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_ui.list->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_ui.list->header()->setSectionResizeMode(2, QHeaderView::Stretch);
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

    uint64_t hash = ((uint64_t)mac[0] << 48) | ((uint64_t)mac[1] << 32) | ((uint64_t)mac[2] << 24) |
            ((uint64_t)mac[3] << 16) | ((uint64_t)mac[4] << 8) | ((uint64_t)mac[5]);

    auto it = std::find(m_baseStations.begin(), m_baseStations.end(), hash);
    if (it != m_baseStations.end())
    {
        return;
    }

    int row = m_baseStations.size();
    m_baseStations.push_back(hash);

    QStandardItem* macItem = new QStandardItem();
    QStandardItem* ipItem = new QStandardItem();
    QStandardItem* portItem = new QStandardItem();

    {
        char buf[128];
        sprintf(buf, "%X:%X:%X:%X:%X:%X", mac[0]&0xFF, mac[1]&0xFF, mac[2]&0xFF, mac[3]&0xFF, mac[4]&0xFF, mac[5]&0xFF);
        macItem->setText(buf);
    }

    {
        char buf[128];
        sprintf(buf, "%s", address.toString().toLatin1().data());
        ipItem->setText(buf);
    }

    {
        portItem->setData(port, Qt::DisplayRole);
    }

    m_model.appendRow({ macItem, ipItem, portItem});
}

