#include "Comms.h"
#include <cassert>
#include <iostream>

#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/writer.h"
#include "rapidjson/reader.h"
#include "rapidjson/stringbuffer.h"

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

    QObject::connect(&db, &DB::configChanged, [this, cbsPtr]() { sendConfig(*cbsPtr); });
    QObject::connect(&db, &DB::sensorAdded, [this, cbsPtr](DB::SensorId id) { requestBindSensor(*cbsPtr, id); });
    QObject::connect(&db, &DB::sensorChanged, [this, cbsPtr](DB::SensorId) { sendSensors(*cbsPtr); });
    QObject::connect(&db, &DB::sensorRemoved, [this, cbsPtr](DB::SensorId) { sendSensors(*cbsPtr); });

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
        cbs->socketAdapter.start();

        sendConfig(*cbs);
        sendSensors(*cbs);
    }
}

//////////////////////////////////////////////////////////////////////////

void Comms::disconnectedFromBaseStation(ConnectedBaseStation* cbs)
{
    if (cbs)
    {
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

void Comms::sendConfig(ConnectedBaseStation& cbs)
{
    DB::Config const& config = cbs.baseStation.db.getConfig();

    rapidjson::Document document;
    document.SetObject();
    document.AddMember("name", rapidjson::Value(config.descriptor.name.c_str(), document.GetAllocator()), document.GetAllocator());
    document.AddMember("sensors_sleeping", config.descriptor.sensorsSleeping, document.GetAllocator());
    document.AddMember("measurement_period", static_cast<int64_t>(std::chrono::duration_cast<std::chrono::seconds>(config.descriptor.measurementPeriod).count()), document.GetAllocator());
    document.AddMember("comms_period", static_cast<int64_t>(std::chrono::duration_cast<std::chrono::seconds>(config.descriptor.commsPeriod).count()), document.GetAllocator());
    document.AddMember("baseline", static_cast<int64_t>(std::chrono::duration_cast<std::chrono::seconds>(config.baselineTimePoint.time_since_epoch()).count()), document.GetAllocator());

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    document.Accept(writer);

    cbs.channel.send(data::Server_Message::SET_CONFIG_REQ, buffer.GetString(), buffer.GetSize());
}

//////////////////////////////////////////////////////////////////////////

void Comms::sendSensors(ConnectedBaseStation& cbs)
{
    DB& db = cbs.baseStation.db;

    rapidjson::Document document;
    document.SetArray();
    for (size_t i = 0; i < db.getSensorCount(); i++)
    {
        DB::Sensor const& sensor = db.getSensor(i);
        rapidjson::Value sensorj;
        sensorj.SetObject();
        sensorj.AddMember("name", rapidjson::Value(sensor.descriptor.name.c_str(), document.GetAllocator()), document.GetAllocator());
        sensorj.AddMember("id", sensor.id, document.GetAllocator());
        sensorj.AddMember("address", sensor.address, document.GetAllocator());
        document.PushBack(sensorj, document.GetAllocator());
    }

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    document.Accept(writer);

    cbs.channel.send(data::Server_Message::SET_SENSORS_REQ, buffer.GetString(), buffer.GetSize());
}

//////////////////////////////////////////////////////////////////////////

void Comms::requestBindSensor(ConnectedBaseStation& cbs, DB::SensorId sensorId)
{
    DB& db = cbs.baseStation.db;

    int32_t sensorIndex = db.findSensorIndexById(sensorId);
    if (sensorIndex < 0)
    {
        std::cerr << "Cannot send bind request: Bad sensor id.\n";
        return;
    }

    DB::Sensor const& sensor = db.getSensor(sensorIndex);
    if (sensor.state != DB::Sensor::State::Unbound)
    {
        std::cerr << "Cannot send bind request: Sensor in bad state.\n";
        return;
    }

    rapidjson::Document document;
    document.SetObject();
    document.AddMember("name", rapidjson::Value(sensor.descriptor.name.c_str(), document.GetAllocator()), document.GetAllocator());
    document.AddMember("id", sensor.id, document.GetAllocator());

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    document.Accept(writer);

    cbs.channel.send(data::Server_Message::ADD_SENSOR_REQ, buffer.GetString(), buffer.GetSize());
}

//////////////////////////////////////////////////////////////////////////

void Comms::processSetSensorsRes(ConnectedBaseStation& cbs)
{
    std::vector<uint8_t> buffer;
    cbs.channel.unpack(buffer);

    if (buffer.size() != 1)
    {
        std::cerr << "Cannot deserialize request: Bad data.\n";
        return;
    }

    bool ok = *reinterpret_cast<bool const*>(buffer.data());
    if (!ok)
    {
        std::cerr << "Setting the sensors FAILED.\n";
        return;
    }

    std::cout << "Setting the sensors succeded.\n";
}

//////////////////////////////////////////////////////////////////////////

void Comms::processSetConfigRes(ConnectedBaseStation& cbs)
{
    std::vector<uint8_t> buffer;
    cbs.channel.unpack(buffer);

    if (buffer.size() != 1)
    {
        std::cerr << "Cannot deserialize request: Bad data.\n";
        return;
    }

    bool ok = *reinterpret_cast<bool const*>(buffer.data());
    if (!ok)
    {
        std::cerr << "Setting the config FAILED.\n";
        return;
    }

    std::cout << "Setting the config succeded.\n";
}

//////////////////////////////////////////////////////////////////////////

void Comms::processAddSensorRes(ConnectedBaseStation& cbs)
{
    std::vector<uint8_t> buffer;
    cbs.channel.unpack(buffer);

    if (buffer.size() != 1)
    {
        std::cerr << "Cannot deserialize request: Bad data.\n";
        return;
    }

    bool ok = *reinterpret_cast<bool const*>(buffer.data());
    if (!ok)
    {
        std::cerr << "Adding the sensor FAILED.\n";
        return;
    }

    std::cout << "Adding the sensor succeded.\n";
}

//////////////////////////////////////////////////////////////////////////

void Comms::processReportMeasurementReq(ConnectedBaseStation& cbs)
{
    std::vector<uint8_t> buffer;
    cbs.channel.unpack(buffer);

    DB::Measurement measurement;
    bool ok = false;

    {
        rapidjson::Document document;
        document.Parse(reinterpret_cast<const char*>(buffer.data()), buffer.size());
        if (document.GetParseError() != rapidjson::ParseErrorCode::kParseErrorNone)
        {
            std::cerr << "Cannot deserialize request: " << rapidjson::GetParseErrorFunc(document.GetParseError()) << "\n";
            goto end;
        }
        if (!document.IsObject())
        {
            std::cerr << "Cannot deserialize request: Bad document.\n";
            goto end;
        }

        auto it = document.FindMember("sensor_id");
        if (it == document.MemberEnd() || !it->value.IsUint())
        {
            std::cerr << "Cannot deserialize request: Missing sensor_id.\n";
            goto end;
        }
        measurement.sensorId = it->value.GetUint();

        it = document.FindMember("index");
        if (it == document.MemberEnd() || !it->value.IsUint())
        {
            std::cerr << "Cannot deserialize request: Missing index.\n";
            goto end;
        }
        measurement.index = it->value.GetUint();

        it = document.FindMember("time_point");
        if (it == document.MemberEnd() || !it->value.IsInt64())
        {
            std::cerr << "Cannot deserialize request: Missing time_point.\n";
            goto end;
        }
        measurement.timePoint = DB::Clock::time_point(std::chrono::seconds(it->value.GetInt64()));

        it = document.FindMember("temperature");
        if (it == document.MemberEnd() || !it->value.IsNumber())
        {
            std::cerr << "Cannot deserialize request: Missing temperature.\n";
            goto end;
        }
        measurement.temperature = static_cast<float>(it->value.GetDouble());

        it = document.FindMember("humidity");
        if (it == document.MemberEnd() || !it->value.IsNumber())
        {
            std::cerr << "Cannot deserialize request: Missing humidity.\n";
            goto end;
        }
        measurement.humidity = static_cast<float>(it->value.GetDouble());

        it = document.FindMember("vcc");
        if (it == document.MemberEnd() || !it->value.IsNumber())
        {
            std::cerr << "Cannot deserialize request: Missing vcc.\n";
            goto end;
        }
        measurement.vcc = static_cast<float>(it->value.GetDouble());

        it = document.FindMember("b2s");
        if (it == document.MemberEnd() || !it->value.IsInt())
        {
            std::cerr << "Cannot deserialize request: Missing b2s.\n";
            goto end;
        }
        measurement.b2s = static_cast<int8_t>(it->value.GetInt());

        it = document.FindMember("s2b");
        if (it == document.MemberEnd() || !it->value.IsInt())
        {
            std::cerr << "Cannot deserialize request: Missing s2b.\n";
            goto end;
        }
        measurement.s2b = static_cast<int8_t>(it->value.GetInt());

        it = document.FindMember("sensor_errors");
        if (it == document.MemberEnd() || !it->value.IsUint())
        {
            std::cerr << "Cannot deserialize request: Missing sensor_errors.\n";
            goto end;
        }
        measurement.sensorErrors = static_cast<uint8_t>(it->value.GetUint());

        if (!cbs.baseStation.db.addMeasurement(measurement))
        {
            std::cerr << "Cannot deserialize request: Adding measurement failed.\n";
            goto end;
        }

        ok = true;
    }

end:
    {
        DB& db = cbs.baseStation.db;

        rapidjson::Document document;
        document.SetObject();
        document.AddMember("sensor_id", measurement.sensorId, document.GetAllocator());
        document.AddMember("index", measurement.index, document.GetAllocator());
        document.AddMember("ok", ok, document.GetAllocator());

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        document.Accept(writer);

        cbs.channel.send(data::Server_Message::REPORT_MEASUREMENT_RES, buffer.GetString(), buffer.GetSize());
    }
}

//////////////////////////////////////////////////////////////////////////

void Comms::processSensorBoundReq(ConnectedBaseStation& cbs)
{
    std::vector<uint8_t> buffer;
    cbs.channel.unpack(buffer);

    bool ok = false;

    {
        rapidjson::Document document;
        document.Parse(reinterpret_cast<const char*>(buffer.data()), buffer.size());
        if (document.GetParseError() != rapidjson::ParseErrorCode::kParseErrorNone)
        {
            std::cerr << "Cannot deserialize request: " << rapidjson::GetParseErrorFunc(document.GetParseError()) << "\n";
            goto end;
        }
        if (!document.IsObject())
        {
            std::cerr << "Cannot deserialize request: Bad document.\n";
            goto end;
        }

        auto it = document.FindMember("id");
        if (it == document.MemberEnd() || !it->value.IsUint())
        {
            std::cerr << "Cannot deserialize request: Missing id.\n";
            goto end;
        }
        DB::SensorId id = it->value.GetUint();

        it = document.FindMember("address");
        if (it == document.MemberEnd() || !it->value.IsUint())
        {
            std::cerr << "Cannot deserialize request: Missing address.\n";
            goto end;
        }
        DB::SensorAddress address = it->value.GetUint();

        if (!cbs.baseStation.db.bindSensor(id, address))
        {
            std::cerr << "Cannot deserialize request: Bind failed.\n";
            goto end;
        }

        ok = true;
    }

end:
    cbs.channel.send(data::Server_Message::SENSOR_BOUND_RES, &ok, 1);
}

//////////////////////////////////////////////////////////////////////////

void Comms::process()
{
    data::Server_Message message;
    for (std::unique_ptr<ConnectedBaseStation>& cbs: m_connectedBaseStations)
    {
        while (cbs->channel.get_next_message(message))
        {
            switch (message)
            {
            case data::Server_Message::SET_CONFIG_RES:
                processSetConfigRes(*cbs);
                break;
            case data::Server_Message::SET_SENSORS_RES:
                processSetSensorsRes(*cbs);
                break;
            case data::Server_Message::ADD_SENSOR_RES:
                processAddSensorRes(*cbs);
                break;
            case data::Server_Message::REPORT_MEASUREMENT_REQ:
                processReportMeasurementReq(*cbs);
                break;
            case data::Server_Message::SENSOR_BOUND_REQ:
                processSensorBoundReq(*cbs);
                break;
            default:
                std::cerr << "Invalid message received: " << (int)message << "\n";
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////////

