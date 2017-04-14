#include "System_DB.h"
#include <iostream>
#include <mysql++/ssqls.h>

sql_create_6(System_DB_Config_Row, 6, 0,
    mysqlpp::sql_varchar, user_db_server,
    mysqlpp::sql_varchar, user_db_name,
    mysqlpp::sql_varchar, user_db_username,
    mysqlpp::sql_varchar, user_db_password,
    mysqlpp::sql_int_unsigned, measurement_period,
    mysqlpp::sql_int_unsigned, comms_period
);


bool System_DB::Sensor::operator==(Sensor const& other) const
{
    return id == other.id &&
            address == other.address &&
            name == other.name;
}
bool System_DB::Sensor::operator!=(Sensor const& other) const
{
    return !operator==(other);
}


System_DB::System_DB()
    : m_connection(false)
{
    System_DB_Config_Row::table("config");
}

bool System_DB::init(std::string const& server, std::string const& db, std::string const& username, std::string const& password)
{
    bool result = m_connection.connect(db.c_str(), server.c_str(), username.c_str(), password.c_str(), 0);
    if (result)
    {
        std::cout << "Connected to DB " << db << " on " << server << "\n";
        return true;
    }

    std::cerr << "Error while connecting to the database: " << m_connection.error() << "\n";
    return false;
}

boost::optional<System_DB::Config> System_DB::get_config()
{
    try
    {
        std::vector<System_DB_Config_Row> rows;

        mysqlpp::Query query = m_connection.query("SELECT * FROM config;");
        query.storein(rows);
        if (query.errnum() != 0)
        {
            std::cerr << "Error executing query: " << query.error() << "\n";
            return boost::none;
        }

        if (rows.size() == 0)
        {
            return boost::none;
        }

        Config config;
        config.user_db_server = rows.front().user_db_server;
        config.user_db_name = rows.front().user_db_name;
        config.user_db_username = rows.front().user_db_username;
        config.user_db_password = rows.front().user_db_password;
        config.measurement_period = std::chrono::seconds(rows.front().measurement_period);
        config.comms_period = std::chrono::seconds(rows.front().comms_period);

        return config;
    }
    catch (const mysqlpp::BadQuery& er)
    {
        std::cerr << "Query error: " << er.what() << std::endl;
        return boost::none;
    }
    catch (const mysqlpp::BadConversion& er)
    {
        // Handle bad conversions
        std::cerr << "Conversion error: " << er.what() << std::endl <<
                "\tretrieved data size: " << er.retrieved <<
                ", actual size: " << er.actual_size << std::endl;
        return boost::none;
    }
    catch (const mysqlpp::Exception& er)
    {
        // Catch-all for any other MySQL++ exceptions
        std::cerr << "Error: " << er.what() << std::endl;
        return boost::none;
    }
}

boost::optional<System_DB::Expected_Sensor> System_DB::get_expected_sensor()
{
    if (!m_connection)
    {
        std::cerr << "Not connected to the database\n";
        return boost::none;
    }

    try
    {
        mysqlpp::Query query = m_connection.query(
                    "SELECT id, name, address "
                    "FROM sensors "
                    "WHERE address IS NULL;");
        mysqlpp::StoreQueryResult result = query.store();
        if (!result)
        {
            std::cerr << "Error executing query: " << query.error() << "\n";
            return boost::none;
        }

        std::vector<Expected_Sensor> sensors;

        for (mysqlpp::StoreQueryResult::const_iterator it = result.begin(); it != result.end(); ++it)
        {
            Expected_Sensor sensor;
            mysqlpp::Row const& row = *it;
            sensor.name = row["name"].c_str();
            sensors.push_back(sensor);
        }

        if (sensors.empty())
        {
            return boost::none;
        }

        return sensors.front();
    }
    catch (const mysqlpp::BadQuery& er)
    {
        std::cerr << "Query error: " << er.what() << std::endl;
        return boost::none;
    }
    catch (const mysqlpp::BadConversion& er)
    {
        // Handle bad conversions
        std::cerr << "Conversion error: " << er.what() << std::endl <<
                "\tretrieved data size: " << er.retrieved <<
                ", actual size: " << er.actual_size << std::endl;
        return boost::none;
    }
    catch (const mysqlpp::Exception& er)
    {
        // Catch-all for any other MySQL++ exceptions
        std::cerr << "Error: " << er.what() << std::endl;
        return boost::none;
    }
}

