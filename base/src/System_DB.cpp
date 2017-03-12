#include "System_DB.h"
#include <iostream>
#include <mysql++/ssqls.h>

sql_create_3(System_DB_Config_Row, 3, 0,
    mysqlpp::sql_int_unsigned, measurement_period,
    mysqlpp::sql_int_unsigned, comms_period,
    mysqlpp::sql_datetime, baseline_timestamp);

sql_create_4(System_DB_Settings_Row, 4, 0,
    mysqlpp::sql_varchar, user_db_server,
    mysqlpp::sql_varchar, user_db_name,
    mysqlpp::sql_varchar, user_db_username,
    mysqlpp::sql_varchar, user_db_password);


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
    System_DB_Config_Row::table("configs");
    System_DB_Settings_Row::table("settings");
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

        mysqlpp::Query query = m_connection.query("SELECT * FROM configs ORDER BY creation_timestamp DESC;");
        query.storein(rows);

        if (rows.size() == 0)
        {
            return boost::none;
        }

        Config config;
        config.measurement_period = std::chrono::seconds(rows.front().measurement_period);
        config.comms_period = std::chrono::seconds(rows.front().comms_period);
        config.baseline_time_point = Clock::time_point(std::chrono::seconds(rows.front().baseline_timestamp));

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

bool System_DB::set_config(Config const& config)
{
    try
    {
        mysqlpp::Query query = m_connection.query();

        System_DB_Config_Row row;
        row.measurement_period = std::chrono::duration_cast<std::chrono::seconds>(config.measurement_period).count();
        row.comms_period = std::chrono::duration_cast<std::chrono::seconds>(config.comms_period).count();
        row.baseline_timestamp = mysqlpp::sql_datetime(Clock::to_time_t(config.baseline_time_point));

        query.insert(row);
        query.execute();

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

boost::optional<System_DB::Settings> System_DB::get_settings()
{
    try
    {
        std::vector<System_DB_Settings_Row> rows;

        mysqlpp::Query query = m_connection.query("SELECT * FROM settings;");
        query.storein(rows);

        if (rows.size() == 0)
        {
            return boost::none;
        }

        Settings settings;
        settings.user_db_server = rows.front().user_db_server;
        settings.user_db_name = rows.front().user_db_name;
        settings.user_db_username = rows.front().user_db_username;
        settings.user_db_password = rows.front().user_db_password;

        return settings;
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
        mysqlpp::StoreQueryResult res = query.store();

        std::vector<Expected_Sensor> sensors;

        for (mysqlpp::StoreQueryResult::const_iterator it = res.begin(); it != res.end(); ++it)
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
            query.execute();
        }
        {
            mysqlpp::Query query = m_connection.query();
            query << "INSERT INTO sensors (name, address) VALUES (\"" << mysqlpp::escape << name << "\"," << address << ");";
            query.execute();

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
        query.execute();

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
        mysqlpp::StoreQueryResult res = query.store();

        std::vector<Sensor> sensors;

        for (mysqlpp::StoreQueryResult::const_iterator it = res.begin(); it != res.end(); ++it)
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

bool System_DB::set_measurement(Sensor_Id sensor_id, Clock::time_point time_point, Measurement const& measurement)
{
    try
    {
        mysqlpp::Query query = m_connection.query();

        query <<    "UPDATE sensors "
                    "SET last_comm_timestamp='" << mysqlpp::sql_datetime(Clock::to_time_t(time_point)) << "', " <<
                    "temperature=" << measurement.temperature << ", " <<
                    "humidity=" << measurement.humidity << ", " <<
                    "vcc=" << measurement.vcc << ", " <<
                    "flags=" << measurement.flags << ", " <<
                    "b2s_input_dBm=" << measurement.b2s_input_dBm << ", " <<
                    "s2b_input_dBm=" << measurement.s2b_input_dBm << " " <<
                    "WHERE id=" << sensor_id << ";";

        std::cout << query.str() << std::endl << std::flush;
        query.execute();

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
