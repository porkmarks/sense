#pragma once

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

    bool connectToBaseStation(DB& db, QHostAddress const& address);

    struct BaseStationDescriptor
    {
        std::string name;
        std::array<uint8_t, 6> mac;
        QHostAddress address;
        bool operator==(BaseStationDescriptor const& other) const { return name == other.name && mac == other.mac && address == other.address; }
    };

    struct BaseStation
    {
        BaseStation(DB& db)
            : db(db)
        {}

        BaseStationDescriptor descriptor;
        DB& db;
    };

    std::vector<BaseStationDescriptor> const& getDiscoveredBaseStations() const;
    std::vector<BaseStation> const& getConnectedBasestations() const;

    void process();

public slots:
    void requestConfig();
    void requestSensors();
    void requestBindSensor(std::string const& name);

signals:
    void baseStationDiscovered(BaseStationDescriptor const& bs);
    void baseStationConnected(BaseStation const& bs);
    void baseStationDisconnected(BaseStation const& bs);

private:
    struct ConnectedBaseStation
    {
        ConnectedBaseStation(DB& db)
            : baseStation(db)
            , channel(socketAdapter)
        {
            socketAdapter.start();
        }

        BaseStation baseStation;
        QTcpSocketAdapter socketAdapter;
        util::comms::Channel<data::Server_Message, QTcpSocketAdapter> channel;
    };

private slots:
    void broadcastReceived();
    void connectedToBaseStation(ConnectedBaseStation* cbs);
    void disconnectedFromBaseStation(ConnectedBaseStation* cbs);

private:

    void processGetConfigRes();
    void processSetConfigRes();
    void processGetSensorsRes();
    void processAddSensorRes();
    void processRemoveSensorRes();
    void processReportMeasurementReq();
    void processSensorBoundReq();


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
