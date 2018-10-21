#pragma once

#include <asio.hpp>
#include <memory>
#include <atomic>
#include <chrono>
#include <thread>

#include "ASIO_Socket_Adapter.h"
#include "Channel.h"
#include "Data_Defs.h"

class Server
{
public:
    Server();
    ~Server();

    using Clock = std::chrono::system_clock;

    bool init(uint16_t comms_port, uint16_t broadcast_port);

    struct Sensor_Request
    {
        uint8_t type = 0;
        int8_t signal_s2b = 0;
        uint32_t address = 0;
        std::vector<uint8_t> payload;
    };

    struct Sensor_Response
    {
        uint8_t type = 0;
        uint32_t address = 0;
        uint8_t retries = 0;
        std::vector<uint8_t> payload;
    };

    bool send_sensor_message(Sensor_Request const& request, Sensor_Response& response);
    void process();

private:
    void init_broadcast();
    void broadcast_thread_func();

    void start_accept();
    void accept_func(asio::error_code ec);

    bool wait_for_message(data::Server_Message expected_message, Clock::duration timeout);
    void process_message(data::Server_Message message);

    void process_get_config_req();
    void process_add_config_req();
    void process_set_configs_req();
    void process_set_sensors_req();
    void process_add_sensor_req();
    void process_remove_sensor_req();
    void process_report_measurements_res();
    void process_sensor_bound_res();
    void process_power_off_req();

    std::string compute_sensor_details_response() const;
    std::string compute_configs_response() const;

    std::thread m_io_service_thread;
    asio::io_service m_io_service;
    std::shared_ptr<asio::io_service::work> m_io_service_work;

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

    std::atomic_bool m_exit = { false };
};

