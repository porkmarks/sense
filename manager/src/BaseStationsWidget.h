#pragma once

#include <QWidget>
#include <unordered_map>
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
    void baseStationActivated(Comms::BaseStationDescriptor const& bs, DB& db);
    void baseStationDeactivated(Comms::BaseStationDescriptor const& bs);


private slots:
    void baseStationDiscovered(Comms::BaseStationDescriptor const& bs);
    void baseStationDisconnected(Comms::BaseStation const& bs);
    void activateBaseStation(QModelIndex const& index);

private:
    Ui::BaseStationsWidget m_ui;
    Comms* m_comms = nullptr;
    QStandardItemModel m_model;

    struct BaseStationData
    {
        Comms::BaseStationDescriptor descriptor;
        std::unique_ptr<DB> db;
    };

    std::vector<BaseStationData> m_baseStations;
    Comms::BaseStationDescriptor m_activatedBaseStation;
};


