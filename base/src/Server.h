#pragma once

#include <asio.hpp>
#include <memory>
#include <atomic>
#include <chrono>
#include <thread>

#include "ASIO_Socket_Adapter.h"
#include "Channel.h"
#include "Data_Defs.h"
#include "Sensor_Comms.h"

class Server
{
public:
    Server();
    ~Server();

    using Clock = std::chrono::system_clock;

    bool init(uint16_t comms_port, uint16_t broadcast_port);

    bool is_connected() const;

    struct Sensor_Request
    {
        uint8_t type = 0;
        int8_t signal_s2b = 0;
        Sensor_Comms::Address address = 0;
        bool needs_response = true;
        std::vector<uint8_t> payload;
    };

    struct Sensor_Response
    {
        uint8_t type = 0;
        Sensor_Comms::Address address = 0;
        std::vector<uint8_t> payload;
    };

    enum class Result
    {
        Ok,
        Has_Response,
        Data_Error,
        Timeout_Error,
        Connection_Error
    };

    Result send_sensor_message(Sensor_Request const& request, Sensor_Response& response);
    void process();

    std::function<void(data::Server_State)> on_state_requested;

private:
    void init_broadcast();
    void broadcast_thread_func();

    void start_accept();
    void accept_func(asio::error_code ec);

    bool wait_for_message(data::Server_Message expected_message, Clock::duration timeout);
    void process_message(data::Server_Message message);

    void process_change_state_req();
    void process_ping();

    std::string compute_sensor_details_response() const;
    std::string compute_configs_response() const;

    std::thread m_io_service_thread;
    asio::io_service m_io_service;
    std::shared_ptr<asio::io_service::work> m_io_service_work;
    mutable std::recursive_mutex m_mutex;

    uint16_t m_port = 0;
    std::unique_ptr<asio::ip::tcp::acceptor> m_acceptor;
    asio::ip::tcp::socket m_socket;
    bool m_is_connected = false;
    bool m_is_accepting = false;

    using Socket_Adapter = util::comms::ASIO_Socket_Adapter<asio::ip::tcp::socket>;
    Socket_Adapter m_socket_adapter;
    using Channel = util::comms::Channel<data::Server_Message, Socket_Adapter>;
    Channel m_channel;

    uint16_t m_broadcast_port = 0;
    std::thread m_broadcast_thread;
    asio::ip::udp::socket m_broadcast_socket;
    asio::ip::udp::endpoint m_broadcast_endpoint;
    uint8_t m_mac_address[6];

    uint32_t m_last_request_id = 0;

    Clock::time_point m_last_talk_tp = Clock::time_point(Clock::duration::zero());

    std::atomic_bool m_exit = { false };
    data::Server_State m_state = data::Server_State::NORMAL;
};

