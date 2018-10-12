#include "Server.h"

#include <iostream>
#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/writer.h"
#include "rapidjson/reader.h"
#include "rapidjson/stringbuffer.h"

#include "Log.h"

Server::Server(Sensors& sensors)
    : m_sensors(sensors)
    , m_socket(m_io_service)
    , m_socket_adapter(m_socket)
    , m_channel(m_socket_adapter)
    , m_broadcast_socket(m_io_service)
{
    m_sensors.cb_report_measurements = std::bind(&Server::report_measurements, this, std::placeholders::_1);
    m_sensors.cb_sensor_bound = std::bind(&Server::sensor_bound, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4);
    m_sensors.cb_sensor_details_changed = std::bind(&Server::sensor_details_changed, this, std::placeholders::_1);
}

///////////////////////////////////////////////////////////////////////////////////////////

Server::~Server()
{
    m_exit = true;
    m_io_service.stop();
    m_io_service_work.reset();

    if (m_io_service_thread.joinable())
    {
        m_io_service_thread.join();
    }
    if (m_broadcast_thread.joinable())
    {
        m_broadcast_thread.join();
    }
}

///////////////////////////////////////////////////////////////////////////////////////////

bool Server::init(uint16_t port, uint16_t broadcast_port)
{
    if (!m_sensors.init())
    {
        LOGE << "Sensors not initialized. Functionality will be limited" << std::endl;
    }

    {
        struct ifreq ifr;
        struct ifconf ifc;
        char buf[1024];
        int success = 0;

        int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
        if (sock == -1)
        {
            LOGE << "Cannot get MAC address: socket failed" << std::endl;
            return -1;
        };

        ifc.ifc_len = sizeof(buf);
        ifc.ifc_buf = buf;
        if (ioctl(sock, SIOCGIFCONF, &ifc) == -1)
        {
            close(sock);
            LOGE << "Cannot get MAC address: ioctl SIOCGIFCONF failed" << std::endl;
            return -1;
        }

        struct ifreq* it = ifc.ifc_req;
        const struct ifreq* const end = it + (static_cast<size_t>(ifc.ifc_len) / sizeof(struct ifreq));

        for (; it != end; ++it)
        {
            strcpy(ifr.ifr_name, it->ifr_name);
            if (ioctl(sock, SIOCGIFFLAGS, &ifr) == 0)
            {
                if (! (ifr.ifr_flags & IFF_LOOPBACK))
                { // don't count loopback
                    if (ioctl(sock, SIOCGIFHWADDR, &ifr) == 0)
                    {
                        success = 1;
                        break;
                    }
                }
            }
            else
            {
                close(sock);
                LOGE << "Cannot get MAC address: ioctl SIOCGIFFLAGS failed" << std::endl;
                return -1;
            }
        }

        close(sock);

        if (success)
        {
            memcpy(m_mac_address, ifr.ifr_hwaddr.sa_data, 6);
            printf("MAC address: %X:%X:%X:%X:%X:%X\n", m_mac_address[0]&0xFF, m_mac_address[1]&0xFF, m_mac_address[2]&0xFF,
                    m_mac_address[3]&0xFF, m_mac_address[4]&0xFF, m_mac_address[5]&0xFF);
        }
        else
        {
            LOGE << "Cannot get MAC address." << std::endl;
            return false;
        }
    }

    m_port = port;
    m_broadcast_port = broadcast_port;
    try
    {
        m_io_service_work.reset(new asio::io_service::work(m_io_service));
        m_io_service_thread = std::thread([this]()
        {
            LOGI << "Started io service thread." << std::endl;
            m_io_service.run();
            LOGI << "Stopping io service thread." << std::endl;
        });
        m_broadcast_thread = std::thread([this]
        {
            LOGI << "Started broadcast thread." << std::endl;
            broadcast_thread_func();
            LOGI << "Stopping broadcast thread." << std::endl;
        });

        m_acceptor.reset(new asio::ip::tcp::acceptor(m_io_service, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port)));

        start_accept();
    }
    catch (std::exception const& e)
    {
        LOGE << "Init error: " << e.what() << std::endl;
        return false;
    }

    LOGI << "Server initialized!" << std::endl;

    return true;
}

