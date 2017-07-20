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
    void baseStationDiscovered(Comms::BaseStationDescriptor const& bs);
    void baseStationDisconnected(Comms::BaseStation const& bs);
    void activateBaseStation(QModelIndex const& index);

private:
    Ui::BaseStationsWidget m_ui;
    Comms* m_comms = nullptr;
    Settings* m_settings = nullptr;
    QStandardItemModel m_model;

    struct BaseStationData
    {
        Comms::BaseStationDescriptor descriptor;
        std::unique_ptr<DB> db;
        std::unique_ptr<Emailer> emailer;
    };

    std::vector<BaseStationData> m_baseStations;
    Comms::BaseStationDescriptor m_activatedBaseStation;
};


