#include "Comms.h"
#include <cassert>
#include <iostream>

#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/writer.h"
#include "rapidjson/reader.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/error/en.h"
#include "Logger.h"
#include "Settings.h"

extern std::string getMacStr(Settings::BaseStationDescriptor::Mac const& mac);
extern Logger s_logger;

//////////////////////////////////////////////////////////////////////////

Comms::Comms()
{
    m_broadcastSocket.bind(5555, QUdpSocket::ShareAddress);

    connect(&m_broadcastSocket, &QUdpSocket::readyRead, this, &Comms::broadcastReceived);
}

//////////////////////////////////////////////////////////////////////////

Comms::~Comms()
{
    m_discoveredBaseStations.clear();
    for (std::unique_ptr<InitializedBaseStation>& cbs: m_initializedBaseStations)
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
                //reconnect?
                {
                    auto it = std::find_if(m_initializedBaseStations.begin(), m_initializedBaseStations.end(), [&bs](std::unique_ptr<InitializedBaseStation> const& cbs) { return cbs->descriptor.mac == bs.mac; });
                    if (it != m_initializedBaseStations.end() && (*it)->isConnected == false)
                    {
                        reconnectToBaseStation(it->get());
                    }
                }

                //new BS?
                {
                    auto it = std::find_if(m_discoveredBaseStations.begin(), m_discoveredBaseStations.end(), [&bs](Comms::BaseStationDescriptor const& _bs) { return _bs.mac == bs.mac; });
                    if (it == m_discoveredBaseStations.end())
                    {
                        m_discoveredBaseStations.push_back(bs);
                        emit baseStationDiscovered(bs);
                    }
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

bool Comms::connectToBaseStation(DB& db, Mac const& mac)
{
    auto it = std::find_if(m_discoveredBaseStations.begin(), m_discoveredBaseStations.end(), [&mac](Comms::BaseStationDescriptor const& _bs) { return _bs.mac == mac; });
    if (it == m_discoveredBaseStations.end())
    {
        s_logger.logWarning(QString("Attempting to connect to undiscovered BS %1. Postponed until discovery.").arg(getMacStr(mac).c_str()));
        return false;
    }

    s_logger.logInfo(QString("Attempting to connect to BS %1").arg(getMacStr(mac).c_str()));

    InitializedBaseStation* cbsPtr = new InitializedBaseStation(db, *it);
    std::unique_ptr<InitializedBaseStation> cbs(cbsPtr);
    m_initializedBaseStations.push_back(std::move(cbs));

    cbsPtr->connections.push_back(connect(&cbsPtr->socketAdapter.getSocket(), &QTcpSocket::connected, [this, cbsPtr]() { connectedToBaseStation(cbsPtr); }));
    cbsPtr->connections.push_back(connect(&cbsPtr->socketAdapter.getSocket(), &QTcpSocket::disconnected, [this, cbsPtr]() { disconnectedFromBaseStation(cbsPtr); }));
    cbsPtr->connections.push_back(connect(&cbsPtr->socketAdapter.getSocket(), static_cast<void(QTcpSocket::*)(QAbstractSocket::SocketError)>(&QTcpSocket::error), [this, cbsPtr]()
    {
        s_logger.logCritical(QString("Socket error for BS %1: %2").arg(getMacStr(cbsPtr->descriptor.mac).c_str()).arg(cbsPtr->socketAdapter.getSocket().errorString()));
        disconnectedFromBaseStation(cbsPtr);
    }));

    cbsPtr->connections.push_back(connect(&db, &DB::sensorsConfigAdded, [this, cbsPtr]() { sendLastSensorsConfig(*cbsPtr); }));
    cbsPtr->connections.push_back(connect(&db, &DB::sensorAdded, [this, cbsPtr](DB::SensorId id) { requestBindSensor(*cbsPtr, id); }));
    cbsPtr->connections.push_back(connect(&db, &DB::sensorChanged, [this, cbsPtr](DB::SensorId) { sendSensors(*cbsPtr); }));
    cbsPtr->connections.push_back(connect(&db, &DB::sensorRemoved, [this, cbsPtr](DB::SensorId) { sendSensors(*cbsPtr); }));

    cbsPtr->isConnecting = true;
    cbsPtr->socketAdapter.getSocket().connectToHost(it->address, 4444);

    return true;
}

//////////////////////////////////////////////////////////////////////////

void Comms::reconnectToBaseStation(InitializedBaseStation* cbs)
{
    if (cbs->isConnecting)
    {
        return;
    }
    s_logger.logInfo(QString("Attempting to reconnect to BS %1").arg(getMacStr(cbs->descriptor.mac).c_str()));

    cbs->isConnecting = true;
    cbs->socketAdapter.getSocket().connectToHost(cbs->descriptor.address, 4444);
}

//////////////////////////////////////////////////////////////////////////

void Comms::connectedToBaseStation(InitializedBaseStation* cbs)
{
    if (cbs)
    {
        s_logger.logInfo(QString("Connected to BS %1").arg(getMacStr(cbs->descriptor.mac).c_str()));

        cbs->isConnecting = false;
        cbs->isConnected = true;

        emit baseStationConnected(cbs->descriptor);
        cbs->socketAdapter.start();

        sendSensorsConfigs(*cbs);
        sendSensors(*cbs);
    }
}

//////////////////////////////////////////////////////////////////////////

void Comms::disconnectedFromBaseStation(InitializedBaseStation* cbs)
{
    if (cbs)
    {
        if (cbs->isConnected)
        {
            s_logger.logWarning(QString("Disconnected from BS %1").arg(getMacStr(cbs->descriptor.mac).c_str()));
            emit baseStationDisconnected(cbs->descriptor);
        }

        cbs->isConnected = false;
        cbs->isConnecting = false;
    }
}

//////////////////////////////////////////////////////////////////////////

bool Comms::isBaseStationConnected(Mac const& mac) const
{
    auto it = std::find_if(m_initializedBaseStations.begin(), m_initializedBaseStations.end(), [&mac](std::unique_ptr<InitializedBaseStation> const& cbs) { return cbs->descriptor.mac == mac; });
    return (it != m_initializedBaseStations.end() && (*it)->isConnected == true);
}

//////////////////////////////////////////////////////////////////////////

QHostAddress Comms::getBaseStationAddress(Mac const& mac) const
{
    {
        auto it = std::find_if(m_initializedBaseStations.begin(), m_initializedBaseStations.end(), [&mac](std::unique_ptr<InitializedBaseStation> const& cbs) { return cbs->descriptor.mac == mac; });
        if (it != m_initializedBaseStations.end())
        {
            return (*it)->descriptor.address;
        }
    }
    {
        auto it = std::find_if(m_discoveredBaseStations.begin(), m_discoveredBaseStations.end(), [&mac](Comms::BaseStationDescriptor const& _bs) { return _bs.mac == mac; });
        if (it != m_discoveredBaseStations.end())
        {
            return it->address;
        }
    }

    return QHostAddress();
}

//////////////////////////////////////////////////////////////////////////

void Comms::processSensorDetails(InitializedBaseStation& cbs, std::vector<uint8_t> const& data)
{
    rapidjson::Document document;
    document.Parse(reinterpret_cast<const char*>(data.data()), data.size());
    if (document.GetParseError() != rapidjson::ParseErrorCode::kParseErrorNone)
    {
        s_logger.logCritical(QString("Cannot deserialize sensor details: %1").arg(rapidjson::GetParseError_En(document.GetParseError())));
        return;
    }
    if (!document.IsObject())
    {
        s_logger.logCritical(QString("Cannot deserialize sensor details: Bad document."));
        return;
    }

    std::vector<DB::SensorDetails> sensorsDetails;

    {
        auto it = document.FindMember("sensors");
        if (it == document.MemberEnd() || !it->value.IsArray())
        {
            s_logger.logCritical(QString("Cannot deserialize sensor details: Missing sensors array."));
            return;
        }

        rapidjson::Value const& sensorArrayj = it->value;
        sensorsDetails.reserve(sensorArrayj.Size());

        for (size_t i = 0; i < sensorArrayj.Size(); i++)
        {
            DB::SensorDetails details;
            rapidjson::Value const& sensorj = sensorArrayj[i];

            auto it = sensorj.FindMember("id");
            if (it == sensorj.MemberEnd() || !it->value.IsUint())
            {
                s_logger.logCritical(QString("Cannot deserialize sensor details: Missing id."));
                continue;
            }
            details.id = it->value.GetUint();

            int32_t sensorIndex = cbs.db.findSensorIndexById(details.id);
            if (sensorIndex < 0)
            {
                s_logger.logCritical(QString("Cannot deserialize sensor details: cannot find sensor id %1.").arg(details.id));
                continue;
            }

            it = sensorj.FindMember("serial_number");
            if (it == sensorj.MemberEnd() || !it->value.IsUint())
            {
                s_logger.logCritical(QString("Cannot deserialize sensor details: Missing serial_number."));
                continue;
            }
            //uint32_t serialNumber = it->value.GetUint();

            DB::Sensor::Calibration calibration;
            it = sensorj.FindMember("temperature_bias");
            if (it == sensorj.MemberEnd() || !it->value.IsNumber())
            {
                s_logger.logCritical(QString("Cannot deserialize sensor details: Missing temperature_bias."));
                continue;
            }
            calibration.temperatureBias = static_cast<float>(it->value.GetDouble());

            it = sensorj.FindMember("humidity_bias");
            if (it == sensorj.MemberEnd() || !it->value.IsNumber())
            {
                s_logger.logCritical(QString("Cannot deserialize sensor details: Missing humidity_bias."));
                continue;
            }
            calibration.humidityBias = static_cast<float>(it->value.GetDouble());

            it = sensorj.FindMember("b2s");
            if (it == sensorj.MemberEnd() || !it->value.IsInt())
            {
                s_logger.logCritical(QString("Cannot deserialize sensor details: Missing b2s."));
                continue;
            }
            //int8_t b2s = static_cast<int8_t>(it->value.GetInt());

            it = sensorj.FindMember("first_recorded_measurement_index");
            if (it == sensorj.MemberEnd() || !it->value.IsUint())
            {
                s_logger.logCritical(QString("Cannot deserialize sensor details: Missing first_recorded_measurement_index."));
                continue;
            }
            //uint32_t firstRecordedMeasurementIndex = it->value.GetUint();

            it = sensorj.FindMember("max_confirmed_measurement_index");
            if (it == sensorj.MemberEnd() || !it->value.IsUint())
            {
                s_logger.logCritical(QString("Cannot deserialize sensor details: Missing max_confirmed_measurement_index."));
                continue;
            }
            //uint32_t maxConfirmedMeasurementIndex = it->value.GetUint();

            it = sensorj.FindMember("recorded_measurement_count");
            if (it == sensorj.MemberEnd() || !it->value.IsUint())
            {
                s_logger.logCritical(QString("Cannot deserialize sensor details: Missing recorded_measurement_count."));
                continue;
            }
            details.storedMeasurementCount = it->value.GetUint();

            it = sensorj.FindMember("next_measurement_dt");
            if (it == sensorj.MemberEnd() || !it->value.IsInt())
            {
                s_logger.logCritical(QString("Cannot deserialize sensor details: Missing next_measurement_dt."));
                continue;
            }
            details.nextMeasurementTimePoint = DB::Clock::now() + std::chrono::seconds(it->value.GetInt());

            it = sensorj.FindMember("last_comms_tp");
            if (it == sensorj.MemberEnd() || !it->value.IsUint64())
            {
                s_logger.logCritical(QString("Cannot deserialize sensor details: Missing last_comms_tp."));
                continue;
            }
            details.lastCommsTimePoint = DB::Clock::from_time_t(time_t(it->value.GetUint64()));

            it = sensorj.FindMember("sleeping");
            if (it == sensorj.MemberEnd() || !it->value.IsBool())
            {
                s_logger.logCritical(QString("Cannot deserialize sensor details: Missing sleeping."));
                continue;
            }
            details.sleeping = it->value.GetBool();

            sensorsDetails.push_back(details);
        }
    }

    if (!cbs.db.setSensorsDetails(sensorsDetails))
    {
        s_logger.logCritical(QString("Cannot deserialize sensor details: failed to set sensor details."));
    }
}

//////////////////////////////////////////////////////////////////////////

void Comms::processConfigs(InitializedBaseStation& cbs, std::vector<uint8_t> const& data)
{
    rapidjson::Document document;
    document.Parse(reinterpret_cast<const char*>(data.data()), data.size());
    if (document.GetParseError() != rapidjson::ParseErrorCode::kParseErrorNone)
    {
        s_logger.logCritical(QString("Cannot deserialize configs: %1").arg(rapidjson::GetParseError_En(document.GetParseError())));
        return;
    }
    if (!document.IsObject())
    {
        s_logger.logCritical(QString("Cannot deserialize configs: Bad document."));
        return;
    }

    {
        auto it = document.FindMember("configs");
        if (it == document.MemberEnd() || !it->value.IsArray())
        {
            s_logger.logCritical(QString("Cannot deserialize configs: Missing configs array."));
            return;
        }

        std::vector<DB::SensorsConfig> configs;

        rapidjson::Value const& configArrayj = it->value;
        for (size_t i = 0; i < configArrayj.Size(); i++)
        {
            rapidjson::Value const& configj = configArrayj[i];
            DB::SensorsConfig config;

            auto it = configj.FindMember("name");
            if (it == configj.MemberEnd() || !it->value.IsString())
            {
                s_logger.logCritical(QString("Cannot deserialize configs: Missing name."));
                continue;
            }
            config.descriptor.name = it->value.GetString();

            it = configj.FindMember("sensors_sleeping");
            if (it == configj.MemberEnd() || !it->value.IsBool())
            {
                s_logger.logCritical(QString("Cannot deserialize config '%1': Missing sensors_sleeping.").arg(config.descriptor.name.c_str()));
                continue;
            }
            config.descriptor.sensorsSleeping = it->value.GetBool();

            it = configj.FindMember("sensors_power");
            if (it == configj.MemberEnd() || !it->value.IsUint())
            {
                s_logger.logCritical(QString("Cannot deserialize config '%1': Missing sensors_power.").arg(config.descriptor.name.c_str()));
                continue;
            }
            config.descriptor.sensorsPower = static_cast<uint8_t>(it->value.GetUint());

            it = configj.FindMember("measurement_period");
            if (it == configj.MemberEnd() || !it->value.IsInt64())
            {
                s_logger.logCritical(QString("Cannot deserialize config '%1': Missing measurement_period.").arg(config.descriptor.name.c_str()));
                continue;
            }
            config.descriptor.measurementPeriod = std::chrono::seconds(it->value.GetInt64());

            it = configj.FindMember("comms_period");
            if (it == configj.MemberEnd() || !it->value.IsInt64())
            {
                s_logger.logCritical(QString("Cannot deserialize config '%1': Missing comms_period.").arg(config.descriptor.name.c_str()));
                continue;
            }
            config.descriptor.commsPeriod = std::chrono::seconds(it->value.GetInt64());

            it = configj.FindMember("baseline_measurement_tp");
            if (it == configj.MemberEnd() || !it->value.IsUint64())
            {
                s_logger.logCritical(QString("Cannot deserialize config '%1': Missing baseline_measurement_tp.").arg(config.descriptor.name.c_str()));
                continue;
            }
            config.baselineMeasurementTimePoint = DB::Clock::from_time_t(time_t(it->value.GetUint64()));

            it = configj.FindMember("baseline_measurement_index");
            if (it == configj.MemberEnd() || !it->value.IsUint())
            {
                s_logger.logCritical(QString("Cannot deserialize config '%1': Missing baseline_measurement_index.").arg(config.descriptor.name.c_str()));
                continue;
            }
            config.baselineMeasurementIndex = it->value.GetUint();
            configs.push_back(config);
        }

        if (!cbs.db.setSensorsConfigs(configs))
        {
            s_logger.logCritical(QString("Cannot deserialize configs: failed to set configs."));
        }
    }
}

//////////////////////////////////////////////////////////////////////////

void Comms::sendLastSensorsConfig(InitializedBaseStation& cbs)
{
    DB::SensorsConfig const& config = cbs.db.getLastSensorsConfig();

    rapidjson::Document document;
    document.SetObject();
    document.AddMember("name", rapidjson::Value(config.descriptor.name.c_str(), document.GetAllocator()), document.GetAllocator());
    document.AddMember("sensors_sleeping", config.descriptor.sensorsSleeping, document.GetAllocator());
    document.AddMember("sensors_power", static_cast<uint32_t>(config.descriptor.sensorsPower), document.GetAllocator());
    document.AddMember("measurement_period", static_cast<int64_t>(std::chrono::duration_cast<std::chrono::seconds>(config.descriptor.measurementPeriod).count()), document.GetAllocator());
    document.AddMember("comms_period", static_cast<int64_t>(std::chrono::duration_cast<std::chrono::seconds>(config.descriptor.commsPeriod).count()), document.GetAllocator());

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    document.Accept(writer);
    cbs.channel.send(data::Server_Message::ADD_CONFIG_REQ, buffer.GetString(), buffer.GetSize());
}

//////////////////////////////////////////////////////////////////////////

void Comms::sendSensorsConfigs(InitializedBaseStation& cbs)
{
    DB& db = cbs.db;

    rapidjson::Document document;
    document.SetArray();
    for (size_t i = 0; i < db.getSensorsConfigCount(); i++)
    {
        DB::SensorsConfig const& config = db.getSensorsConfig(i);
        rapidjson::Value configj;
        configj.SetObject();
        configj.AddMember("name", rapidjson::Value(config.descriptor.name.c_str(), document.GetAllocator()), document.GetAllocator());
        configj.AddMember("sensors_sleeping", config.descriptor.sensorsSleeping, document.GetAllocator());
        configj.AddMember("sensors_power", static_cast<uint32_t>(config.descriptor.sensorsPower), document.GetAllocator());
        configj.AddMember("measurement_period", static_cast<int64_t>(std::chrono::duration_cast<std::chrono::seconds>(config.descriptor.measurementPeriod).count()), document.GetAllocator());
        configj.AddMember("comms_period", static_cast<int64_t>(std::chrono::duration_cast<std::chrono::seconds>(config.descriptor.commsPeriod).count()), document.GetAllocator());
        configj.AddMember("baseline_measurement_tp", static_cast<uint64_t>(Clock::to_time_t(config.baselineMeasurementTimePoint)), document.GetAllocator());
        configj.AddMember("baseline_measurement_index", config.baselineMeasurementIndex, document.GetAllocator());
        document.PushBack(configj, document.GetAllocator());
    }

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    document.Accept(writer);
    cbs.channel.send(data::Server_Message::SET_CONFIGS_REQ, buffer.GetString(), buffer.GetSize());
}

//////////////////////////////////////////////////////////////////////////

void Comms::sendSensors(InitializedBaseStation& cbs)
{
    DB& db = cbs.db;

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
        sensorj.AddMember("temperature_bias", sensor.calibration.temperatureBias, document.GetAllocator());
        sensorj.AddMember("humidity_bias", sensor.calibration.humidityBias, document.GetAllocator());
        sensorj.AddMember("serial_number", sensor.serialNumber, document.GetAllocator());
        sensorj.AddMember("sleeping", sensor.state == DB::Sensor::State::Sleeping, document.GetAllocator());
        document.PushBack(sensorj, document.GetAllocator());
    }

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    document.Accept(writer);

    cbs.channel.send(data::Server_Message::SET_SENSORS_REQ, buffer.GetString(), buffer.GetSize());
}

//////////////////////////////////////////////////////////////////////////

void Comms::requestBindSensor(InitializedBaseStation& cbs, DB::SensorId sensorId)
{
    DB& db = cbs.db;

    int32_t _sensorIndex = db.findSensorIndexById(sensorId);
    if (_sensorIndex < 0)
    {
        s_logger.logCritical(QString("Cannot send bind request: Bad sensor id %1.").arg(sensorId));
        return;
    }
    size_t sensorIndex = static_cast<size_t>(_sensorIndex);

    DB::Sensor const& sensor = db.getSensor(sensorIndex);
    if (sensor.state != DB::Sensor::State::Unbound)
    {
        s_logger.logCritical(QString("Cannot send bind request: Sensor in bad state: %1.").arg(static_cast<int>(sensor.state)));
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

void Comms::processSetSensorsRes(InitializedBaseStation& cbs)
{
    std::vector<uint8_t> buffer;
    cbs.channel.unpack(buffer);

    if (buffer.empty())
    {
        s_logger.logCritical(QString("Setting the sensors FAILED."));
        return;
    }

    s_logger.logInfo(QString("Setting the sensors succeded."));

    processSensorDetails(cbs, buffer);
}

//////////////////////////////////////////////////////////////////////////

void Comms::processAddConfigRes(InitializedBaseStation& cbs)
{
    std::vector<uint8_t> buffer;
    cbs.channel.unpack(buffer);

    if (buffer.empty())
    {
        s_logger.logCritical(QString("Adding the config FAILED."));
        return;
    }

    s_logger.logInfo(QString("Adding the config succeded."));
    processConfigs(cbs, buffer);
}

//////////////////////////////////////////////////////////////////////////

void Comms::processSetConfigsRes(InitializedBaseStation& cbs)
{
    std::vector<uint8_t> buffer;
    cbs.channel.unpack(buffer);

    if (buffer.empty())
    {
        s_logger.logCritical(QString("Setting the config FAILED."));
        return;
    }

    s_logger.logInfo(QString("Setting the config succeded."));
    processConfigs(cbs, buffer);
}

//////////////////////////////////////////////////////////////////////////

void Comms::processAddSensorRes(InitializedBaseStation& cbs)
{
    std::vector<uint8_t> buffer;
    cbs.channel.unpack(buffer);

    if (buffer.empty())
    {
        s_logger.logCritical(QString("Adding the sensor FAILED."));
        return;
    }

    s_logger.logInfo(QString("Adding the sensor succeded."));
    processSensorDetails(cbs, buffer);
}

//////////////////////////////////////////////////////////////////////////

void Comms::processReportMeasurementsReq(InitializedBaseStation& cbs)
{
    std::vector<uint8_t> buffer;
    cbs.channel.unpack(buffer);

    bool ok = false;

    std::vector<DB::MeasurementDescriptor> descriptors;

    {
        rapidjson::Document document;
        document.Parse(reinterpret_cast<const char*>(buffer.data()), buffer.size());
        if (document.GetParseError() != rapidjson::ParseErrorCode::kParseErrorNone)
        {
            s_logger.logCritical(QString("Cannot deserialize measurement request: %1").arg(rapidjson::GetParseError_En(document.GetParseError())));
            goto end;
        }
        if (!document.IsArray())
        {
            s_logger.logCritical(QString("Cannot deserialize measurement request: Bad document."));
            goto end;
        }

        descriptors.reserve(document.Size());

        for (size_t i = 0; i < document.Size(); i++)
        {
            rapidjson::Value const& valuej = document[i];
            if (!valuej.IsObject())
            {
                s_logger.logCritical(QString("Cannot deserialize report measurements response: Bad value."));
                goto end;
            }

            auto it = valuej.FindMember("sensor_id");
            if (it == valuej.MemberEnd() || !it->value.IsUint())
            {
                s_logger.logCritical(QString("Cannot deserialize measurement request: Missing sensor_id."));
                goto end;
            }
            DB::MeasurementDescriptor descriptor;
            descriptor.sensorId = it->value.GetUint();

            it = valuej.FindMember("index");
            if (it == valuej.MemberEnd() || !it->value.IsUint())
            {
                s_logger.logCritical(QString("Cannot deserialize measurement request: Missing index."));
                goto end;
            }
            descriptor.index = it->value.GetUint();

            it = valuej.FindMember("time_point");
            if (it == valuej.MemberEnd() || !it->value.IsInt64())
            {
                s_logger.logCritical(QString("Cannot deserialize measurement request: Missing time_point."));
                goto end;
            }
            descriptor.timePoint = DB::Clock::time_point(std::chrono::seconds(it->value.GetInt64()));

            it = valuej.FindMember("temperature");
            if (it == valuej.MemberEnd() || !it->value.IsNumber())
            {
                s_logger.logCritical(QString("Cannot deserialize measurement request: Missing temperature."));
                goto end;
            }
            descriptor.temperature = static_cast<float>(it->value.GetDouble());

            it = valuej.FindMember("humidity");
            if (it == valuej.MemberEnd() || !it->value.IsNumber())
            {
                s_logger.logCritical(QString("Cannot deserialize measurement request: Missing humidity."));
                goto end;
            }
            descriptor.humidity = static_cast<float>(it->value.GetDouble());

            it = valuej.FindMember("vcc");
            if (it == valuej.MemberEnd() || !it->value.IsNumber())
            {
                s_logger.logCritical(QString("Cannot deserialize measurement request: Missing vcc."));
                goto end;
            }
            descriptor.vcc = static_cast<float>(it->value.GetDouble());

            it = valuej.FindMember("b2s");
            if (it == valuej.MemberEnd() || !it->value.IsInt())
            {
                s_logger.logCritical(QString("Cannot deserialize measurement request: Missing b2s."));
                goto end;
            }
            descriptor.signalStrength.b2s = static_cast<int8_t>(it->value.GetInt());

            it = valuej.FindMember("s2b");
            if (it == valuej.MemberEnd() || !it->value.IsInt())
            {
                s_logger.logCritical(QString("Cannot deserialize measurement request: Missing s2b."));
                goto end;
            }
            descriptor.signalStrength.s2b = static_cast<int8_t>(it->value.GetInt());

            it = valuej.FindMember("sensor_errors");
            if (it == valuej.MemberEnd() || !it->value.IsUint())
            {
                s_logger.logCritical(QString("Cannot deserialize measurement request: Missing sensor_errors."));
                goto end;
            }
            descriptor.sensorErrors = static_cast<uint8_t>(it->value.GetUint());

            descriptors.push_back(descriptor);
        }

        if (!cbs.db.addMeasurements(descriptors))
        {
            s_logger.logCritical(QString("Cannot deserialize measurement request: Adding measurements failed."));
            goto end;
        }

        ok = true;
    }

end:
    {
        //DB& db = cbs.db;

        rapidjson::Document document;
        document.SetArray();

        for (DB::MeasurementDescriptor descriptor: descriptors)
        {
            rapidjson::Value mj;
            mj.SetObject();
            mj.AddMember("sensor_id", descriptor.sensorId, document.GetAllocator());
            mj.AddMember("index", descriptor.index, document.GetAllocator());
            mj.AddMember("ok", ok, document.GetAllocator());

            document.PushBack(std::move(mj), document.GetAllocator());
        }

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        document.Accept(writer);

        cbs.channel.send(data::Server_Message::REPORT_MEASUREMENTS_RES, buffer.GetString(), buffer.GetSize());
    }
}

//////////////////////////////////////////////////////////////////////////

void Comms::processReportSensorDetails(InitializedBaseStation& cbs)
{
    std::vector<uint8_t> buffer;
    cbs.channel.unpack(buffer);

    processSensorDetails(cbs, buffer);
}

//////////////////////////////////////////////////////////////////////////

void Comms::processSensorBoundReq(InitializedBaseStation& cbs)
{
    std::vector<uint8_t> buffer;
    cbs.channel.unpack(buffer);

    bool ok = false;

    {
        rapidjson::Document document;
        document.Parse(reinterpret_cast<const char*>(buffer.data()), buffer.size());
        if (document.GetParseError() != rapidjson::ParseErrorCode::kParseErrorNone)
        {
            s_logger.logCritical(QString("Cannot deserialize bind request: %1").arg(rapidjson::GetParseError_En(document.GetParseError())));
            goto end;
        }
        if (!document.IsObject())
        {
            s_logger.logCritical(QString("Cannot deserialize bind request: Bad document."));
            goto end;
        }

        auto it = document.FindMember("id");
        if (it == document.MemberEnd() || !it->value.IsUint())
        {
            s_logger.logCritical(QString("Cannot deserialize bind request: Missing id."));
            goto end;
        }
        DB::SensorId id = it->value.GetUint();

        it = document.FindMember("address");
        if (it == document.MemberEnd() || !it->value.IsUint())
        {
            s_logger.logCritical(QString("Cannot deserialize bind request: Missing address."));
            goto end;
        }
        DB::SensorAddress address = it->value.GetUint();

        it = document.FindMember("serial_number");
        if (it == document.MemberEnd() || !it->value.IsUint())
        {
            s_logger.logCritical(QString("Cannot deserialize bind request: Missing serial_number."));
            goto end;
        }
        uint32_t serialNumber = it->value.GetUint();

        DB::Sensor::Calibration calibration;
        it = document.FindMember("temperature_bias");
        if (it == document.MemberEnd() || !it->value.IsNumber())
        {
            s_logger.logCritical(QString("Cannot deserialize bind request: Missing temperature_bias."));
            goto end;
        }
        calibration.temperatureBias = static_cast<float>(it->value.GetDouble());

        it = document.FindMember("humidity_bias");
        if (it == document.MemberEnd() || !it->value.IsNumber())
        {
            s_logger.logCritical(QString("Cannot deserialize bind request: Missing humidity_bias."));
            goto end;
        }
        calibration.humidityBias = static_cast<float>(it->value.GetDouble());

        if (!cbs.db.bindSensor(id, address, serialNumber, calibration))
        {
            s_logger.logCritical(QString("Cannot deserialize bind request: Bind failed."));
            goto end;
        }

        ok = true;
        s_logger.logInfo(QString("Bind request successed."));
    }

end:
    cbs.channel.send(data::Server_Message::SENSOR_BOUND_RES, &ok, 1);
}

//////////////////////////////////////////////////////////////////////////

void Comms::process()
{
//    static DB::Clock::time_point lastFakeDiscovery = DB::Clock::now();
//    if (DB::Clock::now() - lastFakeDiscovery >= std::chrono::seconds(1))
//    {
//        lastFakeDiscovery = DB::Clock::now();

//        BaseStationDescriptor bs;
//        bs.mac = {0xB8, 0x27, 0xEB, 0xDA, 0x89, 0x1B };
//        {
//            auto it = std::find_if(m_discoveredBaseStations.begin(), m_discoveredBaseStations.end(), [&bs](Comms::BaseStationDescriptor const& _bs) { return _bs.mac == bs.mac; });
//            if (it == m_discoveredBaseStations.end())
//            {
//                m_discoveredBaseStations.push_back(bs);
//                emit baseStationDiscovered(bs);
//            }
//        }
//    }

    data::Server_Message message;
    for (std::unique_ptr<InitializedBaseStation>& cbs: m_initializedBaseStations)
    {
        while (cbs->channel.get_next_message(message))
        {
            switch (message)
            {
            case data::Server_Message::ADD_CONFIG_RES:
                processAddConfigRes(*cbs);
                break;
            case data::Server_Message::SET_CONFIGS_RES:
                processSetConfigsRes(*cbs);
                break;
            case data::Server_Message::SET_SENSORS_RES:
                processSetSensorsRes(*cbs);
                break;
            case data::Server_Message::ADD_SENSOR_RES:
                processAddSensorRes(*cbs);
                break;
            case data::Server_Message::REPORT_MEASUREMENTS_REQ:
                processReportMeasurementsReq(*cbs);
                break;
            case data::Server_Message::SENSOR_BOUND_REQ:
                processSensorBoundReq(*cbs);
                break;
            case data::Server_Message::REPORT_SENSORS_DETAILS:
                processReportSensorDetails(*cbs);
                break;
            default:
                s_logger.logCritical(QString("Invalid message received: %1").arg(int(message)));
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////////

