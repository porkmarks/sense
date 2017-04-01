#pragma once

#define MYSQLPP_MYSQL_HEADERS_BURIED
#include <mysql++/mysql++.h>
#include <boost/optional.hpp>
#include <chrono>

class System_DB
{
public:
    System_DB();

    bool init(std::string const& server, std::string const& db, std::string const& username, std::string const& password);

    struct Expected_Sensor
    {
        std::string name;
    };

    typedef std::chrono::high_resolution_clock Clock;
    typedef uint32_t Sensor_Id;
    typedef uint32_t Sensor_Address;

    struct Sensor
    {
        Sensor_Id id = 0;
        Sensor_Address address = 0;
        std::string name;

        bool operator==(Sensor const& other) const;
        bool operator!=(Sensor const& other) const;
    };

    boost::optional<Expected_Sensor> get_expected_sensor();

    boost::optional<Sensor_Id> add_sensor(std::string const& name, Sensor_Address address);
    bool remove_sensor(Sensor_Id);
    boost::optional<std::vector<Sensor>> get_sensors();

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

        float temperature = 0.f;
        float humidity = 0;
        float vcc = 0.f;
        int8_t b2s_input_dBm = 0;
        int8_t s2b_input_dBm = 0;
        uint8_t flags = 0;
    };

    bool set_measurement(Sensor_Id sensor_id, Clock::time_point time_point, Measurement const& measurement);

    struct Config
    {
        std::string user_db_server;
        std::string user_db_name;
        std::string user_db_username;
        std::string user_db_password;
        Clock::duration measurement_period;
        Clock::duration comms_period;
    };

    boost::optional<Config> get_config();

private:
    mysqlpp::Connection m_connection;
};
