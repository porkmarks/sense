#pragma once

#define MYSQLPP_MYSQL_HEADERS_BURIED
#include <mysql++/mysql++.h>
#include <boost/optional.hpp>
#include <chrono>

class DB
{
public:
    DB();

    bool init(std::string const& server, std::string const& db, std::string const& username, std::string const& password, uint16_t port);

    struct Expected_Sensor
    {
        std::string name;
    };

    typedef std::chrono::high_resolution_clock Clock;
    typedef uint32_t Sensor_Id;
    typedef uint16_t Sensor_Address;

    struct Sensor
    {
        Sensor_Id id = 0;
        Sensor_Address address = 0;
        std::string name;
        uint32_t max_index = 0;
    };

    boost::optional<Expected_Sensor> get_expected_sensor();

    boost::optional<Sensor_Id> add_sensor(std::string const& name, Sensor_Address address);
    boost::optional<std::vector<Sensor>> get_sensors();

    struct Config
    {
        Clock::duration measurement_period;
        Clock::duration comms_period;
        Clock::time_point baseline_time_point;
    };

    boost::optional<Config> get_config();
    bool set_config(Config const& config);


    struct Measurement
    {
        struct Flag
        {
            enum type : uint8_t
            {
                SENSOR_ERROR   = 1 << 0,
                COMMS_ERROR    = 1 << 1
            };
        };

        uint32_t index = 0;
        float temperature = 0.f;
        float humidity = 0;
        float vcc = 0.f;
        int8_t tx_rssi = 0;
        int8_t rx_rssi = 0;
        uint8_t flags = 0;
    };

    bool add_measurement(Sensor_Id sensor_id, Clock::time_point time_point, Measurement const& measurement);

    void process();

private:
    mysqlpp::Connection m_connection;
};
