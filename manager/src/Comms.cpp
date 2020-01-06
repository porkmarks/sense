#include "Comms.h"
#include <cassert>
#include <iostream>

#include "Logger.h"
#include "Settings.h"

extern std::string getMacStr(DB::BaseStationDescriptor::Mac const& mac);
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
    //static_assert(std::is_standard_layout<T>::value, "Only PODs pls");
    pack(c, &t, sizeof(T), offset);
}

template<typename C>
bool unpack(C const& c, void* data, size_t size, size_t& offset, size_t maxSize)
{
    if (offset + size > c.size() || offset + size > maxSize)
    {
        return false;
    }
    memcpy(data, c.data() + offset, size);
    offset += size;
    return true;
}

template<typename C, typename T>
bool unpack(C const& c, T& t, size_t& offset, size_t maxSize)
{
    //static_assert(std::is_standard_layout<T>::value, "Only PODs pls");
    return unpack(c, &t, sizeof(T), offset, maxSize);
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
                    if (it != m_initializedBaseStations.end() && (*it)->isConnected == false && (*it)->isConnecting == false)
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
	{
		auto it = std::find_if(m_initializedBaseStations.begin(), m_initializedBaseStations.end(), [&mac](std::unique_ptr<InitializedBaseStation> const& _bs) { return _bs->descriptor.mac == mac; });
		if (it != m_initializedBaseStations.end())
		{
			s_logger.logWarning(QString("Attempting to connect to already connected BS %1.").arg(getMacStr(mac).c_str()));
			return false;
		}
	}

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

    cbsPtr->connections.push_back(connect(&db, &DB::sensorAdded, [this, cbsPtr](DB::SensorId id) { sensorAdded(*cbsPtr, id); }));
    cbsPtr->connections.push_back(connect(&db, &DB::sensorRemoved, [this, cbsPtr](DB::SensorId id) { sensorRemoved(*cbsPtr, id); }));

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
        cbs->lastTalkTP = DB::Clock::now(); //this represents communication so reset the pong

        emit baseStationConnected(cbs->descriptor);
        cbs->socketAdapter.start();

        if (cbs->db.findUnboundSensorIndex() >= 0)
        {
            changeToRadioState(*cbs, data::Radio_State::PAIRING);
        }
        else
        {
            changeToRadioState(*cbs, data::Radio_State::NORMAL);
        }
    }
}

//////////////////////////////////////////////////////////////////////////

