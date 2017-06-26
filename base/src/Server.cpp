#include "Server.h"
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

Server::Server(Sensors& sensors)
    : m_sensors(sensors)
    , m_socket(m_io_service)
    , m_socket_adapter(m_socket)
    , m_channel(m_socket_adapter)
    , m_broadcast_socket(m_io_service)
{
    m_sensors.cb_report_measurement = std::bind(&Server::report_measurement, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
    m_sensors.cb_sensor_bound = std::bind(&Server::sensor_bound, this, std::placeholders::_1, std::placeholders::_2);
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
        m_io_service_work.reset(new boost::asio::io_service::work(m_io_service));
        m_io_service_thread = boost::thread(boost::bind(&boost::asio::io_service::run, &m_io_service));

        m_broadcast_thread = boost::thread([this] { broadcast_thread_func(); });

        m_acceptor.reset(new boost::asio::ip::tcp::acceptor(m_io_service, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port)));

        start_accept();
    }
    catch (std::exception const& e)
    {
        std::cerr << "Init error: " << e.what() << "\n";
        return false;
    }

    std::cout << "Server initialized!";

    return true;
}

///////////////////////////////////////////////////////////////////////////////////////////

void Server::start_accept()
{
    try
    {
        m_acceptor->async_accept(m_socket, boost::bind(&Server::accept_func, this, boost::asio::placeholders::error));
    }
    catch (std::exception const& e)
    {
        std::cerr << "Start accept error: " << e.what() << "\n";
    }
}

///////////////////////////////////////////////////////////////////////////////////////////

