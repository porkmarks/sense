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

    typedef std::chrono::system_clock Clock;

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
    using Channel = util::comms::Channel<data::Server_Message, QTcpSocketAdapter>;

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

        DB& db;
        BaseStationDescriptor descriptor;
        QTcpSocketAdapter socketAdapter;
        Channel channel;
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

    struct SensorRequest
    {
        uint32_t reqId = 0;
        int8_t signalS2B = 0;
        uint8_t type = 0;
        uint32_t address = 0;
        std::vector<uint8_t> payload;
    };

    void sendEmptySensorResponse(InitializedBaseStation& cbs, SensorRequest const& request);
    template <typename T>
    void sendSensorResponse(InitializedBaseStation& cbs, SensorRequest const& request, data::sensor::Type type, uint32_t address, uint8_t retries, T const& payload);

    void processPong(InitializedBaseStation& cbs);
    void processSensorReq(InitializedBaseStation& cbs);
    void processSensorReq(InitializedBaseStation& cbs, SensorRequest const& request);
    void processSensorReq_MeasurementBatch(InitializedBaseStation& cbs, SensorRequest const& request, data::sensor::Measurement_Batch const& payload);
    void processSensorReq_ConfigRequest(InitializedBaseStation& cbs, SensorRequest const& request, data::sensor::Config_Request const& payload);
    void processSensorReq_FirstConfigRequest(InitializedBaseStation& cbs, SensorRequest const& request, data::sensor::First_Config_Request const& payload);
    void processSensorReq_PairRequest(InitializedBaseStation& cbs, SensorRequest const& request, data::sensor::Pair_Request const& payload);

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

