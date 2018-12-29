#include "Server.h"

#include <iostream>
#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/writer.h"
#include "rapidjson/reader.h"
#include "rapidjson/stringbuffer.h"

#include "Log.h"

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
bool unpack(C const& c, void* data, size_t size, size_t& offset)
{
    if (offset + size > c.size())
    {
        return false;
    }
    memcpy(data, c.data() + offset, size);
    offset += size;
    return true;
}

template<typename C, typename T>
bool unpack(C const& c, T& t, size_t& offset)
{
    static_assert(std::is_standard_layout<T>::value, "Only PODs pls");
    return unpack(c, &t, sizeof(T), offset);
}

///////////////////////////////////////////////////////////////////////////////////////////

Server::Server(Radio& radio, LEDs& leds)
    : m_leds(leds)
    , m_socket(m_io_service)
    , m_socket_adapter(m_socket)
    , m_channel(m_socket_adapter)
    , m_radio(radio)
    , m_radio_requests(1)
    , m_radio_responses(1)
    , m_broadcast_socket(m_io_service)
{
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

    m_radio_requests.exit();
    m_radio_responses.exit();
    if (m_radio_thread.joinable())
    {
        m_radio_thread.join();
    }
}

///////////////////////////////////////////////////////////////////////////////////////////

bool Server::init(uint16_t port, uint16_t broadcast_port)
{
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

    m_radio_thread = std::thread([this]
    {
        LOGI << "Started sensor comms thread." << std::endl;
        radio_thread_func();
        LOGI << "Stopping sensor comms thread." << std::endl;
    });

    LOGI << "Server initialized!" << std::endl;

    return true;
}

///////////////////////////////////////////////////////////////////////////////////////////