///////////////////////////////////////////////////////////////////////////////////////////

void Server::start_accept()
{
    LOGI << "Started accepting connections." << std::endl;
    try
    {
        m_is_accepting = true;
        m_socket.close();
        m_acceptor->async_accept(m_socket, std::bind(&Server::accept_func, this, std::placeholders::_1));
    }
    catch (std::exception const& e)
    {
        LOGE << "Start accept error: " << e.what() << std::endl;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////

void Server::accept_func(asio::error_code ec)
{
    if (ec)
    {
        LOGE << "Accept error: " << ec.message() << std::endl;
        start_accept();
    }
    else
    {
        LOGI << "Connected!" << std::endl;
        m_is_accepting = false;
        m_is_connected = true;
        m_socket_adapter.start();
    }
}

///////////////////////////////////////////////////////////////////////////////////////////

void Server::init_broadcast()
{
    try
    {
        if (m_broadcast_socket.is_open())
        {
            m_broadcast_socket.close();
        }

        m_broadcast_socket.open(asio::ip::udp::v4());
        m_broadcast_socket.set_option(asio::ip::udp::socket::reuse_address(true));
        m_broadcast_socket.set_option(asio::socket_base::broadcast(true));
        m_broadcast_endpoint = asio::ip::udp::endpoint(asio::ip::address_v4::broadcast(), m_broadcast_port);
    }
    catch (std::exception const& e)
    {
        LOGE << "Broadcast init error: " << e.what() << std::endl;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////

void Server::broadcast_thread_func()
{
    while (!m_exit)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        try
        {
            if (m_broadcast_socket.is_open())
            {
                m_broadcast_socket.send_to(asio::buffer(m_mac_address, 6), m_broadcast_endpoint);
            }
            else
            {
                init_broadcast();
            }
        }
        catch (std::exception const& e)
        {
            LOGE << "Broadcast error: " << e.what() << ". Reinitializing..." << std::endl;
            init_broadcast();
        }
    }

    try
    {
        m_broadcast_socket.close();
    }
    catch (std::exception const& e)
    {
        LOGE << "Broadcast close error: " << e.what() << std::endl;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////

std::string Server::compute_sensor_details_response() const
{
    rapidjson::Document document;
    document.SetObject();


    rapidjson::Value sensorArrayj;
    {
        sensorArrayj.SetArray();
        for (Sensors::Sensor const& sensor: m_sensors.get_sensors())
        {
            rapidjson::Value sensorj;
            sensorj.SetObject();
            sensorj.AddMember("id", sensor.id, document.GetAllocator());
            sensorj.AddMember("serial_number", sensor.serial_number, document.GetAllocator());
            sensorj.AddMember("b2s", static_cast<int>(sensor.b2s_input_dBm), document.GetAllocator());
            sensorj.AddMember("first_recorded_measurement_index", sensor.first_recorded_measurement_index, document.GetAllocator());
            sensorj.AddMember("max_confirmed_measurement_index", sensor.max_confirmed_measurement_index, document.GetAllocator());
            sensorj.AddMember("humidity_bias", sensor.calibration.humidity_bias, document.GetAllocator());
            sensorj.AddMember("temperature_bias", sensor.calibration.temperature_bias, document.GetAllocator());
            sensorj.AddMember("recorded_measurement_count", sensor.recorded_measurement_count, document.GetAllocator());

            int32_t dt = static_cast<int32_t>(std::chrono::duration_cast<std::chrono::seconds>(m_sensors.compute_next_measurement_time_point(sensor.id) - Clock::now()).count());
            sensorj.AddMember("next_measurement_dt", dt, document.GetAllocator());
            sensorj.AddMember("last_comms_tp", static_cast<uint64_t>(Clock::to_time_t(m_sensors.get_sensor_last_comms_time_point(sensor.id))), document.GetAllocator());

            sensorj.AddMember("sleeping", sensor.sleeping, document.GetAllocator());

            sensorArrayj.PushBack(std::move(sensorj), document.GetAllocator());
        }
    }
    document.AddMember("sensors", std::move(sensorArrayj), document.GetAllocator());

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    document.Accept(writer);

    return std::string(buffer.GetString(), buffer.GetSize());
}

///////////////////////////////////////////////////////////////////////////////////////////

std::string Server::compute_configs_response() const
{
    rapidjson::Document document;
    document.SetObject();

    rapidjson::Value configArrayj;
    {
        configArrayj.SetArray();
        for (Sensors::Config const& config: m_sensors.get_configs())
        {
            rapidjson::Value configj;
            configj.SetObject();
            configj.AddMember("name", rapidjson::Value(config.name.c_str(), document.GetAllocator()), document.GetAllocator());
            configj.AddMember("sensors_sleeping", config.sensors_sleeping, document.GetAllocator());
            configj.AddMember("sensors_power", static_cast<uint32_t>(config.sensors_power), document.GetAllocator());
            configj.AddMember("measurement_period", static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(config.measurement_period).count()), document.GetAllocator());
            configj.AddMember("comms_period", static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(config.comms_period).count()), document.GetAllocator());
            configj.AddMember("baseline_measurement_tp", static_cast<uint64_t>(Clock::to_time_t(config.baseline_measurement_time_point)), document.GetAllocator());
            configj.AddMember("baseline_measurement_index", config.baseline_measurement_index, document.GetAllocator());

            configArrayj.PushBack(std::move(configj), document.GetAllocator());
        }
    }
    document.AddMember("configs", std::move(configArrayj), document.GetAllocator());

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    document.Accept(writer);

    return std::string(buffer.GetString(), buffer.GetSize());
}

///////////////////////////////////////////////////////////////////////////////////////////

void Server::report_measurements(std::vector<Sensors::Reported_Measurement> const& measurements)
{
    rapidjson::Document document;
    document.SetArray();
    for (Sensors::Reported_Measurement const& m: measurements)
    {
        rapidjson::Value mj;
        mj.SetObject();
        mj.AddMember("sensor_id", m.sensor_id, document.GetAllocator());
        mj.AddMember("index", m.measurement.index, document.GetAllocator());
        mj.AddMember("time_point", static_cast<int64_t>(Clock::to_time_t(m.time_point)), document.GetAllocator());
        mj.AddMember("temperature", m.measurement.temperature, document.GetAllocator());
        mj.AddMember("humidity", m.measurement.humidity, document.GetAllocator());
        mj.AddMember("vcc", m.measurement.vcc, document.GetAllocator());
        mj.AddMember("b2s", static_cast<int>(m.measurement.b2s_input_dBm), document.GetAllocator());
        mj.AddMember("s2b", static_cast<int>(m.measurement.s2b_input_dBm), document.GetAllocator());
        mj.AddMember("sensor_errors", static_cast<uint32_t>(m.measurement.flags), document.GetAllocator());

        document.PushBack(std::move(mj), document.GetAllocator());
    }

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    document.Accept(writer);

    m_channel.send(data::Server_Message::REPORT_MEASUREMENTS_REQ, buffer.GetString(), buffer.GetSize());

    {
        std::string details = compute_sensor_details_response();
        m_channel.send(data::Server_Message::REPORT_SENSORS_DETAILS, details.data(), details.size());
    }
}

///////////////////////////////////////////////////////////////////////////////////////////

void Server::sensor_bound(Sensors::Sensor_Id sensor_id, Sensors::Sensor_Address sensor_address, uint32_t serial_number, Sensors::Calibration const& calibration)
{
    rapidjson::Document document;
    document.SetObject();
    document.AddMember("id", sensor_id, document.GetAllocator());
    document.AddMember("address", sensor_address, document.GetAllocator());
    document.AddMember("temperature_bias", calibration.temperature_bias, document.GetAllocator());
    document.AddMember("humidity_bias", calibration.humidity_bias, document.GetAllocator());
    document.AddMember("serial_number", serial_number, document.GetAllocator());

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    document.Accept(writer);

    m_channel.send(data::Server_Message::SENSOR_BOUND_REQ, buffer.GetString(), buffer.GetSize());
}

///////////////////////////////////////////////////////////////////////////////////////////

void Server::sensor_details_changed(Sensors::Sensor_Id /*sensor_id*/)
{
    std::string details = compute_sensor_details_response();
    m_channel.send(data::Server_Message::REPORT_SENSORS_DETAILS, details.data(), details.size());
}

///////////////////////////////////////////////////////////////////////////////////////////

static boost::optional<Sensors::Config> parse_config(rapidjson::Value const& configj, bool needs_baselines)
{
    Sensors::Config config;

    if (!configj.IsObject())
    {
        LOGE << "Cannot deserialize config: Bad json." << std::endl;
        return boost::none;
    }

    auto it = configj.FindMember("name");
    if (it == configj.MemberEnd() || !it->value.IsString())
    {
        LOGE << "Cannot deserialize config: Missing name." << std::endl;
        return boost::none;
    }
    config.name = it->value.GetString();

    it = configj.FindMember("sensors_sleeping");
    if (it == configj.MemberEnd() || !it->value.IsBool())
    {
        LOGE << "Cannot deserialize config: Missing sensors_sleeping." << std::endl;
        return boost::none;
    }
    config.sensors_sleeping = it->value.GetBool();

    it = configj.FindMember("sensors_power");
    if (it == configj.MemberEnd() || !it->value.IsUint())
    {
        LOGE << "Cannot deserialize config: Missing sensors_power." << std::endl;
        return boost::none;
    }
    config.sensors_power = static_cast<uint8_t>(it->value.GetUint());

    it = configj.FindMember("measurement_period");
    if (it == configj.MemberEnd() || !it->value.IsInt64())
    {
        LOGE << "Cannot deserialize config: Missing measurement_period." << std::endl;
        return boost::none;
    }
    config.measurement_period = std::chrono::seconds(it->value.GetInt64());

    it = configj.FindMember("comms_period");
    if (it == configj.MemberEnd() || !it->value.IsInt64())
    {
        LOGE << "Cannot deserialize config: Missing comms_period." << std::endl;
        return boost::none;
    }
    config.comms_period = std::chrono::seconds(it->value.GetInt64());

    if (needs_baselines)
    {
        it = configj.FindMember("baseline_measurement_tp");
        if (it == configj.MemberEnd() || !it->value.IsUint64())
        {
            LOGE << "Cannot deserialize config: Missing baseline_measurement_tp." << std::endl;
            return boost::none;
        }
        uint64_t x = it->value.GetUint64();
        config.baseline_measurement_time_point = Server::Clock::from_time_t(time_t(x));

        it = configj.FindMember("baseline_measurement_index");
        if (it == configj.MemberEnd() || !it->value.IsUint())
        {
            LOGE << "Cannot deserialize config: Missing baseline_measurement_index." << std::endl;
            return boost::none;
        }
        config.baseline_measurement_index = it->value.GetUint();
    }

    return config;
}

///////////////////////////////////////////////////////////////////////////////////////////

void Server::process_add_config_req()
{
    std::vector<uint8_t> buffer;
    m_channel.unpack(buffer);

    bool ok = false;

    {
        rapidjson::Document document;
        document.Parse(reinterpret_cast<const char*>(buffer.data()), buffer.size());
        if (document.GetParseError() != rapidjson::ParseErrorCode::kParseErrorNone)
        {
            LOGE << "Cannot deserialize add config request: " << rapidjson::GetParseErrorFunc(document.GetParseError()) << std::endl;
            goto end;
        }

        boost::optional<Sensors::Config> opt_config = parse_config(document, false);
        if (!opt_config.is_initialized())
        {
            LOGE << "Cannot deserialize add config request: Bad document." << std::endl;
            goto end;
        }

        m_sensors.set_config(*opt_config);
        ok = true;
    }

    LOGI << "Set config request." << std::endl;

end:

    std::string configs;
    if (ok)
    {
        configs = compute_configs_response();
    }

    m_channel.send(data::Server_Message::ADD_CONFIG_RES, configs.data(), configs.size());
}

///////////////////////////////////////////////////////////////////////////////////////////

void Server::process_set_configs_req()
{
    std::vector<uint8_t> buffer;
    m_channel.unpack(buffer);

    bool ok = false;

    {
        rapidjson::Document document;
        document.Parse(reinterpret_cast<const char*>(buffer.data()), buffer.size());
        if (document.GetParseError() != rapidjson::ParseErrorCode::kParseErrorNone)
        {
            LOGE << "Cannot deserialize set config request: " << rapidjson::GetParseErrorFunc(document.GetParseError()) << std::endl;
            goto end;
        }
        if (!document.IsArray())
        {
            LOGE << "Cannot deserialize set config request: Bad json." << std::endl;
            goto end;
        }

        m_sensors.remove_all_configs();

        ok = true;
        for (size_t i = 0; i < document.Size(); i++)
        {
            rapidjson::Value const& configj = document[i];

            boost::optional<Sensors::Config> opt_config = parse_config(configj, true);
            if (!opt_config.is_initialized())
            {
                LOGE << "Cannot deserialize set config request: Bad document." << std::endl;
                ok = false;
                break;
            }
            m_sensors.set_config(*opt_config);
        }

        if (!ok)
        {
            LOGE << "Cannot set config." << std::endl;
            goto end;
        }
    }

    LOGI << "Set config request." << std::endl;

end:

    std::string configs;
    if (ok)
    {
        configs = compute_configs_response();
    }

    m_channel.send(data::Server_Message::SET_CONFIGS_RES, configs.data(), configs.size());
}

///////////////////////////////////////////////////////////////////////////////////////////

void Server::process_set_sensors_req()
{
    std::vector<uint8_t> buffer;
    m_channel.unpack(buffer);

    bool ok = false;

    {
        rapidjson::Document document;
        document.Parse(reinterpret_cast<const char*>(buffer.data()), buffer.size());
        if (document.GetParseError() != rapidjson::ParseErrorCode::kParseErrorNone)
        {
            LOGE << "Cannot deserialize set sensors request: " << rapidjson::GetParseErrorFunc(document.GetParseError()) << std::endl;
            goto end;
        }
        if (!document.IsArray())
        {
            LOGE << "Cannot deserialize set sensors request: Bad document." << std::endl;
            goto end;
        }

        m_sensors.remove_all_sensors();

        for (size_t i = 0; i < document.Size(); i++)
        {
            rapidjson::Value const& sensorj = document[i];

            auto it = sensorj.FindMember("name");
            if (it == sensorj.MemberEnd() || !it->value.IsString())
            {
                LOGE << "Cannot deserialize set sensors request: Missing name." << std::endl;
                goto end;
            }
            std::string name = it->value.GetString();

            it = sensorj.FindMember("id");
            if (it == sensorj.MemberEnd() || !it->value.IsUint())
            {
                LOGE << "Cannot deserialize set sensors request: Missing id." << std::endl;
                goto end;
            }
            Sensors::Sensor_Id id = it->value.GetUint();

            it = sensorj.FindMember("address");
            if (it == sensorj.MemberEnd() || !it->value.IsUint())
            {
                LOGE << "Cannot deserialize set sensors request: Missing address." << std::endl;
                goto end;
            }
            Sensors::Sensor_Address address = it->value.GetUint();

            it = sensorj.FindMember("serial_number");
            if (it == sensorj.MemberEnd() || !it->value.IsUint())
            {
                LOGE << "Cannot deserialize set sensors request: Missing serial_number." << std::endl;
                goto end;
            }
            uint32_t serial_number = it->value.GetUint();

            Sensors::Calibration calibration;
            it = sensorj.FindMember("temperature_bias");
            if (it == sensorj.MemberEnd() || !it->value.IsNumber())
            {
                LOGE << "Cannot deserialize set sensors request: Missing temperature_bias." << std::endl;
                goto end;
            }
            calibration.temperature_bias = static_cast<float>(it->value.GetDouble());

            it = sensorj.FindMember("humidity_bias");
            if (it == sensorj.MemberEnd() || !it->value.IsNumber())
            {
                LOGE << "Cannot deserialize set sensors request: Missing humidity_bias." << std::endl;
                goto end;
            }
            calibration.humidity_bias = static_cast<float>(it->value.GetDouble());

            it = sensorj.FindMember("sleeping");
            if (it == sensorj.MemberEnd() || !it->value.IsBool())
            {
                LOGE << "Cannot deserialize set sensors request: Missing sleeping." << std::endl;
                goto end;
            }
            bool sleeping = it->value.GetBool();

            if (address == 0)
            {
                Sensors::Unbound_Sensor_Data data;
                data.name = name;
                data.id = id;
                //data.calibration = calibration;
                m_sensors.set_unbound_sensor_data(data);
            }
            else if (!m_sensors.add_sensor(id, name, address, serial_number, calibration))
            {
                LOGE << "Cannot add sensor." << std::endl;
                goto end;
            }
            m_sensors.set_sensor_sleeping(id, sleeping);
        }
        ok = true;
    }

    LOGI << "Set sensor request." << std::endl;

end:

    std::string details;
    if (ok)
    {
        details = compute_sensor_details_response();
    }

    m_channel.send(data::Server_Message::SET_SENSORS_RES, details.data(), details.size());
}

///////////////////////////////////////////////////////////////////////////////////////////

void Server::process_add_sensor_req()
{
    std::vector<uint8_t> buffer;
    m_channel.unpack(buffer);

    bool ok = false;

    {
        Sensors::Unbound_Sensor_Data data;

        rapidjson::Document document;
        document.Parse(reinterpret_cast<const char*>(buffer.data()), buffer.size());
        if (document.GetParseError() != rapidjson::ParseErrorCode::kParseErrorNone)
        {
            LOGE << "Cannot deserialize add sensor request: " << rapidjson::GetParseErrorFunc(document.GetParseError()) << std::endl;
            goto end;
        }
        if (!document.IsObject())
        {
            LOGE << "Cannot deserialize add sensor request: Bad document." << std::endl;
            goto end;
        }

        auto it = document.FindMember("name");
        if (it == document.MemberEnd() || !it->value.IsString())
        {
            LOGE << "Cannot deserialize add sensor request: Missing name." << std::endl;
            goto end;
        }
        data.name = it->value.GetString();

        it = document.FindMember("id");
        if (it == document.MemberEnd() || !it->value.IsUint())
        {
            LOGE << "Cannot deserialize add sensor request: Missing id." << std::endl;
            goto end;
        }
        data.id = it->value.GetUint();

        m_sensors.set_unbound_sensor_data(data);

        ok = true;
    }

    LOGI << "Add sensor request." << std::endl;

end:
    std::string details;
    if (ok)
    {
        details = compute_sensor_details_response();
    }

    m_channel.send(data::Server_Message::ADD_SENSOR_RES, details.data(), details.size());
}

///////////////////////////////////////////////////////////////////////////////////////////

void Server::process_report_measurements_res()
{
    std::vector<uint8_t> buffer;
    m_channel.unpack(buffer);

    {
        rapidjson::Document document;
        document.Parse(reinterpret_cast<const char*>(buffer.data()), buffer.size());
        if (document.GetParseError() != rapidjson::ParseErrorCode::kParseErrorNone)
        {
            LOGE << "Cannot deserialize report measurements response: " << rapidjson::GetParseErrorFunc(document.GetParseError()) << std::endl;
            goto end;
        }
        if (!document.IsArray())
        {
            LOGE << "Cannot deserialize report measurements response: Bad document." << std::endl;
            goto end;
        }

        for (size_t i = 0; i < document.Size(); i++)
        {
            rapidjson::Value const& valuej = document[i];
            if (!valuej.IsObject())
            {
                LOGE << "Cannot deserialize report measurements response: Bad value." << std::endl;
                goto end;
            }

            auto it = valuej.FindMember("sensor_id");
            if (it == valuej.MemberEnd() || !it->value.IsUint())
            {
                LOGE << "Cannot deserialize report measurements response: Missing sensor_id." << std::endl;
                goto end;
            }
            Sensors::Sensor_Id sensor_id = it->value.GetUint();

            it = valuej.FindMember("index");
            if (it == valuej.MemberEnd() || !it->value.IsUint())
            {
                LOGE << "Cannot deserialize report measurements response: Missing index." << std::endl;
                goto end;
            }
            uint32_t index = it->value.GetUint();

            it = valuej.FindMember("ok");
            if (it == valuej.MemberEnd() || !it->value.IsBool())
            {
                LOGE << "Cannot deserialize report measurements response: Missing ok." << std::endl;
                goto end;
            }
            bool ok = it->value.GetBool();
            if (ok)
            {
                LOGI << "Measurement " << std::to_string(index) << " from sensor << " << std::to_string(sensor_id) << " confirmed." << std::endl;
                m_sensors.confirm_measurement(sensor_id, index);
            }
            else
            {
                LOGE << "Measurement " << std::to_string(index) << " from sensor << " << std::to_string(sensor_id) << " not confirmed." << std::endl;
            }
        }
    }
end:

    return;
}

///////////////////////////////////////////////////////////////////////////////////////////

void Server::process_sensor_bound_res()
{
    std::vector<uint8_t> buffer;
    m_channel.unpack(buffer);

    if (buffer.size() != 1)
    {
        LOGE << "Cannot deserialize sensor bound response: Bad data." << std::endl;
        return;
    }

    bool ok = *reinterpret_cast<bool const*>(buffer.data());
    if (!ok)
    {
        LOGE << "Binding the sensor FAILED." << std::endl;
        return;
    }

    LOGI << "Binding the sensor succeded." << std::endl;
}

///////////////////////////////////////////////////////////////////////////////////////////

void Server::process_power_off_req()
{
    sync();
    system("sudo shutdown -h now");
}

///////////////////////////////////////////////////////////////////////////////////////////

void Server::process()
{
    if (!m_socket.is_open() || !m_is_connected)
    {
        if (!m_is_accepting)
        {
            LOGI << "Disconnected. Listening..." << std::endl;
            start_accept();
        }
        return;
    }

    data::Server_Message message;
    while (m_channel.get_next_message(message))
    {
        switch (message)
        {
        case data::Server_Message::ADD_CONFIG_REQ:
            process_add_config_req();
            break;
        case data::Server_Message::SET_CONFIGS_REQ:
            process_set_configs_req();
            break;
        case data::Server_Message::SET_SENSORS_REQ:
            process_set_sensors_req();
            break;
        case data::Server_Message::ADD_SENSOR_REQ:
            process_add_sensor_req();
            break;
        case data::Server_Message::REPORT_MEASUREMENTS_RES:
            process_report_measurements_res();
            break;
        case data::Server_Message::SENSOR_BOUND_RES:
            process_sensor_bound_res();
            break;
        case data::Server_Message::POWER_OFF_REQ:
            process_power_off_req();
            break;
        default:
            LOGE << "Invalid message received: " << int(message) << std::endl;
        }
    }
}

