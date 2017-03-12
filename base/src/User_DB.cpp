#include "User_DB.h"
#include <iostream>
#include <mysql++/ssqls.h>

sql_create_9(User_DB_Measurement_Row, 9, 0,
             mysqlpp::sql_int_unsigned, sensor_id,
             mysqlpp::sql_int_unsigned, measurement_index,
             mysqlpp::sql_datetime, timestamp,
             mysqlpp::sql_float, temperature,
             mysqlpp::sql_float, humidity,
             mysqlpp::sql_float, vcc,
             mysqlpp::sql_tinyint_unsigned, flags,
             mysqlpp::sql_tinyint, b2s_input_dBm,
             mysqlpp::sql_tinyint, s2b_input_dBm);


bool User_DB::Sensor::operator==(Sensor const& other) const
{
    return id == other.id &&
            name == other.name &&
            max_confirmed_measurement_index == other.max_confirmed_measurement_index;
}
bool User_DB::Sensor::operator!=(Sensor const& other) const
{
    return !operator==(other);
}

bool User_DB::Sensor::operator==(System_DB::Sensor const& other) const
{
    return id == other.id &&
            name == other.name;
}
bool User_DB::Sensor::operator!=(System_DB::Sensor const& other) const
{
    return !operator==(other);
}



User_DB::User_DB()
    : m_connection(false)
{
    User_DB_Measurement_Row::table("measurements");
}

bool User_DB::init(std::string const& server, std::string const& db, std::string const& username, std::string const& password)
{
    bool result = m_connection.connect(db.c_str(), server.c_str(), username.c_str(), password.c_str(), 0);
    if (!result)
    {
        std::cerr << "Error while connecting to the database: " << m_connection.error() << "\n";
        return false;
    }

    m_server = server;
    m_db_name = db;
    m_username = username;
    m_password = password;

    std::cout << "Connected to DB " << db << " on " << server << "\n";
    return true;
}

std::string User_DB::get_server() const
{
    return m_server;
}
std::string User_DB::get_db_name() const
{
    return m_db_name;
}
std::string User_DB::get_username() const
{
    return m_username;
}
std::string User_DB::get_password() const
{
    return m_password;
}

bool User_DB::add_sensor(Sensor_Id id, std::string const& name)
{
    try
    {
        mysqlpp::Query query = m_connection.query();

        query << "INSERT INTO sensors (id, name) VALUES (" << id << ",\"" << mysqlpp::escape << name << "\");";
        query.execute();

        return true;
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

bool User_DB::remove_sensor(Sensor_Id id)
{
    try
    {
        //first delete the measurements
        {
            mysqlpp::Query query = m_connection.query();

            query << "DELETE FROM measurements WHERE sensor_id=" << id << ";";
            query.execute();
        }

        {
            mysqlpp::Query query = m_connection.query();

            query << "DELETE FROM sensors WHERE id=" << id << ";";
            query.execute();
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

boost::optional<std::vector<User_DB::Sensor>> User_DB::get_sensors()
{
    if (!m_connection)
    {
        std::cerr << "Not connected to the database\n";
        return boost::none;
    }

    try
    {
        mysqlpp::Query query = m_connection.query(
                    "SELECT s.id AS id, s.name AS name, m.max_index as max_index "
                    "FROM sensors s "
                    "LEFT JOIN ( "
                    "   SELECT sensor_id, max(measurement_index) AS max_index "
                    "   FROM measurements "
                    "   GROUP BY sensor_id "
                    ") m ON s.id = m.sensor_id;");
        mysqlpp::StoreQueryResult res = query.store();

        std::vector<Sensor> sensors;

        for (mysqlpp::StoreQueryResult::const_iterator it = res.begin(); it != res.end(); ++it)
        {
            Sensor sensor;
            mysqlpp::Row const& row = *it;
            sensor.id = row["id"];
            sensor.name = row["name"].c_str();
            mysqlpp::String max_index = row["max_index"];
            sensor.max_confirmed_measurement_index = max_index.is_null() ? 0 : (uint32_t)max_index;
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

bool User_DB::add_measurement(Sensor_Id sensor_id, Clock::time_point time_point, Measurement const& measurement)
{
    try
    {
        mysqlpp::Query query = m_connection.query();

        User_DB_Measurement_Row row;
        row.sensor_id = sensor_id;
        row.timestamp = mysqlpp::sql_datetime(Clock::to_time_t(time_point));
        row.measurement_index = measurement.index;
        row.temperature = measurement.temperature;
        row.humidity = measurement.humidity;
        row.vcc = measurement.vcc;
        row.flags = measurement.flags;
        row.b2s_input_dBm = measurement.b2s_input_dBm;
        row.s2b_input_dBm = measurement.s2b_input_dBm;

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
