#include "Admin.h"
#include <boost/asio.hpp>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

#include <sys/utsname.h>

Admin::Admin(Sensors_DB& sensors_db)
    : m_sensors_db(sensors_db)
    , m_gateway_name("Gateway")
    , m_password("admin")
{
    utsname buf;
    if (uname(&buf) == 0)
    {
        m_gateway_name = std::string("TempR: ") + buf.sysname + " " + buf.nodename +  " " + buf.release + " " + buf.machine;
    }
}

Admin::~Admin()
{

}

bool Admin::init()
{
    load_config();
    save_config();

    if (!m_sensors_db.init(m_db_server, m_db_name, m_db_username, m_db_password, m_db_port))
    {
        std::cerr << "Cannot initialize sensors db" << std::endl;
    }

    return true;
}

bool Admin::load_config()
{
    namespace pt = boost::property_tree;

    try
    {
        pt::ptree tree;
        pt::read_xml("config.xml", tree);

        m_gateway_name = tree.get<std::string>("gateway_name", m_gateway_name);
        m_password = tree.get<std::string>("password", m_password);
        m_db_server = tree.get<std::string>("db.server", std::string());
        m_db_name = tree.get<std::string>("db.name", std::string());
        m_db_username = tree.get<std::string>("db.username", std::string());
        m_db_password = tree.get<std::string>("db.password", std::string());
        m_db_port = tree.get<uint16_t>("db.port", 0);
    }
    catch(std::exception const& e)
    {
        std::cerr << "Exception while loading config: " << e.what() << std::endl;
        return false;
    }

    return true;
}

bool Admin::save_config() const
{
    namespace pt = boost::property_tree;

    try
    {
        pt::ptree tree;

        tree.put("gateway_name", m_gateway_name);
        tree.put("password", m_password);
        tree.put("db.server", m_db_server);
        tree.put("db.name", m_db_name);
        tree.put("db.username", m_db_username);
        tree.put("db.password", m_db_password);
        tree.put("db.port", m_db_port);

        pt::write_xml("config.xml", tree);
    }
    catch(std::exception const& e)
    {
        std::cerr << "Exception while saving config: " << e.what() << std::endl;
        return false;
    }

    return true;
}

//std::string const& Admin::get_db_server() const
//{
//    return m_db_server;
//}
//std::string const& Admin::get_db_name() const
//{
//    return m_db_name;
//}
//std::string const& Admin::get_db_username() const
//{
//    return m_db_username;
//}
//std::string const& Admin::get_db_password() const
//{
//    return m_db_password;
//}
//uint16_t Admin::get_db_port() const
//{
//    return m_db_port;
//}
