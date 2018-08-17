#include "Server.h"

#include <iostream>
#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/writer.h"
#include "rapidjson/reader.h"
#include "rapidjson/stringbuffer.h"


Server::Server(Sensors& sensors)
    : m_sensors(sensors)
    , m_socket(m_io_service)
    , m_socket_adapter(m_socket)
    , m_channel(m_socket_adapter)
    , m_broadcast_socket(m_io_service)
{
    m_sensors.cb_report_measurement = std::bind(&Server::report_measurement, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
    m_sensors.cb_sensor_bound = std::bind(&Server::sensor_bound, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4);
    m_sensors.cb_sensor_details_changed = std::bind(&Server::sensor_details_changed, this, std::placeholders::_1);
}

///////////////////////////////////////////////////////////////////////////////////////////

Server::~Server()
{
    m_io_service_work.reset();
    m_exit = true;

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
        std::cerr << "Sensors not initialized. Functionality will be limited\n";
    }

    {
        struct ifreq ifr;
        struct ifconf ifc;
        char buf[1024];
        int success = 0;

        int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
        if (sock == -1)
        {
            std::cerr << "Cannot get MAC address: socket failed\n";
            return -1;
        };

        ifc.ifc_len = sizeof(buf);
        ifc.ifc_buf = buf;
        if (ioctl(sock, SIOCGIFCONF, &ifc) == -1)
        {
            close(sock);
            std::cerr << "Cannot get MAC address: ioctl SIOCGIFCONF failed\n";
            return -1;
        }

        struct ifreq* it = ifc.ifc_req;
        const struct ifreq* const end = it + (ifc.ifc_len / sizeof(struct ifreq));

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
                std::cerr << "Cannot get MAC address: ioctl SIOCGIFFLAGS failed\n";
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
            std::cerr << "Cannot get MAC address.\n";
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
            std::cout << "Started io service thread.\n";
            m_io_service.run();
            std::cout << "Stopping io service thread.\n";
        });
        m_broadcast_thread = std::thread([this]
        {
            std::cout << "Started broadcast thread.\n";
            broadcast_thread_func();
            std::cout << "Stopping broadcast thread.\n";
        });

        m_acceptor.reset(new asio::ip::tcp::acceptor(m_io_service, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port)));

        start_accept();
    }
    catch (std::exception const& e)
    {
        std::cerr << "Init error: " << e.what() << "\n";
        return false;
    }

    std::cout << "Server initialized!\n";

    return true;
}

///////////////////////////////////////////////////////////////////////////////////////////

void Server::start_accept()
{
    std::cout << "Started accepting connections.\n";
    try
    {
        m_is_accepting = true;
        m_socket.close();
        m_acceptor->async_accept(m_socket, std::bind(&Server::accept_func, this, std::placeholders::_1));
    }
    catch (std::exception const& e)
    {
        std::cerr << "Start accept error: " << e.what() << "\n";
    }
}

///////////////////////////////////////////////////////////////////////////////////////////

