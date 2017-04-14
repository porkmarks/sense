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
