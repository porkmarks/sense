#include "Comms.h"
#include <cassert>
#include <iostream>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

//////////////////////////////////////////////////////////////////////////

Comms::Comms()
{
    m_broadcastSocket.bind(5555, QUdpSocket::ShareAddress);

    QObject::connect(&m_broadcastSocket, &QUdpSocket::readyRead, this, &Comms::broadcastReceived);
}

//////////////////////////////////////////////////////////////////////////

Comms::~Comms()
{
    m_discoveredBaseStations.clear();
    for (std::unique_ptr<ConnectedBaseStation>& cbs: m_connectedBaseStations)
    {
        cbs->socketAdapter.getSocket().disconnect();
    }
}

//////////////////////////////////////////////////////////////////////////

void Comms::broadcastReceived()
{
    while (m_broadcastSocket.hasPendingDatagrams())
    {
        if (m_broadcastSocket.pendingDatagramSize() == 6)
        {
            BaseStationDescriptor bs;
            if (m_broadcastSocket.readDatagram(reinterpret_cast<char*>(bs.mac.data()), bs.mac.size(), &bs.address, nullptr) == 6)
            {
                auto it = std::find_if(m_discoveredBaseStations.begin(), m_discoveredBaseStations.end(), [&bs](Comms::BaseStationDescriptor const& _bs) { return _bs.mac == bs.mac; });
                if (it == m_discoveredBaseStations.end())
                {
                    m_discoveredBaseStations.push_back(bs);
                    emit baseStationDiscovered(bs);
                }
            }
        }
        else
        {
            m_broadcastSocket.readDatagram(nullptr, 0);
        }
    }
}

//////////////////////////////////////////////////////////////////////////

bool Comms::connectToBaseStation(DB& db, QHostAddress const& address)
{
    auto it = std::find_if(m_discoveredBaseStations.begin(), m_discoveredBaseStations.end(), [&address](Comms::BaseStationDescriptor const& _bs) { return _bs.address == address; });
    if (it == m_discoveredBaseStations.end())
    {
        assert(false);
        return false;
    }

    ConnectedBaseStation* cbsPtr = new ConnectedBaseStation(db);
    std::unique_ptr<ConnectedBaseStation> cbs(cbsPtr);
    m_connectedBaseStations.push_back(std::move(cbs));

    QObject::connect(&cbsPtr->socketAdapter.getSocket(), &QTcpSocket::connected, [this, cbsPtr]() { connectedToBaseStation(cbsPtr); });
    QObject::connect(&cbsPtr->socketAdapter.getSocket(), &QTcpSocket::disconnected, [this, cbsPtr]() { disconnectedFromBaseStation(cbsPtr); });

    cbsPtr->socketAdapter.getSocket().disconnectFromHost();
    cbsPtr->socketAdapter.getSocket().connectToHost(address, 4444);

    return true;
}

//////////////////////////////////////////////////////////////////////////

void Comms::connectedToBaseStation(ConnectedBaseStation* cbs)
{
    if (cbs)
    {
        emit baseStationConnected(cbs->baseStation);

//        requestConfig();
//        requestSensors();
    }
}

//////////////////////////////////////////////////////////////////////////

void Comms::disconnectedFromBaseStation(ConnectedBaseStation* cbs)
{
    if (cbs)
    {
//        m_sensors.clear();
//        m_config = Config();

        emit baseStationDisconnected(cbs->baseStation);

        auto it = std::find_if(m_connectedBaseStations.begin(), m_connectedBaseStations.end(), [cbs](std::unique_ptr<ConnectedBaseStation> const& _cbs) { return _cbs.get() == cbs; });
        if (it == m_connectedBaseStations.end())
        {
            assert(false);
        }
        else
        {
            m_connectedBaseStations.erase(it);
        }
    }
}

//////////////////////////////////////////////////////////////////////////

std::vector<Comms::BaseStationDescriptor> const& Comms::getDiscoveredBaseStations() const
{
    return m_discoveredBaseStations;
}

//////////////////////////////////////////////////////////////////////////

void Comms::requestConfig()
{
//    m_bsChannel.send(data::Server_Message::GET_CONFIG_REQ, nullptr, 0);
}

//////////////////////////////////////////////////////////////////////////

void Comms::requestSensors()
{
//    m_bsChannel.send(data::Server_Message::GET_SENSORS_REQ, nullptr, 0);
}


//////////////////////////////////////////////////////////////////////////

void Comms::requestBindSensor(std::string const& name)
{
//    auto it = std::find_if(m_sensors.begin(), m_sensors.end(), [&name](Comms::Sensor const& s) { return s.name == name; });
//    if (it != m_sensors.end())
//    {
//        std::cerr << "Sensor already exists: " << name << "\n";
//        return;
//    }

//    m_sensorWaitingForBinding.name = name;

//    try
//    {
//        std::stringstream ss;
//        boost::property_tree::ptree pt;

//        pt.add("unbound_sensor_name", name);

//        boost::property_tree::write_json(ss, pt);

//        std::string str = ss.str();
//        m_bsChannel.send(data::Server_Message::ADD_SENSOR_REQ, str.data(), str.size());
//    }
//    catch (std::exception const& e)
//    {
//        std::cerr << "Cannot serialize request: " << e.what() << "\n";
//    }
}

