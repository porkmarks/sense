#include "Comms.h"
#include <cassert>
#include <iostream>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

Comms::Comms()
    : m_bsChannel(m_bsSocketAdapter)
{
    m_bsSocketAdapter.start();

    m_broadcastSocket.bind(5555, QUdpSocket::ShareAddress);

    QObject::connect(&m_broadcastSocket, &QUdpSocket::readyRead, this, &Comms::broadcastReceived);

    QObject::connect(&m_bsSocketAdapter.getSocket(), &QTcpSocket::connected, this, &Comms::connectedToBaseStation);
    QObject::connect(&m_bsSocketAdapter.getSocket(), &QTcpSocket::disconnected, this, &Comms::disconnectedFromBaseStation);
}

void Comms::broadcastReceived()
{
    while (m_broadcastSocket.hasPendingDatagrams())
    {
        if (m_broadcastSocket.pendingDatagramSize() == 6)
        {
            BaseStationDescriptor bs;
            if (m_broadcastSocket.readDatagram(reinterpret_cast<char*>(bs.mac.data()), bs.mac.size(), &bs.address, nullptr) == 6)
            {
                auto it = std::find_if(m_baseStations.begin(), m_baseStations.end(), [&bs](Comms::BaseStationDescriptor const& _bs) { return _bs.mac == bs.mac; });
                if (it == m_baseStations.end())
                {
                    m_baseStations.push_back(bs);
                }

                emit baseStationDiscovered(bs);
            }
        }
        else
        {
            m_broadcastSocket.readDatagram(nullptr, 0);
        }
    }
}

void Comms::connectToBaseStation(QHostAddress const& address)
{
    m_bsSocketAdapter.getSocket().disconnectFromHost();

    auto it = std::find_if(m_baseStations.begin(), m_baseStations.end(), [&address](Comms::BaseStationDescriptor const& _bs) { return _bs.address == address; });
    if (it == m_baseStations.end())
    {
        assert(false);
        return;
    }

    m_connectedBSIndex = std::distance(m_baseStations.begin(), it);

    m_bsSocketAdapter.getSocket().connectToHost(address, 4444);
}

void Comms::connectedToBaseStation()
{
    assert(m_connectedBSIndex < m_baseStations.size());
    if (m_connectedBSIndex < m_baseStations.size())
    {
        emit baseStationConnected(m_baseStations[m_connectedBSIndex]);

        requestConfig();
        requestSensors();
    }
}

void Comms::disconnectedFromBaseStation()
{
    assert(m_connectedBSIndex < m_baseStations.size());
    if (m_connectedBSIndex < m_baseStations.size())
    {
        size_t index = m_connectedBSIndex;
        m_connectedBSIndex = size_t(-1);

        emit baseStationDisconnected(m_baseStations[index]);
    }
}

void Comms::requestConfig()
{
    m_bsChannel.send(data::Server_Message::GET_CONFIG_REQ, nullptr, 0);
}

void Comms::requestSensors()
{
    m_bsChannel.send(data::Server_Message::GET_SENSORS_REQ, nullptr, 0);
}

void Comms::processGetConfigRes()
{
    std::vector<uint8_t> buffer;
    m_bsChannel.unpack(buffer);

    std::string str_buffer(reinterpret_cast<char*>(buffer.data()), reinterpret_cast<char*>(buffer.data()) + buffer.size());
    std::stringstream ss(str_buffer);

    try
    {
        boost::property_tree::ptree pt;
        boost::property_tree::read_json(ss, pt);

        Config config;
        config.sensors_sleeping = pt.get<bool>("sensors_sleeping");
        config.measurement_period = std::chrono::seconds(pt.get<std::chrono::seconds::rep>("measurement_period"));
        config.comms_period = std::chrono::seconds(pt.get<std::chrono::seconds::rep>("comms_period"));
        config.computed_comms_period = std::chrono::seconds(pt.get<std::chrono::seconds::rep>("computed_comms_period"));
        config.baseline_time_point = Clock::time_point(std::chrono::seconds(pt.get<std::chrono::seconds::rep>("baseline_time_point")));

        emit configReceived(config);
    }
    catch (std::exception const& e)
    {
        std::cerr << "Cannot deserialize response: " << e.what() << "\n";
    }
}

void Comms::processSetConfigRes()
{

}

void Comms::processGetSensorsRes()
{
    std::vector<uint8_t> buffer;
    m_bsChannel.unpack(buffer);

    std::string str_buffer(reinterpret_cast<char*>(buffer.data()), reinterpret_cast<char*>(buffer.data()) + buffer.size());
    std::stringstream ss(str_buffer);

    try
    {
        boost::property_tree::ptree pt;
        boost::property_tree::read_json(ss, pt);

        std::vector<Sensor> sensors;
        for (auto const& node: pt.get_child("sensors"))
        {
            Sensor sensor;
            sensor.name = node.first;
            sensor.address = node.second.get<Sensor_Address>("address");
            sensor.id = node.second.get<Sensor_Id>("id");
            sensors.push_back(sensor);
        }

        emit sensorsReceived(sensors);
    }
    catch (std::exception const& e)
    {
        std::cerr << "Cannot deserialize response: " << e.what() << "\n";
    }
}

void Comms::processAddSensorRes()
{

}

void Comms::processRemoveSensorRes()
{

}

void Comms::processReportMeasurementReq()
{

}

void Comms::processSensorBoundReq()
{

}

void Comms::process()
{
    data::Server_Message message;
    while (m_bsChannel.get_next_message(message))
    {
        switch (message)
        {
        case data::Server_Message::GET_CONFIG_RES:
            processGetConfigRes();
            break;
        case data::Server_Message::SET_CONFIG_RES:
            processSetConfigRes();
            break;
        case data::Server_Message::GET_SENSORS_RES:
            processGetSensorsRes();
            break;
        case data::Server_Message::ADD_SENSOR_RES:
            processAddSensorRes();
            break;
        case data::Server_Message::REMOVE_SENSOR_RES:
            processRemoveSensorRes();
            break;
        case data::Server_Message::REPORT_MEASUREMENT_REQ:
            processReportMeasurementReq();
            break;
        case data::Server_Message::SENSOR_BOUND_REQ:
            processSensorBoundReq();
            break;
        default:
            std::cerr << "Invalid message received: " << (int)message << "\n";
        }
    }
}

