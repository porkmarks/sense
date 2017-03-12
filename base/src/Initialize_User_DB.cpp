#include <iostream>
#include <sstream>

#include "User_DB.h"
#include "System_DB.h"

static bool check_sensors(std::vector<System_DB::Sensor>& system_sensors, std::vector<User_DB::Sensor>& user_sensors)
{
    if (system_sensors.size() != user_sensors.size())
    {
        return false;
    }

    for (size_t i = 0; i < system_sensors.size(); i++)
    {
        if (user_sensors[i] != system_sensors[i])
        {
            return false;
        }
    }

    return true;
}

static bool has_system_sensor(System_DB::Sensor const& sensor, std::vector<User_DB::Sensor>& user_sensors)
{
    auto it = std::find_if(user_sensors.begin(), user_sensors.end(), [&sensor](User_DB::Sensor const& s) { return s == sensor; });
    return it != user_sensors.end();
}
static bool has_user_sensor(User_DB::Sensor const& sensor, std::vector<System_DB::Sensor>& system_sensors)
{
    auto it = std::find_if(system_sensors.begin(), system_sensors.end(), [&sensor](System_DB::Sensor const& s) { return sensor == s; });
    return it != system_sensors.end();
}

static bool synchronize_user_db(System_DB& system_db, User_DB& user_db)
{
    boost::optional<std::vector<System_DB::Sensor>> opt_system_sensors = system_db.get_sensors();
    if (!opt_system_sensors)
    {
        std::cerr << "Cannot get system sensors\n";
        return false;
    }

    boost::optional<std::vector<User_DB::Sensor>> opt_user_sensors = user_db.get_sensors();
    if (!opt_user_sensors)
    {
        std::cerr << "Cannot get user sensors\n";
        return false;
    }

    std::vector<System_DB::Sensor>& system_sensors = *opt_system_sensors;
    std::vector<User_DB::Sensor>& user_sensors = *opt_user_sensors;

    //sort so we can check faster
    std::sort(system_sensors.begin(), system_sensors.end(), [](System_DB::Sensor const& s1, System_DB::Sensor const& s2) { return s1.id < s2.id; });
    std::sort(user_sensors.begin(), user_sensors.end(), [](User_DB::Sensor const& s1, User_DB::Sensor const& s2) { return s1.id < s2.id; });

    //do a first check
    if (check_sensors(system_sensors, user_sensors))
    {
        return true;
    }

    std::vector<System_DB::Sensor> missing_system_sensors;
    std::vector<User_DB::Sensor> extra_user_sensors;

    for (System_DB::Sensor const& sensor: system_sensors)
    {
        if (!has_system_sensor(sensor, user_sensors))
        {
            missing_system_sensors.push_back(sensor);
        }
    }

    for (User_DB::Sensor const& sensor: user_sensors)
    {
        if (!has_user_sensor(sensor, system_sensors))
        {
            extra_user_sensors.push_back(sensor);
        }
    }

    //now create the missing sensors
    for (System_DB::Sensor const& sensor: missing_system_sensors)
    {
        std::cout << "Adding missing system sensor: ID " << std::to_string(sensor.id) <<
                     ", NAME '" << sensor.name <<
                     "', ADDRESS " << sensor.address << "\n";

        if (!user_db.add_sensor(sensor.id, sensor.name))
        {
            std::cerr << "Cannot add user sensor '" << sensor.name << "'\n";
            return false;
        }
    }

    //now remove the extra sensors
    for (User_DB::Sensor const& sensor: extra_user_sensors)
    {
        std::cout << "Removing extra user sensor: ID " << std::to_string(sensor.id) <<
                     ", NAME '" << sensor.name <<
                     "', LAST INDEX " << sensor.max_confirmed_measurement_index << "\n";

        if (!user_db.remove_sensor(sensor.id))
        {
            std::cerr << "Cannot remove user sensor '" << sensor.name << "'\n";
            return false;
        }
    }

    return true;
}


std::unique_ptr<User_DB> initialize_user_db(System_DB& system_db)
{
    boost::optional<System_DB::Settings> opt_settings = system_db.get_settings();
    if (!opt_settings)
    {
        std::cerr << "System DB doesn't have settings!\n";
        return nullptr;
    }

    std::unique_ptr<User_DB> user_db(new User_DB);
    if (!user_db->init(opt_settings->user_db_server,
                       opt_settings->user_db_name,
                       opt_settings->user_db_username,
                       opt_settings->user_db_password))
    {
        return nullptr;
    }

    if (!synchronize_user_db(system_db, *user_db))
    {
        return nullptr;
    }

    return std::move(user_db);
}