void Server::accept_func(asio::error_code ec)
{
    if (ec)
    {
        std::cerr << "Accept error: " << ec.message() << "\n";
        start_accept();
    }
    else
    {
        std::cout << "Connected!\n";
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
        std::cerr << "Broadcast init error: " << e.what() << "\n";
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
            std::cerr << "Broadcast error: " << e.what() << ". Reinitializing...\n";
            init_broadcast();
        }
    }

    try
    {
        m_broadcast_socket.close();
    }
    catch (std::exception const& e)
    {
        std::cerr << "Broadcast close error: " << e.what() << "\n";
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

            int32_t dt = std::chrono::duration_cast<std::chrono::seconds>(m_sensors.compute_next_measurement_time_point(sensor.id) - Clock::now()).count();
            sensorj.AddMember("next_measurement_dt", dt, document.GetAllocator());

            sensorj.AddMember("last_comms_tp", static_cast<uint64_t>(Clock::to_time_t(m_sensors.get_sensor_last_comms_time_point(sensor.id))), document.GetAllocator());

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

void Server::report_measurement(Sensors::Sensor_Id sensor_id, Clock::time_point time_point, Sensors::Measurement const& measurement)
{
    rapidjson::Document document;
    document.SetObject();
    document.AddMember("sensor_id", sensor_id, document.GetAllocator());
    document.AddMember("index", measurement.index, document.GetAllocator());
    document.AddMember("time_point", static_cast<int64_t>(Clock::to_time_t(time_point)), document.GetAllocator());
    document.AddMember("temperature", measurement.temperature, document.GetAllocator());
    document.AddMember("humidity", measurement.humidity, document.GetAllocator());
    document.AddMember("vcc", measurement.vcc, document.GetAllocator());
    document.AddMember("b2s", static_cast<int>(measurement.b2s_input_dBm), document.GetAllocator());
    document.AddMember("s2b", static_cast<int>(measurement.s2b_input_dBm), document.GetAllocator());
    document.AddMember("sensor_errors", static_cast<uint32_t>(measurement.flags), document.GetAllocator());

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    document.Accept(writer);

    m_channel.send(data::Server_Message::REPORT_MEASUREMENT_REQ, buffer.GetString(), buffer.GetSize());

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

void Server::sensor_details_changed(Sensors::Sensor_Id sensor_id)
{
    std::string details = compute_sensor_details_response();
    m_channel.send(data::Server_Message::REPORT_SENSORS_DETAILS, details.data(), details.size());
}

///////////////////////////////////////////////////////////////////////////////////////////

void Server::process_set_config_req()
{
    std::vector<uint8_t> buffer;
    m_channel.unpack(buffer);

    bool ok = false;

    {
        Sensors::Config config;

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

        auto it = document.FindMember("name");
        if (it == document.MemberEnd() || !it->value.IsString())
        {
            std::cerr << "Cannot deserialize request: Missing name.\n";
            goto end;
        }
        config.name = it->value.GetString();

        it = document.FindMember("sensors_sleeping");
        if (it == document.MemberEnd() || !it->value.IsBool())
        {
            std::cerr << "Cannot deserialize request: Missing sensors_sleeping.\n";
            goto end;
        }
        config.sensors_sleeping = it->value.GetBool();

        it = document.FindMember("measurement_period");
        if (it == document.MemberEnd() || !it->value.IsInt64())
        {
            std::cerr << "Cannot deserialize request: Missing measurement_period.\n";
            goto end;
        }
        config.measurement_period = std::chrono::seconds(it->value.GetInt64());

        it = document.FindMember("comms_period");
        if (it == document.MemberEnd() || !it->value.IsInt64())
        {
            std::cerr << "Cannot deserialize request: Missing comms_period.\n";
            goto end;
        }
        config.comms_period = std::chrono::seconds(it->value.GetInt64());

        it = document.FindMember("baseline");
        if (it == document.MemberEnd() || !it->value.IsInt64())
        {
            std::cerr << "Cannot deserialize request: Missing baseline.\n";
            goto end;
        }
        config.baseline_time_point = Clock::time_point(std::chrono::seconds(it->value.GetInt64()));

        m_sensors.set_config(config);
        if (!m_sensors.init())
        {
            std::cerr << "Cannot initialize sensors\n";
            goto end;
        }
        else
        {
            ok = true;
        }
    }

    std::cout << "Set config request.\n";

end:

    std::string details;
    if (ok)
    {
        details = compute_sensor_details_response();
    }

    m_channel.send(data::Server_Message::SET_CONFIG_RES, details.data(), details.size());
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
            std::cerr << "Cannot deserialize request: " << rapidjson::GetParseErrorFunc(document.GetParseError()) << "\n";
            goto end;
        }
        if (!document.IsArray())
        {
            std::cerr << "Cannot deserialize request: Bad document.\n";
            goto end;
        }

        m_sensors.remove_all_sensors();

        for (size_t i = 0; i < document.Size(); i++)
        {
            rapidjson::Value const& sensorj = document[i];

            auto it = sensorj.FindMember("name");
            if (it == sensorj.MemberEnd() || !it->value.IsString())
            {
                std::cerr << "Cannot deserialize request: Missing name.\n";
                goto end;
            }
            std::string name = it->value.GetString();

            it = sensorj.FindMember("id");
            if (it == sensorj.MemberEnd() || !it->value.IsUint())
            {
                std::cerr << "Cannot deserialize request: Missing id.\n";
                goto end;
            }
            Sensors::Sensor_Id id = it->value.GetUint();

            it = sensorj.FindMember("address");
            if (it == sensorj.MemberEnd() || !it->value.IsUint())
            {
                std::cerr << "Cannot deserialize request: Missing address.\n";
                goto end;
            }
            Sensors::Sensor_Address address = it->value.GetUint();

            it = sensorj.FindMember("serial_number");
            if (it == sensorj.MemberEnd() || !it->value.IsUint())
            {
                std::cerr << "Cannot deserialize request: Missing serial_number.\n";
                goto end;
            }
            uint32_t serial_number = it->value.GetUint();

            Sensors::Calibration calibration;
            it = sensorj.FindMember("temperature_bias");
            if (it == sensorj.MemberEnd() || !it->value.IsNumber())
            {
                std::cerr << "Cannot deserialize request: Missing temperature_bias.\n";
                goto end;
            }
            calibration.temperature_bias = static_cast<float>(it->value.GetDouble());

            it = sensorj.FindMember("humidity_bias");
            if (it == sensorj.MemberEnd() || !it->value.IsNumber())
            {
                std::cerr << "Cannot deserialize request: Missing humidity_bias.\n";
                goto end;
            }
            calibration.humidity_bias = static_cast<float>(it->value.GetDouble());

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
                std::cerr << "Cannot add sensor\n";
                goto end;
            }
        }
        ok = true;
    }

    std::cout << "Set sensor request.\n";

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
            std::cerr << "Cannot deserialize request: " << rapidjson::GetParseErrorFunc(document.GetParseError()) << "\n";
            goto end;
        }
        if (!document.IsObject())
        {
            std::cerr << "Cannot deserialize request: Bad document.\n";
            goto end;
        }

        auto it = document.FindMember("name");
        if (it == document.MemberEnd() || !it->value.IsString())
        {
            std::cerr << "Cannot deserialize request: Missing name.\n";
            goto end;
        }
        data.name = it->value.GetString();

        it = document.FindMember("id");
        if (it == document.MemberEnd() || !it->value.IsUint())
        {
            std::cerr << "Cannot deserialize request: Missing id.\n";
            goto end;
        }
        data.id = it->value.GetUint();

        m_sensors.set_unbound_sensor_data(data);

        ok = true;
    }

    std::cout << "Add sensor request.\n";

