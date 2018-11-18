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


template<typename C>
void pack(C& c, void const* data, size_t size, size_t& offset)
{
    if (offset + size > c.size())
    {
        assert(false);
        return;
        //c.resize(offset + size);
    }
    memcpy(c.data() + offset, data, size);
    offset += size;
}

template<typename C, typename T>
void pack(C& c, T const& t, size_t& offset)
{
    static_assert(std::is_standard_layout<T>::value, "Only PODs pls");
    pack(c, &t, sizeof(T), offset);
}

template<typename C>
bool unpack(C const& c, void* data, size_t size, size_t& offset, size_t max_size)
{
    if (offset + size > c.size() || offset + size > max_size)
    {
        return false;
    }
    memcpy(data, c.data() + offset, size);
    offset += size;
    return true;
}

template<typename C, typename T>
bool unpack(C const& c, T& t, size_t& offset, size_t max_size)
{
    static_assert(std::is_standard_layout<T>::value, "Only PODs pls");
    return unpack(c, &t, sizeof(T), offset, max_size);
}

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

void Comms::sendEmptySensorResponse(InitializedBaseStation& cbs, SensorRequest const& request)
{
    std::array<uint8_t, 1024> buffer;
    size_t offset = 0;
    pack(buffer, request.reqId, offset);
    pack(buffer, false, offset);

    cbs.channel.send(data::Server_Message::SENSOR_RES, buffer.data(), offset);
}

//////////////////////////////////////////////////////////////////////////

template <typename T>
void Comms::sendSensorResponse(InitializedBaseStation& cbs, SensorRequest const& request, data::sensor::Type type, uint32_t address, uint8_t retries, T const& payload)
{
    std::array<uint8_t, 1024> buffer;
    size_t offset = 0;
    pack(buffer, request.reqId, offset);
    pack(buffer, true, offset);
    pack(buffer, type, offset);
    pack(buffer, address, offset);
    pack(buffer, retries, offset);
    pack(buffer, static_cast<uint32_t>(sizeof(payload)), offset);
    pack(buffer, payload, offset);

    cbs.channel.send(data::Server_Message::SENSOR_RES, buffer.data(), offset);
}

//////////////////////////////////////////////////////////////////////////

void Comms::processPong(InitializedBaseStation& cbs)
{
    std::cout << "PONG" << std::endl;
}

//////////////////////////////////////////////////////////////////////////

void Comms::processSensorReq(InitializedBaseStation& cbs)
{
    Clock::time_point start_tp = Clock::now();

    std::array<uint8_t, 1024> buffer;
    size_t max_size = 0;
    bool ok = cbs.channel.unpack_fixed(buffer, max_size) == Channel::Unpack_Result::OK;

    size_t offset = 0;
    SensorRequest request;
    uint32_t payload_size = 0;
    ok &= unpack(buffer, request.reqId, offset, max_size);
    ok &= unpack(buffer, request.signalS2B, offset, max_size);
    ok &= unpack(buffer, request.type, offset, max_size);
    ok &= unpack(buffer, request.address, offset, max_size);
    ok &= unpack(buffer, payload_size, offset, max_size);
    ok &= payload_size < 1024 * 1024;
    if (ok)
    {
        request.payload.resize(payload_size);
        ok &= unpack(buffer, request.payload.data(), payload_size, offset, max_size);
    }

    if (!ok)
    {
        std::cerr << "Malformed message" << std::endl;
        return;
    }

    processSensorReq(cbs, request);

    std::cout << "Duration: " << std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - start_tp).count() << std::endl;
}

//////////////////////////////////////////////////////////////////////////

void Comms::processSensorReq(InitializedBaseStation& cbs, SensorRequest const& request)
{
    switch (static_cast<data::sensor::Type>(request.type))
    {
    case data::sensor::Type::MEASUREMENT_BATCH:
        if (request.payload.size() == sizeof(data::sensor::Measurement_Batch))
        {
            processSensorReq_MeasurementBatch(cbs, request, *reinterpret_cast<data::sensor::Measurement_Batch const*>(request.payload.data()));
        }
        break;
    case data::sensor::Type::CONFIG_REQUEST:
        if (request.payload.size() == sizeof(data::sensor::Config_Request))
        {
            processSensorReq_ConfigRequest(cbs, request, *reinterpret_cast<data::sensor::Config_Request const*>(request.payload.data()));
        }
        break;
    case data::sensor::Type::FIRST_CONFIG_REQUEST:
        if (request.payload.size() == sizeof(data::sensor::First_Config_Request))
        {
            processSensorReq_FirstConfigRequest(cbs, request, *reinterpret_cast<data::sensor::First_Config_Request const*>(request.payload.data()));
        }
        break;
    case data::sensor::Type::PAIR_REQUEST:
        if (request.payload.size() == sizeof(data::sensor::Pair_Request))
        {
            processSensorReq_PairRequest(cbs, request, *reinterpret_cast<data::sensor::Pair_Request const*>(request.payload.data()));
        }
        break;
    default:
        std::cerr << "Invalid sensor request: " << int(request.type) << std::endl;
        s_logger.logCritical(QString("Invalid sensor request: %1").arg(request.type));
        break;
    }
}


