#include "BaseStationsWidget.h"
#include <cassert>


//////////////////////////////////////////////////////////////////////////

BaseStationsWidget::BaseStationsWidget(QWidget *parent)
    : QWidget(parent)
{
    m_ui.setupUi(this);
    m_ui.list->setModel(&m_model);

    m_model.setColumnCount(3);
    m_model.setHorizontalHeaderLabels({"Name", "MAC", "IP"});

    m_ui.list->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_ui.list->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_ui.list->header()->setSectionResizeMode(2, QHeaderView::Stretch);

    connect(m_ui.list, &QTreeView::doubleClicked, this, &BaseStationsWidget::activateBaseStation);
}

//////////////////////////////////////////////////////////////////////////

BaseStationsWidget::~BaseStationsWidget()
{
    m_ui.list->setModel(nullptr);
    m_comms = nullptr;
    m_baseStations.clear();
}

//////////////////////////////////////////////////////////////////////////

void BaseStationsWidget::init(Comms& comms)
{
    m_comms = &comms;

    connect(m_comms, &Comms::baseStationDiscovered, this, &BaseStationsWidget::baseStationDiscovered);
    connect(m_comms, &Comms::baseStationDisconnected, this, &BaseStationsWidget::baseStationDisconnected);
}

//////////////////////////////////////////////////////////////////////////

void BaseStationsWidget::activateBaseStation(QModelIndex const& index)
{
    uint32_t bsIndex = index.row();
    if (bsIndex >= m_baseStations.size())
    {
        assert(false);
        return;
    }

    BaseStationData& bsd = m_baseStations[bsIndex];
    m_activatedBaseStation = bsd.descriptor;

    emit baseStationActivated(bsd.descriptor, *bsd.db);
}

//////////////////////////////////////////////////////////////////////////

void BaseStationsWidget::baseStationDisconnected(Comms::BaseStation const& bs)
{
    auto it = std::find_if(m_baseStations.begin(), m_baseStations.end(), [&bs](BaseStationData const& bsd) { return bsd.descriptor == bs.descriptor; });
    if (it == m_baseStations.end())
    {
        assert(false);
        return;
    }

    if (bs.descriptor == m_activatedBaseStation)
    {
        emit baseStationDeactivated(bs.descriptor);
        m_activatedBaseStation = Comms::BaseStationDescriptor();
    }

    size_t bsIndex = std::distance(m_baseStations.begin(), it);
    m_model.removeRow(static_cast<int>(bsIndex));

    m_baseStations.erase(it);
}

//////////////////////////////////////////////////////////////////////////

void BaseStationsWidget::baseStationDiscovered(Comms::BaseStationDescriptor const& bs)
{
//    QList<QListWidgetItem*> items = m_ui.list->findItems(QString(buf), Qt::MatchExactly);
//    if (!items.empty())
//    {
//        return;
//    }

    auto it = std::find_if(m_baseStations.begin(), m_baseStations.end(), [&bs](BaseStationData const& bsd) { return bsd.descriptor == bs; });
    if (it != m_baseStations.end())
    {
        return;
    }

    char macStr[128];
    sprintf(macStr, "%X_%X_%X_%X_%X_%X", bs.mac[0]&0xFF, bs.mac[1]&0xFF, bs.mac[2]&0xFF, bs.mac[3]&0xFF, bs.mac[4]&0xFF, bs.mac[5]&0xFF);
    std::string dbName = macStr;

    sprintf(macStr, "%X:%X:%X:%X:%X:%X", bs.mac[0]&0xFF, bs.mac[1]&0xFF, bs.mac[2]&0xFF, bs.mac[3]&0xFF, bs.mac[4]&0xFF, bs.mac[5]&0xFF);

    std::unique_ptr<DB> db(new DB);
    if (!db->load(dbName))
    {
        if (!db->create(dbName))
        {
            QMessageBox::critical(this, "Error", QString("Cannot open nor create a DB for Base Station '%1' (%2)").arg(bs.name.c_str()).arg(macStr));
            return;
        }
    }

    if (!m_comms->connectToBaseStation(*db, bs.address))
    {
        QMessageBox::critical(this, "Error", QString("Cannot connect to Base Station '%1' (%2)").arg(bs.name.c_str()).arg(macStr));
        return;
    }

    BaseStationData bsd;
    bsd.descriptor = bs;
    bsd.db = std::move(db);
    bsd.emailer.reset(new Emailer());
    bsd.emailer->init(*bsd.db);
    m_baseStations.push_back(std::move(bsd));

    QStandardItem* nameItem = new QStandardItem();
    QStandardItem* macItem = new QStandardItem();
    QStandardItem* ipItem = new QStandardItem();

    {
        nameItem->setText(bs.name.c_str());
        nameItem->setIcon(QIcon(":/icons/ui/station.png"));
        nameItem->setEditable(false);
    }

    {
        macItem->setText(QString("   %1   ").arg(macStr));
        macItem->setEditable(false);
    }

    {
        char buf[128];
        sprintf(buf, "  %s  ", bs.address.toString().toLatin1().data());
        ipItem->setText(buf);
        ipItem->setEditable(false);
    }

    m_model.appendRow({ nameItem, macItem, ipItem });
}

//////////////////////////////////////////////////////////////////////////

