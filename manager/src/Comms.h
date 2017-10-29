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
        std::string name;
        Mac mac;
        QHostAddress address;
        bool operator==(BaseStationDescriptor const& other) const { return name == other.name && mac == other.mac && address == other.address; }
    };

    void process();

signals:
    void baseStationDiscovered(BaseStationDescriptor const& bs);
    void baseStationConnected(BaseStationDescriptor const& bs);
    void baseStationDisconnected(BaseStationDescriptor const& bs);

private:
    struct ConnectedBaseStation
    {
        ConnectedBaseStation(DB& db)
            : db(db)
            , channel(socketAdapter)
        {
        }

        BaseStationDescriptor descriptor;
        DB& db;
        QTcpSocketAdapter socketAdapter;
        util::comms::Channel<data::Server_Message, QTcpSocketAdapter> channel;
    };

private slots:
    void broadcastReceived();
    void connectedToBaseStation(ConnectedBaseStation* cbs);
    void disconnectedFromBaseStation(ConnectedBaseStation* cbs);

private:
    void processSetConfigRes(ConnectedBaseStation& cbs);
    void processSetSensorsRes(ConnectedBaseStation& cbs);
    void processAddSensorRes(ConnectedBaseStation& cbs);
    void processReportMeasurementReq(ConnectedBaseStation& cbs);
    void processSensorBoundReq(ConnectedBaseStation& cbs);

    void sendSensorSettings(ConnectedBaseStation& cbs);
    void sendSensors(ConnectedBaseStation& cbs);
    void requestBindSensor(ConnectedBaseStation& cbs, DB::SensorId sensorId);

    QUdpSocket m_broadcastSocket;

    std::vector<BaseStationDescriptor> m_discoveredBaseStations;
    std::vector<std::unique_ptr<ConnectedBaseStation>> m_connectedBaseStations;
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
                hash<std::string>()(bs.name) ^
                hash<std::string>()(std::string(bs.address.toString().toUtf8().data()));
    }
};

}