boost::optional<System_DB::Sensor_Id> System_DB::add_sensor(std::string const& name, Sensor_Address address)
{
    try
    {
        //remove all expected sensors
        {
            mysqlpp::Query query = m_connection.query();
            query << "DELETE FROM sensors WHERE address IS NULL;";
            mysqlpp::SimpleResult result = query.execute();
            if (!result)
            {
                std::cerr << "Error executing query: " << query.error() << "\n";
                return boost::none;
            }
        }
        {
            mysqlpp::Query query = m_connection.query();
            query << "INSERT INTO sensors (name, address) VALUES (\"" << mysqlpp::escape << name << "\"," << address << ");";
            mysqlpp::SimpleResult result = query.execute();
            if (!result)
            {
                std::cerr << "Error executing query: " << query.error() << "\n";
                return boost::none;
            }

            return query.insert_id();
        }
    }
    catch (const mysqlpp::BadQuery& er)
    {
        std::cerr << "Query error: " << er.what() << std::endl;
        return boost::none;
    }
    catch (const mysqlpp::BadConversion& er)
    {
        // Handle bad conversions
        std::cerr << "Conversion error: " << er.what() << std::endl <<
                "\tretrieved data size: " << er.retrieved <<
                ", actual size: " << er.actual_size << std::endl;
        return boost::none;
    }
    catch (const mysqlpp::Exception& er)
    {
        // Catch-all for any other MySQL++ exceptions
        std::cerr << "Error: " << er.what() << std::endl;
        return boost::none;
    }
}

bool System_DB::remove_sensor(Sensor_Id id)
{
    try
    {
        mysqlpp::Query query = m_connection.query();

        query << "DELETE FROM sensors WHERE id=" << id << ";";
        mysqlpp::SimpleResult result = query.execute();
        if (!result)
        {
            std::cerr << "Error executing query: " << query.error() << "\n";
            return false;
        }

        return true;
    }
    catch (const mysqlpp::BadQuery& er)
    {
        std::cerr << "Query error: " << er.what() << std::endl;
        return false;
    }
    catch (const mysqlpp::BadConversion& er)
    {
        // Handle bad conversions
        std::cerr << "Conversion error: " << er.what() << std::endl <<
                "\tretrieved data size: " << er.retrieved <<
                ", actual size: " << er.actual_size << std::endl;
        return false;
    }
    catch (const mysqlpp::Exception& er)
    {
        // Catch-all for any other MySQL++ exceptions
        std::cerr << "Error: " << er.what() << std::endl;
        return false;
    }
}

boost::optional<std::vector<System_DB::Sensor>> System_DB::get_sensors()
{
    if (!m_connection)
    {
        std::cerr << "Not connected to the database\n";
        return boost::none;
    }

    try
    {
        mysqlpp::Query query = m_connection.query(
                    "SELECT id, name, address "
                    "FROM sensors "
                    "WHERE address IS NOT NULL;");
        mysqlpp::StoreQueryResult result = query.store();
        if (!result)
        {
            std::cerr << "Error executing query: " << query.error() << "\n";
            return boost::none;
        }

        std::vector<Sensor> sensors;

        for (mysqlpp::StoreQueryResult::const_iterator it = result.begin(); it != result.end(); ++it)
        {
            Sensor sensor;
            mysqlpp::Row const& row = *it;
            sensor.id = row["id"];
            sensor.name = row["name"].c_str();
            sensor.address = row["address"];
            sensors.push_back(sensor);
        }

        return sensors;
    }
    catch (const mysqlpp::BadQuery& er)
    {
        std::cerr << "Query error: " << er.what() << std::endl;
        return boost::none;
    }
    catch (const mysqlpp::BadConversion& er)
    {
        // Handle bad conversions
        std::cerr << "Conversion error: " << er.what() << std::endl <<
                "\tretrieved data size: " << er.retrieved <<
                ", actual size: " << er.actual_size << std::endl;
        return boost::none;
    }
    catch (const mysqlpp::Exception& er)
    {
        // Catch-all for any other MySQL++ exceptions
        std::cerr << "Error: " << er.what() << std::endl;
        return boost::none;
    }
}

