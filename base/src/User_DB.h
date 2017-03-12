#pragma once

#define MYSQLPP_MYSQL_HEADERS_BURIED
#include <mysql++/mysql++.h>
#include <boost/optional.hpp>
#include <chrono>
#include "System_DB.h"


class User_DB
{
public:
    User_DB();

    bool init(std::string const& server, std::string const& db, std::string const& username, std::string const& password);

    std::string get_server() const;
    std::string get_db_name() const;
    std::string get_username() const;
    std::string get_password() const;

    typedef std::chrono::high_resolution_clock Clock;
    typedef System_DB::Sensor_Id Sensor_Id;

    struct Sensor
    {
        Sensor_Id id = 0;
        std::string name;

        uint32_t max_confirmed_measurement_index = 0;

        bool operator==(Sensor const& other) const;
        bool operator!=(Sensor const& other) const;
        bool operator==(System_DB::Sensor const& other) const;
        bool operator!=(System_DB::Sensor const& other) const;
    };

    boost::optional<std::vector<Sensor>> get_sensors();

    bool add_sensor(Sensor_Id id, std::string const& name);
    bool remove_sensor(Sensor_Id);

    struct Measurement : public System_DB::Measurement
    {
        uint32_t index = 0;
    };

    bool add_measurement(Sensor_Id sensor_id, Clock::time_point time_point, Measurement const& measurement);

private:
    mysqlpp::Connection m_connection;
    std::string m_server;
    std::string m_db_name;
    std::string m_username;
    std::string m_password;
};
