#include "DB.h"
#include <iostream>
#include <mysql++/ssqls.h>

sql_create_3(Sensor_Row, 3, 0,
    mysqlpp::sql_int_unsigned, id,
    mysqlpp::sql_varchar, name,
    mysqlpp::sql_int_unsigned, address);

sql_create_9(Measurement_Row, 9, 0,
             mysqlpp::sql_int_unsigned, sensor_id,
             mysqlpp::sql_int_unsigned, measurement_index,
             mysqlpp::sql_datetime, timestamp,
             mysqlpp::sql_float, temperature,
             mysqlpp::sql_float, humidity,
             mysqlpp::sql_float, vcc,
             mysqlpp::sql_tinyint_unsigned, flags,
             mysqlpp::sql_tinyint, b2s_input_dBm,
             mysqlpp::sql_tinyint, s2b_input_dBm);

sql_create_3(Config_Row, 3, 0,
    mysqlpp::sql_datetime, creation_timestamp,
    mysqlpp::sql_int_unsigned, measurement_period,
    mysqlpp::sql_int_unsigned, comms_period);



DB::DB()
    : m_connection(false)
{
    Sensor_Row::table("sensors");
    Measurement_Row::table("measurements");
    Config_Row::table("configs");
}

bool DB::init(std::string const& server, std::string const& db, std::string const& username, std::string const& password, uint16_t port)
{
    bool result = m_connection.connect(db.c_str(), server.c_str(), username.c_str(), password.c_str(), port);
    if (result)
    {
        std::cout << "Connected to DB " << db << " on " << server << "\n";
        return true;
    }

    std::cerr << "Error while connecting to the database: " << m_connection.error() << "\n";
    return false;
}

boost::optional<std::vector<DB::Config>> DB::get_configs()
{
    try
    {
        std::vector<Config_Row> rows;

        mysqlpp::Query query = m_connection.query("SELECT * FROM configs ORDER BY creation_timestamp ASC;");
        query.storein(rows);

        if (rows.size() == 0)
        {
            return boost::none;
        }

        std::vector<Config> configs;
        for (Config_Row const& row: rows)
        {
            Config config;
            config.measurement_period = std::chrono::seconds(row.measurement_period);
            config.creation_time_point = Clock::time_point(std::chrono::seconds(row.creation_timestamp));
            config.comms_period = std::chrono::seconds(row.comms_period);
            configs.push_back(config);
        }

        return configs;
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

bool DB::add_config(Config const& config)
{
    try
    {
        mysqlpp::Query query = m_connection.query();

        query << "INSERT INTO configs (measurement_period, comms_period) VALUES (" <<
                 std::chrono::duration_cast<std::chrono::seconds>(config.measurement_period).count() << "," <<
                 std::chrono::duration_cast<std::chrono::seconds>(config.comms_period).count() << ");";
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

boost::optional<DB::Expected_Sensor> DB::get_expected_sensor()
{
    if (!m_connection)
    {
        std::cerr << "Not connected to the database\n";
        return boost::none;
    }

    try
    {
        mysqlpp::Query query = m_connection.query(
                    "SELECT s.id AS id, s.name AS name "
                    "FROM sensors s "
                    "WHERE s.is_valid = 0 "
                    "ORDER BY s.creation_timestamp DESC;");
        mysqlpp::StoreQueryResult res = query.store();

        for (mysqlpp::StoreQueryResult::const_iterator it = res.begin(); it != res.end(); ++it)
        {
            Expected_Sensor sensor;
            mysqlpp::Row const& row = *it;
            sensor.id = row["id"];
            sensor.name = row["name"].c_str();
            return sensor;
        }

        return boost::none;
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

bool DB::add_expected_sensor(Sensor_Id id, Sensor_Address address)
{
    try
    {
        mysqlpp::Query query = m_connection.query();

        query << "UPDATE sensors SET address=" << address << ", is_valid=1 WHERE id=" << id << ";";
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


bool DB::revert_to_expected_sensor(Sensor_Id id)
{
    try
    {
        mysqlpp::Query query = m_connection.query();

        query << "UPDATE sensors SET is_valid=0 WHERE id=" << id << ";";
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

boost::optional<DB::Sensor_Id> DB::add_sensor(std::string const& name, Sensor_Address address)
{
    try
    {
        mysqlpp::Query query = m_connection.query();

        query << "INSERT INTO sensors (name, address) VALUES (\"" << mysqlpp::escape << name << "\"," << address << ");";
        query.execute();

        return query.insert_id();
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

bool DB::remove_sensor(Sensor_Id id)
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

boost::optional<std::vector<DB::Sensor>> DB::get_valid_sensors()
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
                    "WHERE is_valid != 0;");
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

bool DB::add_measurement(Sensor_Id sensor_id, Clock::time_point time_point, Measurement const& measurement)
{
    try
    {
        mysqlpp::Query query = m_connection.query();

        Measurement_Row row;
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

boost::optional<std::map<DB::Sensor_Id, DB::Measurement_Indices>> DB::get_all_measurement_indices()
{
    if (!m_connection)
    {
        std::cerr << "Not connected to the database\n";
        return boost::none;
    }

    try
    {
        mysqlpp::Query query = m_connection.query(
                    "SELECT sensor_id, measurement_index "
                    "FROM measurements "
                    "ORDER BY sensor_id ASC;");
        mysqlpp::StoreQueryResult res = query.store();

        std::map<Sensor_Id, Measurement_Indices> indices;

        for (mysqlpp::StoreQueryResult::const_iterator it = res.begin(); it != res.end(); ++it)
        {
            mysqlpp::Row const& row = *it;
            Sensor_Id sensor_id = row["sensor_id"];
            uint32_t index = row["measurement_index"];
            indices[sensor_id].indices.insert(index);
        }

        return indices;
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
