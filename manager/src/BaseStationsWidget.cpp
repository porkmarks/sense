#include "BaseStationsWidget.h"

BaseStationsWidget::BaseStationsWidget(QWidget *parent)
    : QWidget(parent)
{
    m_ui.setupUi(this);

}

void BaseStationsWidget::init(Comms& comms)
{
    m_comms = &comms;

    QObject::connect(m_comms, &Comms::baseStationDiscovered, this, &BaseStationsWidget::baseStationDiscovered);
}

void BaseStationsWidget::baseStationDiscovered(std::array<uint8_t, 6> const& mac, QHostAddress const& address, uint16_t port)
{
    char buf[128];
    sprintf(buf, "Mac: %X:%X:%X:%X:%X:%X\tIp: %s\t Port: %d", mac[0]&0xFF, mac[1]&0xFF, mac[2]&0xFF, mac[3]&0xFF, mac[4]&0xFF, mac[5]&0xFF,
            address.toString().toLatin1().data(), port);

    QList<QListWidgetItem*> items = m_ui.list->findItems(QString(buf), Qt::MatchExactly);
    if (!items.empty())
    {
        return;
    }

    QListWidgetItem* item = new QListWidgetItem(m_ui.list);

    item->setText(buf);

//    uint64_t hash = ((uint64_t)mac[0] << 48) | ((uint64_t)mac[1] << 32) | ((uint64_t)mac[2] << 24) |
//            ((uint64_t)mac[3] << 16) | ((uint64_t)mac[4] << 8) | ((uint64_t)mac[5]);
//    item->setData(Qt::UserRole, hash);
    m_ui.list->addItem(item);
}