//////////////////////////////////////////////////////////////////////////

void Comms::processGetConfigRes()
{
//    std::vector<uint8_t> buffer;
//    m_bsChannel.unpack(buffer);

//    std::string str_buffer(reinterpret_cast<char*>(buffer.data()), reinterpret_cast<char*>(buffer.data()) + buffer.size());
//    std::stringstream ss(str_buffer);

//    try
//    {
//        boost::property_tree::ptree pt;
//        boost::property_tree::read_json(ss, pt);

//        m_config.sensorsSleeping = pt.get<bool>("sensors_sleeping");
//        m_config.measurementPeriod = std::chrono::seconds(pt.get<std::chrono::seconds::rep>("measurement_period"));
//        m_config.commsPeriod = std::chrono::seconds(pt.get<std::chrono::seconds::rep>("comms_period"));
//        m_config.computedCommsPeriod = std::chrono::seconds(pt.get<std::chrono::seconds::rep>("computed_comms_period"));
//        m_config.baselineTimePoint = Clock::time_point(std::chrono::seconds(pt.get<std::chrono::seconds::rep>("baseline_time_point")));

//        emit configReceived(m_config);
//    }
//    catch (std::exception const& e)
//    {
//        std::cerr << "Cannot deserialize response: " << e.what() << "\n";
//    }
}

//////////////////////////////////////////////////////////////////////////

void Comms::processSetConfigRes()
{

}

//////////////////////////////////////////////////////////////////////////

void Comms::processGetSensorsRes()
{
//    std::vector<uint8_t> buffer;
//    m_bsChannel.unpack(buffer);

//    std::string str_buffer(reinterpret_cast<char*>(buffer.data()), reinterpret_cast<char*>(buffer.data()) + buffer.size());
//    std::stringstream ss(str_buffer);

//    try
//    {
//        boost::property_tree::ptree pt;
//        boost::property_tree::read_json(ss, pt);

//        m_sensors.clear();
//        if (pt.get_child_optional("sensors"))
//        {
//            for (auto const& node: pt.get_child("sensors"))
//            {
//                Sensor sensor;
//                sensor.name = node.first;
//                sensor.address = node.second.get<Sensor_Address>("address");
//                sensor.id = node.second.get<Sensor_Id>("id");
//                m_sensors.push_back(sensor);
//            }
//        }
//    }
//    catch (std::exception const& e)
//    {
//        std::cerr << "Cannot deserialize response: " << e.what() << "\n";
//        return;
//    }

//    for (Sensor const& sensor: m_sensors)
//    {
//        emit sensorAdded(sensor);
//    }
}

//////////////////////////////////////////////////////////////////////////

void Comms::processAddSensorRes()
{

}

//////////////////////////////////////////////////////////////////////////

void Comms::processRemoveSensorRes()
{

}

//////////////////////////////////////////////////////////////////////////

void Comms::processReportMeasurementReq()
{

}

//////////////////////////////////////////////////////////////////////////

void Comms::processSensorBoundReq()
{
//    std::vector<uint8_t> buffer;
//    m_bsChannel.unpack(buffer);

//    std::string str_buffer(reinterpret_cast<char*>(buffer.data()), reinterpret_cast<char*>(buffer.data()) + buffer.size());
//    std::stringstream ss(str_buffer);

//    try
//    {
//        boost::property_tree::ptree pt;
//        boost::property_tree::read_json(ss, pt);

//        Sensor sensor;
//        sensor.name = pt.get<std::string>("name");;
//        sensor.address = pt.get<Sensor_Address>("address");
//        sensor.id = pt.get<Sensor_Id>("id");

//        if (sensor.name != m_sensorWaitingForBinding.name)
//        {
//            std::cerr << "Was waiting to bind sensor '" << m_sensorWaitingForBinding.name << "' but received '" << sensor.name << "'\n";
//        }

//        m_sensors.push_back(sensor);

//        emit sensorAdded(sensor);
//    }
//    catch (std::exception const& e)
//    {
//        std::cerr << "Cannot deserialize response: " << e.what() << "\n";
//    }
}

//////////////////////////////////////////////////////////////////////////

void Comms::process()
{
//    data::Server_Message message;
//    while (m_bsChannel.get_next_message(message))
//    {
//        switch (message)
//        {
//        case data::Server_Message::GET_CONFIG_RES:
//            processGetConfigRes();
//            break;
//        case data::Server_Message::SET_CONFIG_RES:
//            processSetConfigRes();
//            break;
//        case data::Server_Message::GET_SENSORS_RES:
//            processGetSensorsRes();
//            break;
//        case data::Server_Message::ADD_SENSOR_RES:
//            processAddSensorRes();
//            break;
//        case data::Server_Message::REMOVE_SENSOR_RES:
//            processRemoveSensorRes();
//            break;
//        case data::Server_Message::REPORT_MEASUREMENT_REQ:
//            processReportMeasurementReq();
//            break;
//        case data::Server_Message::SENSOR_BOUND_REQ:
//            processSensorBoundReq();
//            break;
//        default:
//            std::cerr << "Invalid message received: " << (int)message << "\n";
//        }
//    }
}

//////////////////////////////////////////////////////////////////////////

