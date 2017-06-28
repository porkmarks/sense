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
    void baseStationSelected(Comms::BaseStation const& bs);


private slots:
    void baseStationDiscovered(Comms::BaseStation const& bs);
    void baseStationConnected(Comms::BaseStation const& bs);
    void baseStationDisconnected(Comms::BaseStation const& bs);
    void activateBaseStation(QModelIndex const& index);

private:
    Ui::BaseStationsWidget m_ui;
    Comms* m_comms = nullptr;
    QStandardItemModel m_model;

    std::vector<Comms::BaseStation> m_baseStations;
};


