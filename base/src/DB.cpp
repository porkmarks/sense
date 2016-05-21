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
    mysqlpp::sql_int_unsigned, measurement_period,
    mysqlpp::sql_int_unsigned, comms_period,
    mysqlpp::sql_datetime, baseline_timestamp);



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

boost::optional<DB::Config> DB::get_config()
{
    try
    {
        std::vector<Config_Row> rows;

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

bool DB::set_config(Config const& config)
{
    try
    {
        mysqlpp::Query query = m_connection.query();

        Config_Row row;
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

boost::optional<DB::Expected_Sensor> DB::get_expected_sensor()
{
    Expected_Sensor data;
    data.name = "caca";
    return data;
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

boost::optional<std::vector<DB::Sensor>> DB::get_sensors()
{
    if (!m_connection)
    {
        std::cerr << "Not connected to the database\n";
        return boost::none;
    }

    try
    {
        mysqlpp::Query query = m_connection.query(
                    "SELECT s.id AS id, s.name AS name, s.address AS address, m.max_index as max_index "
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
            sensor.address = row["address"];
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
