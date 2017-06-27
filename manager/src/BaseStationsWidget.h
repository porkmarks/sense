#pragma once

#include <QWidget>
#include <QStandardItemModel>
#include "ui_BaseStationsWidget.h"
#include "Comms.h"

class BaseStationsWidget : public QWidget
{
    Q_OBJECT
public:
    explicit BaseStationsWidget(QWidget *parent = 0);

    void init(Comms& comms);

signals:
    void baseStationSelected(Comms::BaseStationDescriptor const& bs);


private slots:
    void baseStationDiscovered(Comms::BaseStationDescriptor const& bs);
    void baseStationConnected(Comms::BaseStationDescriptor const& bs);
    void baseStationDisconnected(Comms::BaseStationDescriptor const& bs);
    void activateBaseStation(QModelIndex const& index);

private:
    Ui::BaseStationsWidget m_ui;
    Comms* m_comms = nullptr;
    QStandardItemModel m_model;

    std::vector<Comms::BaseStationDescriptor> m_baseStations;
};