end:
    std::string details;
    if (ok)
    {
        details = compute_sensor_details_response();
    }

    m_channel.send(data::Server_Message::ADD_SENSOR_RES, details.data(), details.size());
}

///////////////////////////////////////////////////////////////////////////////////////////

void Server::process_report_measurement_res()
{
    std::vector<uint8_t> buffer;
    m_channel.unpack(buffer);

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
        Sensors::Sensor_Id sensor_id = it->value.GetUint();

        it = document.FindMember("index");
        if (it == document.MemberEnd() || !it->value.IsUint())
        {
            std::cerr << "Cannot deserialize request: Missing index.\n";
            goto end;
        }
        uint32_t index = it->value.GetUint();

        it = document.FindMember("ok");
        if (it == document.MemberEnd() || !it->value.IsBool())
        {
            std::cerr << "Cannot deserialize request: Missing ok.\n";
            goto end;
        }
        bool ok = it->value.GetBool();

        if (ok)
        {
            std::cout << "Measurement " << std::to_string(index) << " from sensor << " << std::to_string(sensor_id) << " confirmed.\n";
            m_sensors.confirm_measurement(sensor_id, index);
        }
        else
        {
            std::cerr << "Measurement " << std::to_string(index) << " from sensor << " << std::to_string(sensor_id) << " not confirmed.\n";
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
        std::cerr << "Cannot deserialize request: Bad data.\n";
        return;
    }

    bool ok = *reinterpret_cast<bool const*>(buffer.data());
    if (!ok)
    {
        std::cerr << "Binding the sensor FAILED.\n";
        return;
    }

    std::cout << "Binding the sensor succeded.\n";
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
            std::cout << "Disconnected. Listening...\n";
            start_accept();
        }
        return;
    }

    data::Server_Message message;
    while (m_channel.get_next_message(message))
    {
        switch (message)
        {
        case data::Server_Message::SET_CONFIG_REQ:
            process_set_config_req();
            break;
        case data::Server_Message::SET_SENSORS_REQ:
            process_set_sensors_req();
            break;
        case data::Server_Message::ADD_SENSOR_REQ:
            process_add_sensor_req();
            break;
        case data::Server_Message::REPORT_MEASUREMENT_RES:
            process_report_measurement_res();
            break;
        case data::Server_Message::SENSOR_BOUND_RES:
            process_sensor_bound_res();
            break;
        case data::Server_Message::POWER_OFF_REQ:
            process_power_off_req();
            break;
        default:
            std::cerr << "Invalid message received: " << (int)message << "\n";
        }
    }
}

