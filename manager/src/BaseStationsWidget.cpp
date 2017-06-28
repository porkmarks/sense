#include "BaseStationsWidget.h"
#include <cassert>

BaseStationsWidget::BaseStationsWidget(QWidget *parent)
    : QWidget(parent)
{
    m_ui.setupUi(this);
    m_ui.list->setModel(&m_model);

    m_model.setColumnCount(3);
    m_model.setHorizontalHeaderLabels({"", "MAC", "IP"});

    m_ui.list->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_ui.list->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_ui.list->header()->setSectionResizeMode(2, QHeaderView::Stretch);

    QObject::connect(m_ui.list, &QTreeView::doubleClicked, this, &BaseStationsWidget::activateBaseStation);
}

void BaseStationsWidget::init(Comms& comms)
{
    m_comms = &comms;

    QObject::connect(m_comms, &Comms::baseStationDiscovered, this, &BaseStationsWidget::baseStationDiscovered);
    QObject::connect(m_comms, &Comms::baseStationConnected, this, &BaseStationsWidget::baseStationConnected);
}

void BaseStationsWidget::activateBaseStation(QModelIndex const& index)
{
    QStandardItem* item = m_model.itemFromIndex(index);
    if (!item)
    {
        return;
    }

    QStandardItem* selectionItem = m_model.item(item->row(), 0);
    if (!selectionItem)
    {
        return;
    }

    uint32_t bsIndex = selectionItem->data().toUInt();
    assert(bsIndex < m_baseStations.size());
    if (bsIndex >= m_baseStations.size())
    {
        return;
    }

    emit baseStationSelected(m_baseStations[bsIndex]);
}

void BaseStationsWidget::baseStationConnected(Comms::BaseStation const& bs)
{
    auto it = std::find_if(m_baseStations.begin(), m_baseStations.end(), [&bs](Comms::BaseStation const& _bs) { return _bs.mac == bs.mac; });
    if (it == m_baseStations.end())
    {
        assert(false);
        return;
    }

    size_t selectedBsIndex = std::distance(m_baseStations.begin(), it);

    for (int i = 0; i < m_model.rowCount(); i++)
    {
        QStandardItem* si = m_model.item(i, 0);
        if (!si)
        {
            continue;
        }
        uint32_t bsIndex = si->data().toUInt();
        if (bsIndex != selectedBsIndex)
        {
            si->setIcon(QIcon());
        }
        else
        {
            si->setIcon(QIcon(":/icons/ui/arrow-right.png"));
        }
    }
}

void BaseStationsWidget::baseStationDisconnected(Comms::BaseStation const& bs)
{
    for (int i = 0; i < m_model.rowCount(); i++)
    {
        QStandardItem* si = m_model.item(i, 0);
        if (!si)
        {
            continue;
        }
        si->setIcon(QIcon());
    }
}

void BaseStationsWidget::baseStationDiscovered(Comms::BaseStation const& bs)
{
//    QList<QListWidgetItem*> items = m_ui.list->findItems(QString(buf), Qt::MatchExactly);
//    if (!items.empty())
//    {
//        return;
//    }

    auto it = std::find_if(m_baseStations.begin(), m_baseStations.end(), [&bs](Comms::BaseStation const& _bs) { return _bs.mac == bs.mac; });
    if (it != m_baseStations.end())
    {
        return;
    }

    m_baseStations.push_back(bs);

    QStandardItem* selectionItem = new QStandardItem();
    QStandardItem* macItem = new QStandardItem();
    QStandardItem* ipItem = new QStandardItem();

    {
        char buf[128];
        sprintf(buf, "  %X:%X:%X:%X:%X:%X  ", bs.mac[0]&0xFF, bs.mac[1]&0xFF, bs.mac[2]&0xFF, bs.mac[3]&0xFF, bs.mac[4]&0xFF, bs.mac[5]&0xFF);
        macItem->setText(buf);
        macItem->setIcon(QIcon(":/icons/ui/station.png"));
        macItem->setEditable(false);
    }

    {
        char buf[128];
        sprintf(buf, "  %s  ", bs.address.toString().toLatin1().data());
        ipItem->setText(buf);
        ipItem->setIcon(QIcon(":/icons/ui/ip.png"));
        ipItem->setEditable(false);
    }

    m_model.appendRow({ selectionItem, macItem, ipItem });
}

