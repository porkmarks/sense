#include "BaseStationsWidget.h"
#include "DB.h"
#include <cassert>
#include <QMessageBox>
#include <QSettings>
#include <QInputDialog>
#include <QDateTime>
#include "PermissionsCheck.h"
#include "Utils.h"

//////////////////////////////////////////////////////////////////////////

BaseStationsWidget::BaseStationsWidget(QWidget *parent)
    : QWidget(parent)
{
    m_ui.setupUi(this);
    m_ui.list->setModel(&m_model);
    m_ui.list->setUniformRowHeights(true);

    m_model.setColumnCount(4);
    m_model.setHorizontalHeaderLabels({"Name", "MAC", "IP", "Status"});

//    m_ui.list->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
//    m_ui.list->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
//    m_ui.list->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
//    m_ui.list->header()->setSectionResizeMode(3, QHeaderView::Stretch);

    connect(m_ui.list->header(), &QHeaderView::sectionResized, [this]()
    {
        if (!m_sectionSaveScheduled)
        {
            m_sectionSaveScheduled = true;
            QTimer::singleShot(500, [this]()
            {
                m_sectionSaveScheduled = false;
                QSettings settings;
                settings.setValue("baseStations/list/state", m_ui.list->header()->saveState());
            });
        }
    });

    {
        QSettings settings;
        m_ui.list->header()->restoreState(settings.value("baseStations/list/state").toByteArray());
    }

    connect(m_ui.list, &QTreeView::doubleClicked, this, &BaseStationsWidget::activateBaseStation);
}

//////////////////////////////////////////////////////////////////////////

BaseStationsWidget::~BaseStationsWidget()
{
    m_ui.list->setModel(nullptr);
    m_comms = nullptr;
    m_baseStationDescriptors.clear();
}

//////////////////////////////////////////////////////////////////////////

void BaseStationsWidget::init(Comms& comms, DB& db)
{
    for (const QMetaObject::Connection& connection: m_uiConnections)
    {
        QObject::disconnect(connection);
    }
    m_uiConnections.clear();

    setEnabled(true);

    m_comms = &comms;
    m_db = &db;

    for (size_t i = 0; i < db.getBaseStationCount(); i++)
    {
        DB::BaseStation bs = db.getBaseStation(i);

        QStandardItem* nameItem = new QStandardItem();
        QStandardItem* macItem = new QStandardItem();
        QStandardItem* ipItem = new QStandardItem();
        QStandardItem* statusItem = new QStandardItem();

        {
            nameItem->setText(bs.descriptor.name.c_str());
            nameItem->setIcon(QIcon(":/icons/ui/station.png"));
            nameItem->setEditable(false);
        }

        {
            DB::BaseStationDescriptor::Mac mac = bs.descriptor.mac;
            macItem->setText(QString("   %1   ").arg(utils::getMacStr(mac).c_str()));
            macItem->setEditable(false);
        }
        ipItem->setEditable(false);
        statusItem->setEditable(false);

        m_model.appendRow({ nameItem, macItem, ipItem, statusItem });

        QHostAddress address = m_comms->getBaseStationAddress(bs.descriptor.mac);
        setAddress(i, address);

        bool connected = m_comms->isBaseStationConnected(bs.descriptor.mac);
        setStatus(i, connected ? "Connected" : "Disconnected");

        m_baseStationDescriptors.push_back(bs.descriptor);
    }

	for (Comms::BaseStationDescriptor const& bsd: m_comms->getDiscoveredBaseStations())
	{
        baseStationDiscovered(bsd);
	}

    setPermissions();
	m_uiConnections.push_back(connect(&db, &DB::userLoggedIn, this, &BaseStationsWidget::setPermissions));

	m_uiConnections.push_back(connect(m_comms, &Comms::baseStationDiscovered, this, &BaseStationsWidget::baseStationDiscovered));
	m_uiConnections.push_back(connect(m_comms, &Comms::baseStationConnected, this, &BaseStationsWidget::baseStationConnected));
	m_uiConnections.push_back(connect(m_comms, &Comms::baseStationDisconnected, this, &BaseStationsWidget::baseStationDisconnected));
}

//////////////////////////////////////////////////////////////////////////

void BaseStationsWidget::setPermissions()
{
    std::lock_guard<std::recursive_mutex> lg(m_mutex);
}

//////////////////////////////////////////////////////////////////////////

void BaseStationsWidget::setStatus(size_t row, std::string const& status)
{
    QStandardItem* item = m_model.item(static_cast<int>(row), 3);
    item->setText(status.c_str());
}

//////////////////////////////////////////////////////////////////////////

void BaseStationsWidget::setName(size_t row, std::string const& name)
{
    QStandardItem* item = m_model.item(static_cast<int>(row), 0);
    item->setText(name.c_str());
}

//////////////////////////////////////////////////////////////////////////

void BaseStationsWidget::setAddress(size_t row, QHostAddress const& address)
{
    QStandardItem* item = m_model.item(static_cast<int>(row), 2);
    char buf[128];
    sprintf(buf, "  %s  ", address.isNull() ? "N/A" : address.toString().toLatin1().data());
    item->setText(buf);
}

//////////////////////////////////////////////////////////////////////////

