#pragma once

#include <QWidget>
#include <unordered_map>
#include <QStandardItemModel>
#include "ui_BaseStationsWidget.h"
#include "Comms.h"
#include "Settings.h"
#include "Emailer.h"

class BaseStationsWidget : public QWidget
{
    Q_OBJECT
public:
    explicit BaseStationsWidget(QWidget *parent = 0);
    ~BaseStationsWidget();

    void init(Comms& comms, Settings& settings);

signals:
    void baseStationActivated(Comms::BaseStationDescriptor const& bs, DB& db);
    void baseStationDeactivated(Comms::BaseStationDescriptor const& bs);


private slots:
    void setRW();
    void baseStationDiscovered(Comms::BaseStationDescriptor const& bs);
    void baseStationDisconnected(Comms::BaseStationDescriptor const& bs);
    void activateBaseStation(QModelIndex const& index);

private:
    void setStatus(int row, std::string const& status);
    void setAddress(int row, QHostAddress const& address);


    Ui::BaseStationsWidget m_ui;
    Comms* m_comms = nullptr;
    Settings* m_settings = nullptr;
    QStandardItemModel m_model;

    std::vector<Comms::BaseStationDescriptor> m_unregisteredBaseStations;
    std::vector<QMetaObject::Connection> m_uiConnections;
};


