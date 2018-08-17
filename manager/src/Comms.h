#pragma once

#include <array>
#include <memory>
#include <unordered_map>
#include <QUdpSocket>
#include <QTcpSocket>

#include "DB.h"
#include "Data_Defs.h"
#include "Channel.h"
#include "QTcpSocketAdapter.h"

class Comms : public QObject
{
    Q_OBJECT
public:

    typedef std::chrono::high_resolution_clock Clock;

    Comms();
    ~Comms();

    typedef std::array<uint8_t, 6> Mac;

    QHostAddress getBaseStationAddress(Mac const& mac) const;
    bool isBaseStationConnected(Mac const& mac) const;
    bool connectToBaseStation(DB& db, Mac const& mac);

    struct BaseStationDescriptor
    {
        Mac mac;
        QHostAddress address;
        bool operator==(BaseStationDescriptor const& other) const { return mac == other.mac && address == other.address; }
    };

    void process();

signals:
    void baseStationDiscovered(BaseStationDescriptor const& bs);
    void baseStationConnected(BaseStationDescriptor const& bs);
    void baseStationDisconnected(BaseStationDescriptor const& bs);

private:
    struct InitializedBaseStation
    {
        InitializedBaseStation(DB& db, const BaseStationDescriptor& descriptor)
            : db(db)
            , descriptor(descriptor)
            , channel(socketAdapter)
        {
        }
        ~InitializedBaseStation()
        {
            for (QMetaObject::Connection& c : connections)
            {
                QObject::disconnect(c);
            }
        }

        BaseStationDescriptor descriptor;
        DB& db;
        QTcpSocketAdapter socketAdapter;
        util::comms::Channel<data::Server_Message, QTcpSocketAdapter> channel;
        bool isConnecting = false;
        bool isConnected = false;

        std::vector<QMetaObject::Connection> connections;
    };

private slots:
    void broadcastReceived();
    void connectedToBaseStation(InitializedBaseStation* cbs);
    void disconnectedFromBaseStation(InitializedBaseStation* cbs);

private:
    void reconnectToBaseStation(InitializedBaseStation* cbs);

    void processSetConfigRes(InitializedBaseStation& cbs);
    void processSetSensorsRes(InitializedBaseStation& cbs);
    void processAddSensorRes(InitializedBaseStation& cbs);
    void processReportMeasurementReq(InitializedBaseStation& cbs);
    void processSensorBoundReq(InitializedBaseStation& cbs);
    void processReportSensorDetails(InitializedBaseStation& cbs);

    void processSensorDetails(InitializedBaseStation& cbs, std::vector<uint8_t> const& data);

    void sendSensorSettings(InitializedBaseStation& cbs);
    void sendSensors(InitializedBaseStation& cbs);
    void requestBindSensor(InitializedBaseStation& cbs, DB::SensorId sensorId);

    QUdpSocket m_broadcastSocket;

    std::vector<BaseStationDescriptor> m_discoveredBaseStations;
    std::vector<std::unique_ptr<InitializedBaseStation>> m_initializedBaseStations;
};

namespace std
{

template <>
struct hash<Comms::BaseStationDescriptor>
{
    size_t operator()(Comms::BaseStationDescriptor const& bs) const
    {
        return hash<uint8_t>()(bs.mac[0]) ^
                hash<uint8_t>()(bs.mac[1]) ^
                hash<uint8_t>()(bs.mac[2]) ^
                hash<uint8_t>()(bs.mac[3]) ^
                hash<uint8_t>()(bs.mac[4]) ^
                hash<uint8_t>()(bs.mac[5]) ^
                hash<std::string>()(std::string(bs.address.toString().toUtf8().data()));
    }
};

}

