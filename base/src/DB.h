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

    typedef std::chrono::high_resolution_clock Clock;
    typedef uint32_t Sensor_Id;
    typedef uint16_t Sensor_Address;

    struct Expected_Sensor
    {
        Sensor_Id id = 0;
        std::string name;
    };

    struct Sensor
    {
        Sensor_Id id = 0;
        Sensor_Address address = 0;
        std::string name;
    };

    boost::optional<Expected_Sensor> get_expected_sensor();

    bool add_expected_sensor(Sensor_Id id, Sensor_Address address);
    bool revert_to_expected_sensor(Sensor_Id id);
    boost::optional<Sensor_Id> add_sensor(std::string const& name, Sensor_Address address);
    bool remove_sensor(Sensor_Id);
    boost::optional<std::vector<Sensor>> get_valid_sensors();

    struct Measurement_Indices
    {
        std::set<uint32_t> indices;
    };

    boost::optional<std::map<Sensor_Id, Measurement_Indices>> get_all_measurement_indices();

    struct Config
    {
        Clock::duration measurement_period;
        Clock::duration comms_period;
        Clock::time_point start_time_point;
    };

    boost::optional<std::vector<Config>> get_configs();
    bool add_config(Config const& config);


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
        int8_t b2s_input_dBm = 0;
        int8_t s2b_input_dBm = 0;
        uint8_t flags = 0;
    };

    bool add_measurement(Sensor_Id sensor_id, Clock::time_point time_point, Measurement const& measurement);

    void process();

private:
    mysqlpp::Connection m_connection;
};