void Server::start_accept()
{
    std::lock_guard<std::recursive_mutex> lg(m_mutex);

    LOGI << "Started accepting connections." << std::endl;
    bool ok = true;
    try
    {
        m_socket.close();
        m_acceptor->async_accept(m_socket, std::bind(&Server::accept_func, this, std::placeholders::_1));
    }
    catch (std::exception const& e)
    {
        LOGE << "Start accept error: " << e.what() << std::endl;
        ok = false;
    }

    if (ok)
    {
        m_is_connected = false;
        m_is_accepting = true;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////

void Server::accept_func(asio::error_code ec)
{
    std::lock_guard<std::recursive_mutex> lg(m_mutex);

    if (ec)
    {
        LOGE << "Accept error: " << ec.message() << std::endl;
        start_accept();
    }
    else
    {
        m_is_accepting = false;
        bool ok = true;
        try
        {
            asio::ip::tcp::no_delay option(true);
            m_socket.set_option(option);
            m_socket_adapter.start();
        }
        catch (std::exception const& e)
        {
            LOGE << "Start accept error: " << e.what() << std::endl;
            ok = false;
        }

        if (ok)
        {
            LOGI << "Connected!" << std::endl;
            m_is_connected = true;
            m_last_talk_tp = Clock::now();
        }
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

bool Server::is_connected() const
{
    std::lock_guard<std::recursive_mutex> lg(m_mutex);
    return m_is_connected;
}

///////////////////////////////////////////////////////////////////////////////////////////

void Server::radio_thread_func()
{
    m_radio.set_address(Radio::BASE_ADDRESS);
    std::vector<uint8_t> raw_packet_data(packet_raw_size(Radio::MAX_USER_DATA_SIZE));

    Server::Sensor_Request request;
    Server::Sensor_Response response;

    while (!m_exit)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

        {
            std::lock_guard<std::mutex> lg(m_sensor_comms_mutex);
            m_radio.start_async_receive();

            uint8_t size = Radio::MAX_USER_DATA_SIZE;
            uint8_t* packet_data = m_radio.async_receive_packet(raw_packet_data.data(), size);
            if (packet_data)
            {
                m_radio.stop_async_receive();
                m_leds.set_color(LEDs::Color::Yellow);

                request.type = m_radio.get_rx_packet_type(packet_data);
                request.signal_s2b = m_radio.get_input_dBm();
                request.address = m_radio.get_rx_packet_source_address(packet_data);
                request.needs_response = m_radio.get_rx_packet_needs_response(packet_data);
                LOGI << "Incoming type " << (int)request.type
                     << ", size " << (int)size
                     << ", address " << (int)request.address
                     << ", signal strength " << (int)request.signal_s2b << "dBm. Sending to manager..." << std::endl;

                //            if (request.address != 1004 && request.address >= 1000)
                //            {
                //                //XXXYYY
                //                continue;
                //            }

                //TODO - add a marker in the request in indicate if the sensor waits for a response.
                //Like this I can avoid the RTT time for measurement batches, since they don't need a response

                request.payload.resize(size);
                if (size > 0)
                {
                    memcpy(request.payload.data(), m_radio.get_rx_packet_payload(packet_data), size);
                }
                if (m_radio_requests.push_back_timeout(request, std::chrono::seconds(10)) && request.needs_response)
                {
                    if (m_radio_responses.pop_front_timeout(response, std::chrono::seconds(10)))
                    {
                        if (response.is_valid)
                        {
                            LOGI << "\tdone. Sending response back..." << std::endl;
                            m_radio.set_destination_address(response.address);
                            m_radio.begin_packet(raw_packet_data.data(), response.type, false);
                            if (!response.payload.empty())
                            {
                                m_radio.pack(raw_packet_data.data(), response.payload.data(), static_cast<uint8_t>(response.payload.size()));
                            }

                            if (m_radio.send_packed_packet(raw_packet_data.data(), true))
                            {
                                m_leds.set_blink(LEDs::Blink::Fast_Yellow, std::chrono::seconds(1), true);
                                LOGI << "\tdone" << std::endl;
                            }
                            else
                            {
                                m_leds.set_blink(LEDs::Blink::Fast_Red, std::chrono::seconds(1), true);
                                LOGE << "\tfailed" << std::endl;
                            }
                        }
                        else
                        {
                            m_leds.set_blink(LEDs::Blink::Fast_Red, std::chrono::seconds(1), true);
                            LOGE << "\tInvalid response received" << std::endl;
                        }
                    }
                }
            }

            if (m_new_radio_state != m_radio_state)
            {
                LOGI << "Changing radio state to " << (int) m_new_radio_state << std::endl;
                m_revert_radio_state_to_normal_tp = std::chrono::system_clock::now() + std::chrono::seconds(180);
                m_radio_state = m_new_radio_state;

                if (m_radio_state == data::Radio_State::PAIRING)
                {
                    m_radio.stop_async_receive();
                    m_radio.set_frequency(869.f);
                }
                else
                {
                    m_radio.stop_async_receive();
                    m_radio.set_frequency(868.f);
                }
            }
            if (m_radio_state != data::Radio_State::NORMAL)
            {
                if (std::chrono::system_clock::now() >= m_revert_radio_state_to_normal_tp)
                {
                    LOGI << "Reverting radio state to NORMAL" << std::endl;
                    m_new_radio_state = data::Radio_State::NORMAL;
                }
            }
        }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////

Server::Result Server::send_sensor_message(Sensor_Request const& request, Sensor_Response& response)
{
    std::lock_guard<std::recursive_mutex> lg(m_mutex);

    response.is_valid = false;

    if (!is_connected())
    {
        return Result::Connection_Error;
    }
    std::array<uint8_t, 1024> buffer;
    size_t offset = 0;
    uint32_t req_id = m_last_request_id++;
    pack(buffer, req_id, offset);
    pack(buffer, request.signal_s2b, offset);
    pack(buffer, request.type, offset);
    pack(buffer, request.address, offset);
    pack(buffer, static_cast<uint32_t>(request.payload.size()), offset);
    if (!request.payload.empty())
    {
        pack(buffer, request.payload.data(), request.payload.size(), offset);
    }

    m_channel.send(data::Server_Message::SENSOR_REQ, buffer.data(), offset);
    if (!request.needs_response)
    {
        return Result::Ok;
    }

    if (wait_for_message(data::Server_Message::SENSOR_RES, std::chrono::milliseconds(200)))
    {
        size_t max_size = 0;
        bool ok = m_channel.unpack_fixed(buffer, max_size) == Channel::Unpack_Result::OK;

        uint32_t res_id = 0;
        bool has_response = false;
        offset = 0;

        ok &= unpack(buffer, res_id, offset);
        ok &= unpack(buffer, has_response, offset);
        ok &= req_id == res_id;

        if (ok && !has_response)
        {
            return Result::Ok;
        }

        uint32_t payload_size = 0;
        ok &= unpack(buffer, response.type, offset);
        ok &= unpack(buffer, response.address, offset);
        ok &= unpack(buffer, payload_size, offset);
        ok &= payload_size < 1024 * 1024;
        if (ok)
        {
            response.payload.resize(payload_size);
            ok &= unpack(buffer, response.payload.data(), payload_size, offset);
        }
        response.is_valid = true;
        return ok ? Result::Ok : Result::Data_Error;
    }
    return Result::Timeout_Error;
}

///////////////////////////////////////////////////////////////////////////////////////////

bool Server::wait_for_message(data::Server_Message expected_message, Clock::duration timeout)
{
    std::lock_guard<std::recursive_mutex> lg(m_mutex);

    if (!m_socket.is_open() || !m_is_connected)
    {
        if (!m_is_accepting)
        {
            LOGI << "Disconnected. Listening..." << std::endl;
            start_accept();
        }
        return false;
    }

    size_t count = 0;
    Clock::time_point start_tp = Clock::now();
    data::Server_Message message;
    while (Clock::now() < start_tp + timeout)
    {
        count++;
        if (m_channel.get_next_message(message))
        {
            m_last_talk_tp = Clock::now();
            if (message == expected_message)
            {
                return true;
            }
            process_message(message);
        }
        std::this_thread::sleep_for(std::chrono::microseconds(1));
    }
    return false;
}

///////////////////////////////////////////////////////////////////////////////////////////

void Server::process_ping()
{
    std::lock_guard<std::recursive_mutex> lg(m_mutex);

    std::array<uint8_t, 1024> buffer;
    size_t offset = 0;
    uint32_t req_id = m_last_request_id++;
    pack(buffer, req_id, offset);
    m_channel.send(data::Server_Message::PONG, buffer.data(), offset);
    //LOGI << "PING" << std::endl;
}

///////////////////////////////////////////////////////////////////////////////////////////

void Server::process_change_state_req()
{
    std::lock_guard<std::recursive_mutex> lg(m_mutex);

    size_t max_size = 0;
    std::array<uint8_t, 1024> buffer;
    bool ok = m_channel.unpack_fixed(buffer, max_size) == Channel::Unpack_Result::OK;

    data::Radio_State new_state;
    size_t offset = 0;

    ok &= unpack(buffer, new_state, offset);
    if (ok)
    {
        m_new_radio_state = new_state;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////

void Server::process_message(data::Server_Message message)
{
    std::lock_guard<std::recursive_mutex> lg(m_mutex);

    switch (message)
    {
    case data::Server_Message::PING:
        process_ping();
        break;
    case data::Server_Message::CHANGE_RADIO_STATE_REQ:
        process_change_state_req();
        break;
    default:
        LOGE << "Invalid message received: " << int(message) << std::endl;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////

void Server::process()
{
    std::lock_guard<std::recursive_mutex> lg(m_mutex);

    if (!m_socket.is_open() || !m_is_connected)
    {
        m_leds.set_blink(LEDs::Blink::Slow_Green, std::chrono::seconds(10));

        if (!m_is_accepting)
        {
            LOGI << "Disconnected. Listening..." << std::endl;
            start_accept();
        }
        return;
    }

    if (m_radio_state == data::Radio_State::PAIRING)
    {
        m_leds.set_blink(LEDs::Blink::Slow_Green_Yellow, std::chrono::seconds(10));
    }
    else
    {
        m_leds.set_color(LEDs::Color::Green);
    }

    if (Clock::now() - m_last_talk_tp > std::chrono::seconds(10))
    {
        LOGI << "Timed out. Disconnecting & listening..." << std::endl;
        start_accept();
        return;
    }

    {
        Sensor_Request request;
        Sensor_Response response;
        while (m_radio_requests.pop_front(request, false))
        {
            Result result = send_sensor_message(request, response);
            if (result == Result::Ok)
            {
                if (request.needs_response)
                {
                    LOGI << "\tdone. Sending response back..." << std::endl;
                    m_radio_responses.push_back(response, true);
                }
            }
            else
            {
                m_leds.set_blink(LEDs::Blink::Fast_Red, std::chrono::seconds(1), true);
                LOGE << "\tfailed: " << int(result) << std::endl;
            }
        }
    }

    {
        data::Server_Message message;
        while (m_channel.get_next_message(message))
        {
            m_last_talk_tp = Clock::now();
            process_message(message);
        }
    }
}

