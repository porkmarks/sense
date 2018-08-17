#pragma once

#include "Sensors.h"
#include <asio.hpp>
#include <memory>
#include <atomic>
#include <chrono>
#include <thread>

#include "ASIO_Socket_Adapter.h"
#include "Channel.h"

class Server
{
public:
    Server(Sensors& sensors);
    ~Server();

    bool init(uint16_t comms_port, uint16_t broadcast_port);

    typedef Sensors::Clock Clock;
    typedef Sensors::Sensor_Id Sensor_Id;

    void process();

private:
    void init_broadcast();
    void broadcast_thread_func();

    void start_accept();
    void accept_func(asio::error_code ec);

    void process_get_config_req();
    void process_set_config_req();
    void process_set_sensors_req();
    void process_add_sensor_req();
    void process_remove_sensor_req();
    void process_report_measurement_res();
    void process_sensor_bound_res();
    void process_power_off_req();

    std::string compute_sensor_details_response() const;

    void report_measurement(Sensors::Sensor_Id sensor_id, Clock::time_point time_point, Sensors::Measurement const& measurement);
    void sensor_bound(Sensors::Sensor_Id sensor_id, Sensors::Sensor_Address sensor_address, uint32_t serial_number, Sensors::Calibration const& calibration);
    void sensor_details_changed(Sensors::Sensor_Id sensor_id);

    Sensors& m_sensors;

    std::thread m_io_service_thread;
    asio::io_service m_io_service;
    std::shared_ptr<asio::io_service::work> m_io_service_work;

    uint16_t m_port = 0;
    std::unique_ptr<asio::ip::tcp::acceptor> m_acceptor;
    asio::ip::tcp::socket m_socket;
    bool m_is_connected = false;
    bool m_is_accepting = false;

    typedef util::comms::ASIO_Socket_Adapter<asio::ip::tcp::socket> Socket_Adapter;
    Socket_Adapter m_socket_adapter;
    util::comms::Channel<data::Server_Message, Socket_Adapter> m_channel;

    uint16_t m_broadcast_port = 0;
    std::thread m_broadcast_thread;
    asio::ip::udp::socket m_broadcast_socket;
    asio::ip::udp::endpoint m_broadcast_endpoint;
    uint8_t m_mac_address[6];

    std::atomic_bool m_exit = { false };
};

