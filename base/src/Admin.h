#pragma once

#include <chrono>
#include <vector>
#include <map>
#include <string>
#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include <mutex>

#include "Sensors_DB.h"

class Admin
{
public:
    Admin(Sensors_DB& sensors_db);
    ~Admin();

    bool init();

//    std::string const& get_db_server() const;
//    std::string const& get_db_name() const;
//    std::string const& get_db_username() const;
//    std::string const& get_db_password() const;
//    uint16_t get_db_port() const;

private:
    Sensors_DB& m_sensors_db;

    std::string m_gateway_name;
    std::string m_password;

    std::string m_db_server;
    std::string m_db_name;
    std::string m_db_username;
    std::string m_db_password;
    uint16_t m_db_port = 0;

    bool load_config();
    bool save_config() const;
};