void Comms::disconnectedFromBaseStation(InitializedBaseStation* cbs)
{
    if (cbs)
    {
        bool wasConnected = cbs->isConnected;
        cbs->isConnected = false;
        cbs->isConnecting = false;
        cbs->socketAdapter.getSocket().abort();
        //cbs->socketAdapter.getSocket().reset();

        if (wasConnected)
        {
            s_logger.logWarning(QString("Disconnected from BS %1").arg(getMacStr(cbs->descriptor.mac).c_str()));
            emit baseStationDisconnected(cbs->descriptor);
        }
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

void Comms::sendPing(Comms::InitializedBaseStation& cbs)
{
    std::array<uint8_t, 1024> buffer;
    size_t offset = 0;
    pack(buffer, ++cbs.lastPingId, offset);
    cbs.channel.send(data::Server_Message::PING, buffer.data(), offset);
    cbs.lastPingTP = DB::Clock::now();
}

//////////////////////////////////////////////////////////////////////////

template <typename T>
void Comms::sendSensorResponse(InitializedBaseStation& cbs, SensorRequest const& request, data::sensor::Type type, Radio::Address address, T const& payload)
{
    std::array<uint8_t, 1024> buffer;
    size_t offset = 0;
    pack(buffer, request.reqId, offset);
    pack(buffer, true, offset);
    pack(buffer, type, offset);
    pack(buffer, address, offset);
    pack(buffer, static_cast<uint32_t>(sizeof(T)), offset);
    pack(buffer, payload, offset);

    cbs.channel.send(data::Server_Message::SENSOR_RES, buffer.data(), offset);
}

//////////////////////////////////////////////////////////////////////////

void Comms::processPong(InitializedBaseStation& cbs)
{
    cbs.lastTalkTP = DB::Clock::now(); //this represents communication so reset the pong
    //std::cout << "PONG" << std::endl;
}

//////////////////////////////////////////////////////////////////////////

void Comms::sensorAdded(InitializedBaseStation& cbs, DB::SensorId id)
{
    changeToRadioState(cbs, data::Radio_State::PAIRING);
}

//////////////////////////////////////////////////////////////////////////

void Comms::sensorRemoved(InitializedBaseStation& cbs, DB::SensorId id)
{
    int32_t unboundIndex = cbs.db.findUnboundSensorIndex();
    if (unboundIndex == cbs.db.findSensorIndexById(id) || unboundIndex < 0)
    {
        changeToRadioState(cbs, data::Radio_State::NORMAL);
    }
}

//////////////////////////////////////////////////////////////////////////

void Comms::changeToRadioState(InitializedBaseStation& cbs, data::Radio_State state)
{
    std::array<uint8_t, 1024> buffer;
    size_t offset = 0;
    pack(buffer, state, offset);
    cbs.channel.send(data::Server_Message::CHANGE_RADIO_STATE, buffer.data(), offset);
}

//////////////////////////////////////////////////////////////////////////

void Comms::processSensorReq(InitializedBaseStation& cbs)
{
    Clock::time_point startTp = Clock::now();

    std::array<uint8_t, 1024> buffer;
    size_t maxSize = 0;
    bool ok = cbs.channel.unpack_fixed(buffer, maxSize) == Channel::Unpack_Result::OK;

    size_t offset = 0;
    SensorRequest request;
    uint32_t payload_size = 0;
    ok &= unpack(buffer, request.reqId, offset, maxSize);
    ok &= unpack(buffer, request.signalS2B, offset, maxSize);
    ok &= unpack(buffer, request.type, offset, maxSize);
    ok &= unpack(buffer, request.address, offset, maxSize);
    ok &= unpack(buffer, payload_size, offset, maxSize);
    ok &= payload_size < 1024 * 1024;
    if (ok)
    {
        request.payload.resize(payload_size);
        ok &= unpack(buffer, request.payload.data(), payload_size, offset, maxSize);
    }

    if (!ok)
    {
        std::cerr << "Malformed message" << std::endl;
        return;
    }

    processSensorReq(cbs, request);

    std::cout << "Duration: " << std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - startTp).count() << std::endl;
}

//////////////////////////////////////////////////////////////////////////

void Comms::processRevertedRadioStateToNormal(InitializedBaseStation& cbs)
{
	Clock::time_point startTp = Clock::now();

    int32_t index = cbs.db.findUnboundSensorIndex();
    if (index < 0)
    {
        s_logger.logWarning(QString("Unexpected state reversal (no unbound sensor)"));
    }
    else
    {
        cbs.db.removeSensor((size_t)index);
    }

	std::cout << "Duration: " << std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - startTp).count() << std::endl;
}

//////////////////////////////////////////////////////////////////////////

