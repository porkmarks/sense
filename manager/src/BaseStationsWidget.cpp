#include "BaseStationsWidget.h"
#include "Settings.h"
#include <cassert>
#include <QMessageBox>
#include <QSettings>
#include <QInputDialog>

extern std::string getMacStr(Settings::BaseStationDescriptor::Mac const& mac);

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
        QSettings settings;
        settings.setValue("baseStations/list/state", m_ui.list->header()->saveState());
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

void BaseStationsWidget::init(Comms& comms, Settings& settings)
{
    for (const QMetaObject::Connection& connection: m_uiConnections)
    {
        QObject::disconnect(connection);
    }
    m_uiConnections.clear();

    setEnabled(true);

    m_comms = &comms;
    m_settings = &settings;

    m_uiConnections.push_back(connect(m_comms, &Comms::baseStationDiscovered, this, &BaseStationsWidget::baseStationDiscovered));
    m_uiConnections.push_back(connect(m_comms, &Comms::baseStationConnected, this, &BaseStationsWidget::baseStationConnected));
    m_uiConnections.push_back(connect(m_comms, &Comms::baseStationDisconnected, this, &BaseStationsWidget::baseStationDisconnected));

    for (size_t i = 0; i < settings.getBaseStationCount(); i++)
    {
        Settings::BaseStation const& bs = settings.getBaseStation(i);

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
            Settings::BaseStationDescriptor::Mac mac = bs.descriptor.mac;
            macItem->setText(QString("   %1   ").arg(getMacStr(mac).c_str()));
            macItem->setEditable(false);
        }

        m_model.appendRow({ nameItem, macItem, ipItem, statusItem });

        QHostAddress address = m_comms->getBaseStationAddress(bs.descriptor.mac);
        setAddress(i, address);

        bool connected = m_comms->connectToBaseStation(m_settings->getBaseStationDB(i), bs.descriptor.mac);
        if (settings.getActiveBaseStationId() == bs.id)
        {
            setStatus(i, connected ? "Active / Connected" : "Active / Disconnected");
        }
        else
        {
            setStatus(i, connected ? "Added / Connected" : "Added / Disconnected");
        }

        m_baseStationDescriptors.push_back(bs.descriptor);
    }

    setRW();
    m_uiConnections.push_back(connect(&settings, &Settings::userLoggedIn, this, &BaseStationsWidget::setRW));
}


//////////////////////////////////////////////////////////////////////////

void BaseStationsWidget::setRW()
{
//    m_ui.add->setEnabled(m_settings->isLoggedInAsAdmin());
//    m_ui.remove->setEnabled(m_settings->isLoggedInAsAdmin());
}

//////////////////////////////////////////////////////////////////////////

void BaseStationsWidget::setStatus(int row, std::string const& status)
{
    QStandardItem* item = m_model.item(row, 3);
    item->setText(status.c_str());
}

//////////////////////////////////////////////////////////////////////////

void BaseStationsWidget::setName(int row, std::string const& name)
{
    QStandardItem* item = m_model.item(row, 0);
    item->setText(name.c_str());
}

//////////////////////////////////////////////////////////////////////////

void BaseStationsWidget::setAddress(int row, QHostAddress const& address)
{
    QStandardItem* item = m_model.item(row, 2);
    char buf[128];
    sprintf(buf, "  %s  ", address.isNull() ? "N/A" : address.toString().toLatin1().data());
    item->setText(buf);
    item->setEditable(false);
}

//////////////////////////////////////////////////////////////////////////

void BaseStationsWidget::activateBaseStation(QModelIndex const& index)
{
    if (!m_settings->isLoggedInAsAdmin())
    {
        QMessageBox::critical(this, "Error", "You need to be logged in as admin to activate/add base stations.");
        return;
    }

    if (index.row() >= m_baseStationDescriptors.size())
    {
        assert(false);
        return;
    }

    int32_t bsIndex = m_settings->findBaseStationIndexByMac(m_baseStationDescriptors[index.row()].mac);
    if (bsIndex >= 0)
    {
        Settings::BaseStation const& bs = m_settings->getBaseStation(bsIndex);
        m_settings->setActiveBaseStationId(bs.id);
        bool connected = m_comms->isBaseStationConnected(bs.descriptor.mac);
        setStatus(bsIndex, connected ? "Active / Connected" : "Active / Disconnected");
        return;
    }


    QString qname = QInputDialog::getText(this, "Base Station Name", "Name");
    if (qname.isEmpty())
    {
        return;
    }

    Settings::BaseStationDescriptor descriptor = m_baseStationDescriptors[bsIndex];
    descriptor.name = qname.toUtf8().data();
    if (m_settings->addBaseStation(descriptor))
    {
        int32_t index = m_settings->findBaseStationIndexByMac(descriptor.mac);
        if (index >= 0)
        {
            bool connected = m_comms->connectToBaseStation(m_settings->getBaseStationDB(index), descriptor.mac);
            setStatus(bsIndex, connected ? "Added / Connected" : "Added / Disconnected");
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
    int32_t bsIndex = m_settings->findBaseStationIndexByMac(commsBS.mac);
    if (bsIndex >= 0)
    {
        Settings::BaseStation const& bs = m_settings->getBaseStation(bsIndex);

        if (m_settings->getActiveBaseStationId() == bs.id)
        {
            setStatus(bsIndex, "Active / Connected");
        }
        else
        {
            setStatus(bsIndex, "Added / Connected");
        }

        setAddress(bsIndex, commsBS.address);
        return;
    }
}

//////////////////////////////////////////////////////////////////////////

void BaseStationsWidget::baseStationDisconnected(Comms::BaseStationDescriptor const& commsBS)
{
    int32_t bsIndex = m_settings->findBaseStationIndexByMac(commsBS.mac);
    if (bsIndex >= 0)
    {
        Settings::BaseStation const& bs = m_settings->getBaseStation(bsIndex);

        if (m_settings->getActiveBaseStationId() == bs.id)
        {
            setStatus(bsIndex, "Active / Disconnected");
        }
        else
        {
            setStatus(bsIndex, "Added / Disconnected");
        }

        setAddress(bsIndex, commsBS.address);

        return;
    }
}

//////////////////////////////////////////////////////////////////////////

void BaseStationsWidget::baseStationDiscovered(Comms::BaseStationDescriptor const& commsBS)
{
    int32_t bsIndex = m_settings->findBaseStationIndexByMac(commsBS.mac);
    if (bsIndex >= 0)
    {
        Settings::BaseStation const& bs = m_settings->getBaseStation(bsIndex);

        bool connected = m_comms->connectToBaseStation(m_settings->getBaseStationDB(bsIndex), bs.descriptor.mac);
        if (m_settings->getActiveBaseStationId() == bs.id)
        {
            setStatus(bsIndex, connected ? "Active / Connected" : "Active / Disconnected");
        }
        else
        {
            setStatus(bsIndex, connected ? "Added / Connected" : "Added / Disconnected");
        }

        setAddress(bsIndex, commsBS.address);

        return;
    }

    auto it = std::find_if(m_baseStationDescriptors.begin(), m_baseStationDescriptors.end(), [&commsBS](Settings::BaseStationDescriptor const& descriptor) { return descriptor.mac == commsBS.mac; });
    if (it != m_baseStationDescriptors.end())
    {
        return;
    }
    Settings::BaseStationDescriptor descriptor;
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
        macItem->setText(QString("   %1   ").arg(getMacStr(commsBS.mac).c_str()));
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