static void fillConfig(data::sensor::Config& config, DB::SensorsConfig const& sensorsConfig, DB::Sensor const& sensor, DB::SensorOutputDetails const& sensorOutputDetails, DB::Sensor::Calibration const& reportedCalibration)
{
    DB::Clock::time_point now = DB::Clock::now();

    config.baseline_measurement_index = sensorOutputDetails.baselineMeasurementIndex;
    config.next_measurement_delay =
            chrono::seconds(std::chrono::duration_cast<std::chrono::seconds>(sensorOutputDetails.nextMeasurementTimePoint - now).count());

    config.measurement_period =
            chrono::seconds(std::chrono::duration_cast<std::chrono::seconds>(sensorOutputDetails.measurementPeriod).count());

    config.next_comms_delay =
            chrono::seconds(std::chrono::duration_cast<std::chrono::seconds>(sensorOutputDetails.nextCommsTimePoint - now).count());

    config.comms_period =
            chrono::seconds(std::chrono::duration_cast<std::chrono::seconds>(sensorOutputDetails.commsPeriod).count());

    config.last_confirmed_measurement_index = sensor.lastConfirmedMeasurementIndex;
    config.calibration_change.temperature_bias = static_cast<int16_t>((sensor.calibration.temperatureBias - reportedCalibration.temperatureBias) * 100.f);
    config.calibration_change.humidity_bias = static_cast<int16_t>((sensor.calibration.humidityBias - reportedCalibration.humidityBias) * 100.f);
    config.power = sensorsConfig.descriptor.sensorsPower;
}

//////////////////////////////////////////////////////////////////////////

void Comms::processSensorReq_MeasurementBatch(InitializedBaseStation& cbs, SensorRequest const& request, data::sensor::Measurement_Batch const& measurementBatch)
{
    int32_t sensorIndex = cbs.db.findSensorIndexByAddress(request.address);
    if (sensorIndex < 0)
    {
        std::cerr << "Invalid sensor address: " << request.address << std::endl;
        sendEmptySensorResponse(cbs, request);
        return;
    }

    DB::Sensor const& sensor = cbs.db.getSensor(static_cast<size_t>(sensorIndex));

    std::vector<DB::MeasurementDescriptor> measurements;
    uint32_t count = std::min<uint32_t>(measurementBatch.count, data::sensor::Measurement_Batch::MAX_COUNT);
    measurements.reserve(count);

    for (uint32_t i = 0; i < count; i++)
    {
        data::sensor::Measurement const& m = measurementBatch.measurements[i];
        DB::MeasurementDescriptor d;
        d.index = measurementBatch.start_index + i;
        if (m.flags & static_cast<int>(data::sensor::Measurement::Flag::COMMS_ERROR))
        {
            d.sensorErrors |= DB::SensorErrors::Comms;
        }
        else if (m.flags > 0)// & static_cast<int>(data::sensor::Measurement::Flag::SENSOR_ERROR))
        {
            d.sensorErrors |= DB::SensorErrors::Hardware;
        }
        d.sensorId = sensor.id;
        d.signalStrength.b2s = sensor.lastSignalStrengthB2S;
        d.signalStrength.s2b = request.signalS2B;
        measurementBatch.unpack(d.vcc);
        m.unpack(d.humidity, d.temperature);
        measurements.push_back(d);
    }
    sendEmptySensorResponse(cbs, request);

    cbs.db.addMeasurements(measurements);
}

//////////////////////////////////////////////////////////////////////////

