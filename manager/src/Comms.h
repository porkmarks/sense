#pragma once

#include <memory>
#include <QUdpSocket>
#include <QTcpSocket>

#include "Data_Defs.h"
#include "Channel.h"
#include "QTcpSocketAdapter.h"

class Comms : public QObject
{
    Q_OBJECT
public:

    typedef std::chrono::high_resolution_clock Clock;
    typedef uint32_t Sensor_Id;
    typedef uint32_t Sensor_Address;

    Comms();

    void connectToBaseStation(QHostAddress const& address);

    struct BaseStationDescriptor
    {
        std::array<uint8_t, 6> mac;
        QHostAddress address;
    };

    struct Sensor
    {
        Sensor_Id id = 0;
        Sensor_Address address = 0;
        std::string name;
    };

    struct Config
    {
        bool sensors_sleeping = false;
        Clock::duration measurement_period;
        Clock::duration comms_period;
        Clock::duration computed_comms_period;

        //This is computed when creating the config so that this equation holds for any config:
        // measurement_time_point = config.baseline_time_point + measurement_index * config.measurement_period
        //
        //So when creating a new config, this is how to calculate the baseline:
        // m = some measurement (any)
        // config.baseline_time_point = m.time_point - m.index * config.measurement_period
        //
        //The reason for this is to keep the indices valid in all configs
        Clock::time_point baseline_time_point;
    };


    void process();

public slots:
    void requestConfig();
    void requestSensors();

signals:
    void baseStationDiscovered(BaseStationDescriptor const& bs);
    void baseStationConnected(BaseStationDescriptor const& bs);
    void baseStationDisconnected(BaseStationDescriptor const& bs);
    void configReceived(Config const& config);
    void sensorsReceived(std::vector<Sensor> const& sensors);

private slots:
    void broadcastReceived();
    void connectedToBaseStation();
    void disconnectedFromBaseStation();

private:

    void processGetConfigRes();
    void processSetConfigRes();
    void processGetSensorsRes();
    void processAddSensorRes();
    void processRemoveSensorRes();
    void processReportMeasurementReq();
    void processSensorBoundReq();


    QUdpSocket m_broadcastSocket;
    std::vector<BaseStationDescriptor> m_baseStations;

    size_t m_connectedBSIndex = size_t(-1);
    QTcpSocketAdapter m_bsSocketAdapter;
    util::comms::Channel<data::Server_Message, QTcpSocketAdapter> m_bsChannel;
};


