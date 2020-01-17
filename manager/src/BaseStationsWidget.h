#pragma once

#include <QWidget>
#include <unordered_map>
#include <QStandardItemModel>
#include "ui_BaseStationsWidget.h"
#include "Comms.h"
#include "DB.h"
#include "Emailer.h"

class BaseStationsWidget : public QWidget
{
    Q_OBJECT
public:
    explicit BaseStationsWidget(QWidget *parent = 0);
    ~BaseStationsWidget();

	void init(Comms& comms, DB& db);

signals:
    void baseStationActivated(Comms::BaseStationDescriptor const& bs, DB& db);
    void baseStationDeactivated(Comms::BaseStationDescriptor const& bs);


private slots:
	void setPermissions();
    void baseStationDiscovered(Comms::BaseStationDescriptor const& bs);
    void baseStationConnected(Comms::BaseStationDescriptor const& bs);
    void baseStationDisconnected(Comms::BaseStationDescriptor const& bs);
    void activateBaseStation(QModelIndex const& index);
    void showDisconnectionMessageBox(DB::BaseStation const& bs, const QHostAddress& address);

private:
    void setName(size_t row, std::string const& name);
    void setStatus(size_t row, std::string const& status);
    void setAddress(size_t row, QHostAddress const& address);

    Ui::BaseStationsWidget m_ui;
	mutable std::recursive_mutex m_mutex;
    Comms* m_comms = nullptr;
	DB* m_db = nullptr;
    QStandardItemModel m_model;

    std::vector<DB::BaseStationDescriptor> m_baseStationDescriptors;
    std::vector<QMetaObject::Connection> m_uiConnections;
    bool m_sectionSaveScheduled = false;
};