void Comms::processSensorReq(InitializedBaseStation& cbs, SensorRequest const& request)
{
    switch (static_cast<data::sensor::Type>(request.type))
    {
    case data::sensor::Type::MEASUREMENT_BATCH_REQUEST:
        if (request.payload.size() == sizeof(data::sensor::Measurement_Batch_Request))
        {
            processSensorReq_MeasurementBatch(cbs, request, *reinterpret_cast<data::sensor::Measurement_Batch_Request const*>(request.payload.data()));
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


static void fillConfig(data::sensor::Config_Response& config, DB::SensorSettings const& sensorSettings, DB::Sensor const& sensor, DB::SensorOutputDetails const& sensorOutputDetails, DB::Sensor::Calibration const& reportedCalibration)
{
    DB::Clock::time_point now = DB::Clock::now();

    config.next_measurement_delay =
            chrono::seconds(std::chrono::duration_cast<std::chrono::seconds>(sensorOutputDetails.nextMeasurementTimePoint - now).count());

    config.measurement_period =
            chrono::seconds(std::chrono::duration_cast<std::chrono::seconds>(sensorOutputDetails.measurementPeriod).count());

    config.next_comms_delay =
            chrono::seconds(std::chrono::duration_cast<std::chrono::seconds>(sensorOutputDetails.nextCommsTimePoint - now).count());

    config.comms_period =
            chrono::seconds(std::chrono::duration_cast<std::chrono::seconds>(sensorOutputDetails.commsPeriod).count());

    config.last_confirmed_measurement_index = sensor.lastConfirmedMeasurementIndex;
    config.calibration.temperature_bias = static_cast<int16_t>((sensor.calibration.temperatureBias) * 100.f);
    config.calibration.humidity_bias = static_cast<int16_t>((sensor.calibration.humidityBias) * 100.f);
    config.power = sensorSettings.radioPower;
    config.sleeping = sensor.shouldSleep;
}

//////////////////////////////////////////////////////////////////////////

void Comms::processSensorReq_MeasurementBatch(InitializedBaseStation& cbs, SensorRequest const& request, data::sensor::Measurement_Batch_Request const& measurementBatch)
{
    int32_t sensorIndex = cbs.db.findSensorIndexByAddress(request.address);
    if (sensorIndex < 0)
    {
        std::cerr << "Invalid sensor address: " << request.address << std::endl;
        return;
    }

    DB::Sensor const& sensor = cbs.db.getSensor(static_cast<size_t>(sensorIndex));

    std::vector<DB::MeasurementDescriptor> measurements;
    uint32_t count = std::min<uint32_t>(measurementBatch.count, data::sensor::Measurement_Batch_Request::MAX_COUNT);
    measurements.reserve(count);

    for (uint32_t i = 0; i < count; i++)
    {
        data::sensor::Measurement const& m = measurementBatch.measurements[i];
        DB::MeasurementDescriptor d;
        d.index = measurementBatch.start_index + i;
//        if (m.flags & static_cast<int>(data::sensor::Measurement::Flag::COMMS_ERROR))
//        {
//            d.sensorErrors |= DB::SensorErrors::Comms;
//        }
//        else if (m.flags > 0)// & static_cast<int>(data::sensor::Measurement::Flag::SENSOR_ERROR))
//        {
//            d.sensorErrors |= DB::SensorErrors::Hardware;
//        }
        d.sensorId = sensor.id;
        d.signalStrength.b2s = sensor.lastSignalStrengthB2S;
        d.signalStrength.s2b = request.signalS2B;
        d.vcc = data::sensor::unpack_qvcc(measurementBatch.qvcc);
        m.unpack(d.humidity, d.temperature);
        measurements.push_back(d);
    }

    cbs.db.addMeasurements(std::move(measurements));
}

//////////////////////////////////////////////////////////////////////////

DB::SensorInputDetails Comms::createSensorInputDetails(DB::Sensor const& sensor, data::sensor::Config_Request const& configRequest) const
{
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

    details.hasMeasurement = true;
    configRequest.measurement.unpack(details.measurementHumidity, details.measurementTemperature);
    details.measurementVcc = data::sensor::unpack_qvcc(configRequest.qvcc);

    details.hasErrorCountersDelta = true;
    details.errorCountersDelta.comms = configRequest.comms_errors;
    details.errorCountersDelta.resetReboots = (configRequest.reboot_flags & int(data::sensor::Reboot_Flag::REBOOT_RESET)) ? 1 : 0;
    details.errorCountersDelta.unknownReboots = (configRequest.reboot_flags & int(data::sensor::Reboot_Flag::REBOOT_UNKNOWN)) ? 1 : 0;
    details.errorCountersDelta.brownoutReboots = (configRequest.reboot_flags & int(data::sensor::Reboot_Flag::REBOOT_BROWNOUT)) ? 1 : 0;
    details.errorCountersDelta.powerOnReboots = (configRequest.reboot_flags & int(data::sensor::Reboot_Flag::REBOOT_POWER_ON)) ? 1 : 0;
    details.errorCountersDelta.watchdogReboots = (configRequest.reboot_flags & int(data::sensor::Reboot_Flag::REBOOT_WATCHDOG)) ? 1 : 0;

    return details;
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

    DB::SensorInputDetails details = createSensorInputDetails(sensor, configRequest);
    cbs.db.setSensorInputDetails(details);

	DB::SensorSettings const& sensorSettings = cbs.db.getSensorSettings();
    DB::SensorOutputDetails outputDetails = cbs.db.computeSensorOutputDetails(sensor.id);
    DB::Sensor::Calibration reportedCalibration{ static_cast<float>(configRequest.calibration.temperature_bias) / 100.f,
                static_cast<float>(configRequest.calibration.humidity_bias) / 100.f};

    data::sensor::Config_Response response;
	fillConfig(response, sensorSettings, sensor, outputDetails, reportedCalibration);

    sendSensorResponse(cbs, request, data::sensor::Type::CONFIG_RESPONSE, request.address, response);

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

    data::sensor::First_Config_Response response;

    {
        DB::SensorOutputDetails outputDetails = cbs.db.computeSensorOutputDetails(sensor.id);
        response.first_measurement_index = outputDetails.nextRealTimeMeasurementIndex; //since this sensor just booted, it cannot have measurements older than now
    }

    DB::SensorInputDetails details = createSensorInputDetails(sensor, firstConfigRequest);
	details.hasDeviceInfo = true;
    details.deviceInfo.sensorType = firstConfigRequest.descriptor.sensor_type;
    details.deviceInfo.hardwareVersion = firstConfigRequest.descriptor.hardware_version;
    details.deviceInfo.softwareVersion = firstConfigRequest.descriptor.software_version;

    //being the first request, we know the sensor doesn't store anything, so reset the firstStoredMeasurementIndex to the real time one
    details.hasStoredData = true;
    details.firstStoredMeasurementIndex = response.first_measurement_index;
    details.storedMeasurementCount = 0;

    cbs.db.setSensorInputDetails(details);

    {
		DB::SensorSettings const& sensorSettings = cbs.db.getSensorSettings();
        DB::SensorOutputDetails outputDetails = cbs.db.computeSensorOutputDetails(sensor.id);
		fillConfig(response, sensorSettings, sensor, outputDetails, sensor.calibration);
    }

    sendSensorResponse(cbs, request, data::sensor::Type::FIRST_CONFIG_RESPONSE, request.address, response);

    std::cout << "Received First Config Request" << std::endl;
}

//////////////////////////////////////////////////////////////////////////

void Comms::processSensorReq_PairRequest(InitializedBaseStation& cbs, SensorRequest const& request, data::sensor::Pair_Request const& pairRequest)
{
    DB::Sensor::Calibration calibration;
    calibration.temperatureBias = static_cast<float>(pairRequest.calibration.temperature_bias) / 100.f;
    calibration.humidityBias = static_cast<float>(pairRequest.calibration.humidity_bias) / 100.f;

    Result<DB::SensorId> result = cbs.db.bindSensor(pairRequest.descriptor.serial_number,
                                                    pairRequest.descriptor.sensor_type,
                                                    pairRequest.descriptor.hardware_version,
                                                    pairRequest.descriptor.software_version,
                                                    calibration);
    if (result != success)
    {
        s_logger.logWarning(QString("Unexpected bind request received from sensor SN %1: %2").arg(pairRequest.descriptor.serial_number, 8, 18, QChar('0')).arg(result.error().what().c_str()));
        sendEmptySensorResponse(cbs, request);
        return;
    }

    DB::SensorId id = result.payload();
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
    sendSensorResponse(cbs, request, data::sensor::Type::PAIR_RESPONSE, request.address, response);

    //switch back to normal state
    changeToRadioState(cbs, data::Radio_State::NORMAL);

    std::cout << "Received Pair Request" << std::endl;
}

//////////////////////////////////////////////////////////////////////////

size_t Comms::getDiscoveredBaseStationCount() const
{
    return m_discoveredBaseStations.size();
}

//////////////////////////////////////////////////////////////////////////

const Comms::BaseStationDescriptor& Comms::getDiscoveredBaseStation(size_t index) const
{
    return m_discoveredBaseStations[index];
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
        if (!cbs->isConnected)
        {
            continue;
        }

        if (DB::Clock::now() - cbs->lastPingTP > std::chrono::seconds(2))
        {
            sendPing(*cbs);
        }
        if (DB::Clock::now() - cbs->lastTalkTP > std::chrono::seconds(10))
        {
           disconnectedFromBaseStation(cbs.get());
           continue;
        }

        while (cbs->channel.get_next_message(message))
        {
            cbs->lastTalkTP = DB::Clock::now(); //this represents communication so reset the pong
            switch (message)
            {
            case data::Server_Message::PONG:
                processPong(*cbs);
                break;
            case data::Server_Message::SENSOR_REQ:
                processSensorReq(*cbs);
                break;
			case data::Server_Message::REVERTED_RADIO_STATE_TO_NORMAL:
				processRevertedRadioStateToNormal(*cbs);
				break;
            default:
                s_logger.logCritical(QString("Invalid message received: %1").arg(int(message)));
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////////