void Server::accept_func(boost::system::error_code ec)
{
    if (ec)
    {
        std::cerr << "Accept error: " << ec.message() << "\n";
    }
    else
    {
        m_is_connected = true;
        m_socket_adapter.start();
    }

    start_accept();
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

        m_broadcast_socket.open(boost::asio::ip::udp::v4());
        m_broadcast_socket.set_option(boost::asio::ip::udp::socket::reuse_address(true));
        m_broadcast_socket.set_option(boost::asio::socket_base::broadcast(true));
        m_broadcast_endpoint = boost::asio::ip::udp::endpoint(boost::asio::ip::address_v4::broadcast(), m_broadcast_port);
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
                m_broadcast_socket.send_to(boost::asio::buffer(m_mac_address, 6), m_broadcast_endpoint);
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

void Server::report_measurement(Sensors::Sensor_Id sensor_id, Clock::time_point time_point, Sensors::Measurement const& measurement)
{
    try
    {
        std::stringstream ss;
        boost::property_tree::ptree pt;

        pt.add("sensor_id", sensor_id);
        pt.add("time_point", std::chrono::duration_cast<std::chrono::seconds>(time_point.time_since_epoch()).count());
        pt.add("measurement_index", measurement.index);
        pt.add("measurement_temperature", measurement.temperature);
        pt.add("measurement_humidity", measurement.humidity);
        pt.add("measurement_vcc", measurement.vcc);
        pt.add("measurement_b2s_input_dBm", measurement.b2s_input_dBm);
        pt.add("measurement_s2b_input_dBm", measurement.s2b_input_dBm);
        pt.add("measurement_flags", measurement.flags);

        boost::property_tree::write_json(ss, pt);

        std::string str = ss.str();
        m_channel.send(data::Server_Message::REPORT_MEASUREMENT_REQ, str.data(), str.size());
    }
    catch (std::exception const& e)
    {
        std::cerr << "Cannot serialize request: " << e.what() << "\n";
    }
}

///////////////////////////////////////////////////////////////////////////////////////////

void Server::sensor_bound(Sensors::Sensor_Id sensor_id, Sensors::Sensor_Address sensor_address)
{
    try
    {
        std::stringstream ss;
        boost::property_tree::ptree pt;

        pt.add("sensor_id", sensor_id);
        pt.add("sensor_address", sensor_address);

        boost::property_tree::write_json(ss, pt);

        std::string str = ss.str();
        m_channel.send(data::Server_Message::SENSOR_BOUND_REQ, str.data(), str.size());
    }
    catch (std::exception const& e)
    {
        std::cerr << "Cannot serialize request: " << e.what() << "\n";
    }
}

///////////////////////////////////////////////////////////////////////////////////////////

void Server::process_get_config_req()
{
    std::stringstream ss;

    try
    {
        boost::property_tree::ptree pt;

        Sensors::Config const& config = m_sensors.get_config();

        pt.add("sensors_sleeping", config.sensors_sleeping);
        pt.add("measurement_period", std::chrono::duration_cast<std::chrono::seconds>(config.measurement_period).count());
        pt.add("comms_period", std::chrono::duration_cast<std::chrono::seconds>(config.comms_period).count());
        pt.add("baseline_time_point", std::chrono::duration_cast<std::chrono::seconds>(config.baseline_time_point.time_since_epoch()).count());

        boost::property_tree::write_json(ss, pt);
    }
    catch (std::exception const& e)
    {
        std::cerr << "Cannot serialize request: " << e.what() << "\n";
    }

    std::string str = ss.str();
    m_channel.send(data::Server_Message::GET_CONFIG_RES, str.data(), str.size());
}

///////////////////////////////////////////////////////////////////////////////////////////

void Server::process_set_config_req()
{
    std::vector<uint8_t> buffer;
    m_channel.unpack(buffer);

    std::string str_buffer(reinterpret_cast<char*>(buffer.data()), reinterpret_cast<char*>(buffer.data()) + buffer.size());
    std::stringstream ss(str_buffer);

    bool ok = false;

    try
    {
        boost::property_tree::ptree pt;
        boost::property_tree::read_json(ss, pt);

        Sensors::Config config;

        config.sensors_sleeping = pt.get<bool>("sensors_sleeping");
        config.measurement_period = std::chrono::seconds(pt.get<std::chrono::seconds::rep>("measurement_period"));
        config.comms_period = std::chrono::seconds(pt.get<std::chrono::seconds::rep>("comms_period"));
        config.baseline_time_point = Clock::time_point(std::chrono::seconds(pt.get<std::chrono::seconds::rep>("baseline_time_point")));

        m_sensors.set_config(config);
        if (!m_sensors.init())
        {
            std::cerr << "Cannot initialize sensors\n";
        }
        else
        {
            ok = true;
        }
    }
    catch (std::exception const& e)
    {
        std::cerr << "Cannot deserialize request: " << e.what() << "\n";
    }

    m_channel.send(data::Server_Message::SET_CONFIG_RES, &ok, 1);
}

///////////////////////////////////////////////////////////////////////////////////////////

void Server::process_get_sensors_req()
{
    std::stringstream ss;

    try
    {
        boost::property_tree::ptree pt;

        for (Sensors::Sensor const& sensor: m_sensors.get_sensors())
        {
            auto node = pt.add("sensors." + sensor.name, "");
            node.add("address", sensor.name);
            node.add("id", sensor.id);
        }
        boost::property_tree::write_json(ss, pt);
    }
    catch (std::exception const& e)
    {
        std::cerr << "Cannot serialize request: " << e.what() << "\n";
    }

    std::string str = ss.str();
    m_channel.send(data::Server_Message::GET_SENSORS_RES, str.data(), str.size());
}

///////////////////////////////////////////////////////////////////////////////////////////

void Server::process_add_sensor_req()
{
    std::vector<uint8_t> buffer;
    m_channel.unpack(buffer);

    std::string str_buffer(reinterpret_cast<char*>(buffer.data()), reinterpret_cast<char*>(buffer.data()) + buffer.size());
    std::stringstream ss(str_buffer);

    bool ok = false;

    try
    {
        boost::property_tree::ptree pt;
        boost::property_tree::read_json(ss, pt);

        Sensors::Unbound_Sensor_Data data;
        data.name = pt.get<std::string>("unbound_sensor_name");
        m_sensors.set_unbound_sensor_data(data);
        ok = true;
    }
    catch (std::exception const& e)
    {
        std::cerr << "Cannot deserialize request: " << e.what() << "\n";
    }

    m_channel.send(data::Server_Message::ADD_SENSOR_RES, &ok, 1);
}

///////////////////////////////////////////////////////////////////////////////////////////

void Server::process_remove_sensor_req()
{
    std::vector<uint8_t> buffer;
    m_channel.unpack(buffer);

    std::string str_buffer(reinterpret_cast<char*>(buffer.data()), reinterpret_cast<char*>(buffer.data()) + buffer.size());
    std::stringstream ss(str_buffer);

    bool ok = false;

    try
    {
        boost::property_tree::ptree pt;
        boost::property_tree::read_json(ss, pt);

        Sensors::Sensor_Id id = pt.get<Sensors::Sensor_Id>("id");
        ok = m_sensors.remove_sensor(id);
    }
    catch (std::exception const& e)
    {
        std::cerr << "Cannot deserialize request: " << e.what() << "\n";
    }

    m_channel.send(data::Server_Message::REMOVE_SENSOR_RES, &ok, 1);
}

///////////////////////////////////////////////////////////////////////////////////////////

void Server::process_report_measurement_res()
{
    std::vector<uint8_t> buffer;
    m_channel.unpack(buffer);

    std::string str_buffer(reinterpret_cast<char*>(buffer.data()), reinterpret_cast<char*>(buffer.data()) + buffer.size());
    std::stringstream ss(str_buffer);

    try
    {
        boost::property_tree::ptree pt;
        boost::property_tree::read_json(ss, pt);

        Sensors::Sensor_Id id = pt.get<Sensors::Sensor_Id>("id");
        uint32_t measurement_index = pt.get<uint32_t>("measurement_index");
        m_sensors.confirm_measurement(id, measurement_index);
    }
    catch (std::exception const& e)
    {
        std::cerr << "Cannot deserialize request: " << e.what() << "\n";
    }
}

///////////////////////////////////////////////////////////////////////////////////////////

void Server::process_sensor_bound_res()
{
    std::vector<uint8_t> buffer;
    m_channel.unpack(buffer);

    std::string str_buffer(reinterpret_cast<char*>(buffer.data()), reinterpret_cast<char*>(buffer.data()) + buffer.size());
    std::stringstream ss(str_buffer);

    try
    {
        boost::property_tree::ptree pt;
        boost::property_tree::read_json(ss, pt);

        Sensors::Sensor_Id id = pt.get<Sensors::Sensor_Id>("id");
        bool confirmed = pt.get<bool>("confirmed");
        m_sensors.confirm_sensor_binding(id, confirmed);
    }
    catch (std::exception const& e)
    {
        std::cerr << "Cannot deserialize request: " << e.what() << "\n";
    }
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
        return;
    }

    data::Server_Message message;
    while (m_channel.get_next_message(message))
    {
        switch (message)
        {
        case data::Server_Message::GET_CONFIG_REQ:
            process_get_config_req();
            break;
        case data::Server_Message::SET_CONFIG_REQ:
            process_set_config_req();
            break;
        case data::Server_Message::GET_SENSORS_REQ:
            process_get_sensors_req();
            break;
        case data::Server_Message::ADD_SENSOR_REQ:
            process_add_sensor_req();
            break;
        case data::Server_Message::REMOVE_SENSOR_REQ:
            process_remove_sensor_req();
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