void BaseStationsWidget::activateBaseStation(QModelIndex const& index)
{
    std::lock_guard<std::recursive_mutex> lg(m_mutex);

	if (!hasPermissionOrCanLoginAsAdmin(*m_db, DB::UserDescriptor::PermissionAddRemoveBaseStations, this))
    {
        QMessageBox::critical(this, "Error", "You don't have permission to add base stations.");
        return;
    }

    size_t indexRow = static_cast<size_t>(index.row());
    if (indexRow >= m_baseStationDescriptors.size())
    {
        assert(false);
        return;
    }

    DB::BaseStationDescriptor descriptor = m_baseStationDescriptors[indexRow];

    if (m_db->findBaseStationIndexByMac(descriptor.mac) >= 0)
    {
        return;
    }

    QString qname = QInputDialog::getText(this, "Base Station Name", "Name");
    if (qname.isEmpty())
    {
        return;
    }

    descriptor.name = qname.toUtf8().data();
    if (m_db->addBaseStation(descriptor))
    {
        int32_t _bsIndex = m_db->findBaseStationIndexByMac(descriptor.mac);
        if (_bsIndex >= 0)
        {
            size_t bsIndex = static_cast<size_t>(_bsIndex);
            setStatus(bsIndex, "Disconnected");
            m_comms->connectToBaseStation(*m_db, descriptor.mac);
            setName(bsIndex, descriptor.name);
        }
        else
        {
            QMessageBox::critical(this, "Error", "Failed to add base station: internal consistency error.");
        }
    }
    else
    {
        QMessageBox::critical(this, "Error", "Failed to add base station.");
    }
}

//////////////////////////////////////////////////////////////////////////

void BaseStationsWidget::baseStationConnected(Comms::BaseStationDescriptor const& commsBS)
{
    std::lock_guard<std::recursive_mutex> lg(m_mutex);

    int32_t _bsIndex = m_db->findBaseStationIndexByMac(commsBS.mac);
    if (_bsIndex >= 0)
    {
        size_t bsIndex = static_cast<size_t>(_bsIndex);
        //DB::BaseStation bs = db.getBaseStation(bsIndex);
        setStatus(bsIndex, "Connected");
        setAddress(bsIndex, commsBS.address);
        return;
    }
}

//////////////////////////////////////////////////////////////////////////

void BaseStationsWidget::baseStationDisconnected(Comms::BaseStationDescriptor const& commsBS)
{
    std::lock_guard<std::recursive_mutex> lg(m_mutex);

    int32_t _bsIndex = m_db->findBaseStationIndexByMac(commsBS.mac);
    if (_bsIndex >= 0)
    {
        size_t bsIndex = static_cast<size_t>(_bsIndex);
        DB::BaseStation bs = m_db->getBaseStation(bsIndex);
        setStatus(bsIndex, "Disconnected");
        setAddress(bsIndex, commsBS.address);
        showDisconnectionMessageBox(bs, commsBS.address);
        return;
    }
}

//////////////////////////////////////////////////////////////////////////

void BaseStationsWidget::showDisconnectionMessageBox(DB::BaseStation const& bs, const QHostAddress& address)
{
    std::lock_guard<std::recursive_mutex> lg(m_mutex);

    //can't do this modal thing here!!
//    QMessageBox::critical(this, "Error", QString("Base Station %1, %2 has disconnected at %3")
//                    .arg(bs.descriptor.name.c_str())
//                    .arg(address.toString())
//                    .arg(QDateTime::currentDateTime().toString("dd-MM-yyyy HH:mm")));
}

//////////////////////////////////////////////////////////////////////////

void BaseStationsWidget::baseStationDiscovered(Comms::BaseStationDescriptor const& commsBS)
{
	auto id = std::this_thread::get_id();

    std::lock_guard<std::recursive_mutex> lg(m_mutex);

    int32_t _bsIndex = m_db->findBaseStationIndexByMac(commsBS.mac);
    if (_bsIndex >= 0)
    {
        size_t bsIndex = static_cast<size_t>(_bsIndex);
        DB::BaseStation bs = m_db->getBaseStation(static_cast<size_t>(bsIndex));
        bool connected = m_comms->isBaseStationConnected(bs.descriptor.mac);
        setStatus(bsIndex, connected ? "Connected" : "Disconnected");
        setAddress(bsIndex, commsBS.address);
        return;
    }

    auto it = std::find_if(m_baseStationDescriptors.begin(), m_baseStationDescriptors.end(), [&commsBS](DB::BaseStationDescriptor const& descriptor) { return descriptor.mac == commsBS.mac; });
    if (it != m_baseStationDescriptors.end())
    {
        return;
    }
    DB::BaseStationDescriptor descriptor;
    descriptor.mac = commsBS.mac;
    m_baseStationDescriptors.push_back(descriptor);

    QStandardItem* nameItem = new QStandardItem();
    QStandardItem* macItem = new QStandardItem();
    QStandardItem* ipItem = new QStandardItem();
    QStandardItem* statusItem = new QStandardItem();

    {
        nameItem->setText("");
        nameItem->setIcon(QIcon(":/icons/ui/station.png"));
        nameItem->setEditable(false);
    }

    {
        macItem->setText(QString("   %1   ").arg(utils::getMacStr(commsBS.mac).c_str()));
        macItem->setEditable(false);
    }

    {
        char buf[128];
        sprintf(buf, "  %s  ", commsBS.address.toString().toLatin1().data());
        ipItem->setText(buf);
        ipItem->setEditable(false);
    }
    {
        statusItem->setText("Not Added");
    }

    m_model.appendRow({ nameItem, macItem, ipItem, statusItem });
}

//////////////////////////////////////////////////////////////////////////