void Comms::processSensorReq_ConfigRequest(InitializedBaseStation& cbs, SensorRequest const& request, data::sensor::Config_Request const& configRequest)
{
    int32_t sensorIndex = cbs.db.findSensorIndexByAddress(request.address);
    if (sensorIndex < 0)
    {
        std::cerr << "Invalid sensor address: " << request.address << std::endl;
        sendEmptySensorResponse(cbs, request);
        return;
    }

    DB::Sensor const& sensor = cbs.db.getSensor(static_cast<size_t>(sensorIndex));

    DB::SensorInputDetails details;
    details.id = sensor.id;
    details.hasStoredData = true;
    details.firstStoredMeasurementIndex = configRequest.first_measurement_index;
    details.storedMeasurementCount = configRequest.measurement_count;
    details.hasSignalStrength = true;
    details.signalStrengthB2S = configRequest.b2s_input_dBm;
    details.hasSleepingData = true;
    details.sleeping = configRequest.sleeping;
    details.hasLastCommsTimePoint = true;
    details.lastCommsTimePoint = Clock::now();

    cbs.db.setSensorInputDetails(details);

    DB::SensorsConfig const& sensorsConfig = cbs.db.getLastSensorsConfig();
    DB::SensorOutputDetails outputDetails = cbs.db.computeSensorOutputDetails(sensor.id);
    DB::Sensor::Calibration reportedCalibration{ static_cast<float>(configRequest.calibration.temperature_bias) / 100.f,
                static_cast<float>(configRequest.calibration.humidity_bias) / 100.f};

    data::sensor::Config config;
    fillConfig(config, sensorsConfig, sensor, outputDetails, reportedCalibration);

    sendSensorResponse(cbs, request, data::sensor::Type::CONFIG, request.address, 1, config);

    std::cout << "Received Config Request" << std::endl;
}

//////////////////////////////////////////////////////////////////////////

void Comms::processSensorReq_FirstConfigRequest(InitializedBaseStation& cbs, SensorRequest const& request, data::sensor::First_Config_Request const& firstConfigRequest)
{
    int32_t sensorIndex = cbs.db.findSensorIndexByAddress(request.address);
    if (sensorIndex < 0)
    {
        std::cerr << "Invalid sensor address: " << request.address << std::endl;
        sendEmptySensorResponse(cbs, request);
        return;
    }

    DB::Sensor const& sensor = cbs.db.getSensor(static_cast<size_t>(sensorIndex));

    data::sensor::First_Config firstConfig;

    {
        DB::SensorOutputDetails outputDetails = cbs.db.computeSensorOutputDetails(sensor.id);
        firstConfig.first_measurement_index = outputDetails.nextRealTimeMeasurementIndex; //since this sensor just booted, it cannot have measurements older than now
    }

    DB::SensorInputDetails details;
    details.id = sensor.id;
    details.hasStoredData = true;
    details.firstStoredMeasurementIndex = firstConfig.first_measurement_index;
    details.storedMeasurementCount = 0;

    cbs.db.setSensorInputDetails(details);

    {
        DB::SensorsConfig const& sensorsConfig = cbs.db.getLastSensorsConfig();
        DB::SensorOutputDetails outputDetails = cbs.db.computeSensorOutputDetails(sensor.id);
        fillConfig(firstConfig.config, sensorsConfig, sensor, outputDetails, sensor.calibration);
    }

    sendSensorResponse(cbs, request, data::sensor::Type::FIRST_CONFIG, request.address, 2, firstConfig);

    std::cout << "Received First Config Request" << std::endl;
}

//////////////////////////////////////////////////////////////////////////

void Comms::processSensorReq_PairRequest(InitializedBaseStation& cbs, SensorRequest const& request, data::sensor::Pair_Request const& pairRequest)
{
    DB::Sensor::Calibration calibration;
    calibration.temperatureBias = static_cast<float>(pairRequest.calibration.temperature_bias) / 100.f;
    calibration.humidityBias = static_cast<float>(pairRequest.calibration.humidity_bias) / 100.f;
    uint32_t serialNumber = pairRequest.descriptor.serial_number;

    DB::SensorId id;
    if (!cbs.db.bindSensor(serialNumber, calibration, id))
    {
        s_logger.logWarning(QString("Unexpected bind request received from sensor SN %1").arg(serialNumber, 8, 18, QChar('0')));
        sendEmptySensorResponse(cbs, request);
        return;
    }

    int32_t sensorIndex = cbs.db.findSensorIndexById(id);
    if (sensorIndex < 0)
    {
        assert(false);
        sendEmptySensorResponse(cbs, request);
        return;
    }

    DB::Sensor const& sensor = cbs.db.getSensor(static_cast<size_t>(sensorIndex));

    data::sensor::Pair_Response response;
    response.address = sensor.address;
    sendSensorResponse(cbs, request, data::sensor::Type::PAIR_RESPONSE, request.address, 10, response);

    std::cout << "Received Pair Request" << std::endl;
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
            case data::Server_Message::PONG:
                processPong(*cbs);
                break;
            case data::Server_Message::SENSOR_REQ:
                processSensorReq(*cbs);
                break;
            default:
                s_logger.logCritical(QString("Invalid message received: %1").arg(int(message)));
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////////

