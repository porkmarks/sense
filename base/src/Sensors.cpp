#include "Sensors.h"
#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>
#include <cassert>


constexpr std::chrono::seconds CONFIG_REFRESH_PERIOD(10);

std::chrono::high_resolution_clock::time_point string_to_time_point(std::string const& str)
{
    using namespace std;
    using namespace std::chrono;

    int yyyy, mm, dd, HH, MM, SS;

    char scanf_format[] = "%4d-%2d-%2d %2d:%2d:%2d";

    sscanf(str.c_str(), scanf_format, &yyyy, &mm, &dd, &HH, &MM, &SS);

    tm ttm = tm();
    ttm.tm_year = yyyy - 1900; // Year since 1900
    ttm.tm_mon = mm - 1; // Month since January
    ttm.tm_mday = dd; // Day of the month [1-31]
    ttm.tm_hour = HH; // Hour of the day [00-23]
    ttm.tm_min = MM;
    ttm.tm_sec = SS;

    time_t ttime_t = mktime(&ttm);

    high_resolution_clock::time_point time_point_result = std::chrono::high_resolution_clock::from_time_t(ttime_t);

    return time_point_result;
}

std::string time_point_to_string(std::chrono::high_resolution_clock::time_point tp)
{
    using namespace std;
    using namespace std::chrono;

    auto ttime_t = high_resolution_clock::to_time_t(tp);
    //auto tp_sec = high_resolution_clock::from_time_t(ttime_t);
    //milliseconds ms = duration_cast<milliseconds>(tp - tp_sec);

    std::tm * ttm = localtime(&ttime_t);

    char date_time_format[] = "%Y-%m-%d %H:%M:%S";

    char time_str[256];

    strftime(time_str, sizeof(time_str), date_time_format, ttm);

    string result(time_str);
    //result.append(".");
    //result.append(to_string(ms.count()));

    return result;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

const Sensors::Clock::duration Sensors::MEASUREMENT_JITTER = std::chrono::seconds(10);
const Sensors::Clock::duration Sensors::COMMS_DURATION = std::chrono::seconds(10);


Sensors::Sensors()
    : m_main_user_db(new User_DB())
    , m_main_system_db(new System_DB())
{
    m_worker.user_db.reset(new User_DB());
    m_worker.system_db.reset(new System_DB());

    m_sensor_cache.reserve(100);
}

Sensors::~Sensors()
{
    m_worker.stop_request = true;
    if (m_worker.thread.joinable())
    {
        m_worker.thread.join();
    }
}

bool Sensors::init(std::unique_ptr<System_DB> system_db, std::unique_ptr<User_DB> user_db)
{
    std::lock_guard<std::recursive_mutex> lg(m_mutex);

    m_main_system_db = std::move(system_db);
    m_main_user_db = std::move(user_db);

    if (!m_main_system_db || !m_main_user_db)
    {
        std::cerr << "Null database!\n";
        return false;
    }

    if (!m_worker.system_db->init("127.0.0.1", "sense", "sense", "s3ns3"))
    {
        std::cerr << "Failed to initialize worker system DB\n";
        return false;
    }

    if (!m_worker.user_db->init(m_main_user_db->get_server(),
                                m_main_user_db->get_db_name(),
                                m_main_user_db->get_username(),
                                m_main_user_db->get_password()))
    {
        std::cerr << "Failed to initialize worker user DB\n";
        return false;
    }

    boost::optional<System_DB::Config> opt_system_config = m_main_system_db->get_config();
    if (!opt_system_config)
    {
        std::cerr << "Failed to get the system config\n";
        return false;
    }

    boost::optional<User_DB::Config> opt_user_config = m_main_user_db->get_config();
    if (!opt_user_config)
    {
        m_user_config.measurement_period = opt_system_config->measurement_period;
        m_user_config.comms_period = opt_system_config->comms_period;
        m_user_config.baseline_time_point = Clock::now();
        if (!m_main_user_db->set_config(m_user_config))
        {
            std::cerr << "Cannot set config\n";
            return false;
        }
    }
    else
    {
        m_user_config = *opt_user_config;
    }
    m_worker.last_config_refresh_time_point = Clock::now();

    boost::optional<std::vector<System_DB::Sensor>> system_sensors = m_main_system_db->get_sensors();
    if (!system_sensors)
    {
        std::cerr << "Cannot load system sensors\n";
        return false;
    }

    boost::optional<std::vector<User_DB::Sensor>> user_sensors = m_main_user_db->get_sensors();
    if (!user_sensors)
    {
        std::cerr << "Cannot load user sensors\n";
        return false;
    }

    //initialize the sensors from both the system ones and the user ones
    for (System_DB::Sensor const& system_sensor: *system_sensors)
    {
        auto it = std::find_if(user_sensors->begin(), user_sensors->end(), [&system_sensor](User_DB::Sensor const& user_sensor) { return system_sensor.id == user_sensor.id; });
        if (it == user_sensors->end())
        {
            std::cerr << "Cannot find user sensors ID " << system_sensor.id << "\n";
            return false;
        }
        User_DB::Sensor const& user_sensor = *it;

        Sensor sensor;

        sensor.id = system_sensor.id;
        sensor.name = system_sensor.name;
        sensor.address = system_sensor.address;
        sensor.max_confirmed_measurement_index = user_sensor.max_confirmed_measurement_index;

        m_sensor_cache.emplace_back(sensor);
        m_last_address = std::max<uint32_t>(m_last_address, sensor.address);
    }

    m_worker.thread = boost::thread([this]()
    {
        process_worker_thread();
    });

    std::cout << "Sensors initialized\n";

    return true;
}

Sensors::Sensor const* Sensors::add_expected_sensor()
{
    std::lock_guard<std::recursive_mutex> lg(m_mutex);

    boost::optional<System_DB::Expected_Sensor> expected_sensor = m_main_system_db->get_expected_sensor();
    if (!expected_sensor)
    {
        std::cerr << "Not expecting any sensor!\n";
        return nullptr;
    }

    Sensor const* sensor = add_sensor(expected_sensor->name);
    return sensor;
}

void Sensors::set_sensor_measurement_range(Sensor_Id id, uint32_t first_measurement_index, uint32_t measurement_count)
{
    std::lock_guard<std::recursive_mutex> lg(m_mutex);

    Sensor* sensor = _find_sensor_by_id(id);
    if (!sensor)
    {
        std::cerr << "Cannot find sensor id " << id << "\n";
        return;
    }

    sensor->first_recorded_measurement_index = first_measurement_index;
    sensor->recorded_measurement_count = measurement_count;

    if (sensor->first_recorded_measurement_index > 0)
    {
        //we'll never receive measurements lower than sensor->first_recorded_measurement_index
        //so at worst, max_confirmed_measurement_index = sensor->first_recorded_measurement_index - 1 (the one just before the first recorded index)
        sensor->max_confirmed_measurement_index = std::max(sensor->max_confirmed_measurement_index, sensor->first_recorded_measurement_index - 1);
    }
}

void Sensors::set_sensor_b2s_input_dBm(Sensor_Id id, int8_t dBm)
{
    std::lock_guard<std::recursive_mutex> lg(m_mutex);

    Sensor* sensor = _find_sensor_by_id(id);
    if (!sensor)
    {
        std::cerr << "Cannot find sensor id " << id << "\n";
        return;
    }

    sensor->b2s_input_dBm = dBm;
}

Sensors::Sensor const* Sensors::add_sensor(std::string const& name)
{
    std::lock_guard<std::recursive_mutex> lg(m_mutex);

    uint32_t address = ++m_last_address;

    boost::optional<Sensors::Sensor_Id> opt_id = m_main_system_db->add_sensor(name, address);
    if (!opt_id)
    {
        std::cerr << "Failed to add sensor in the system DB\n";
        return nullptr;
    }

    if (!m_main_user_db->add_sensor(*opt_id, name))
    {
        std::cerr << "Failed to add sensor in the user DB\n";
        return nullptr;
    }

    assert(address >= Comms::SLAVE_ADDRESS_BEGIN && address < Comms::SLAVE_ADDRESS_END);
    Sensor sensor;
    sensor.id = *opt_id;
    sensor.name = name;
    sensor.address = address;
    m_sensor_cache.emplace_back(std::move(sensor));

    return &m_sensor_cache.back();
}

bool Sensors::set_comms_period(Clock::duration period)
{
    std::lock_guard<std::recursive_mutex> lg(m_mutex);

    User_DB::Config config = m_user_config;
    config.comms_period = period;
    if (m_main_user_db->set_config(config))
    {
        m_user_config = config;
        return true;
    }
    return false;
}

Sensors::Clock::duration Sensors::compute_comms_period() const
{
    std::lock_guard<std::recursive_mutex> lg(m_mutex);

    Clock::duration max_period = m_sensor_cache.size() * COMMS_DURATION;
    Clock::duration period = std::max(m_user_config.comms_period, max_period);
    return std::max(period, m_user_config.measurement_period + MEASUREMENT_JITTER);
}

bool Sensors::set_measurement_period(Clock::duration period)
{
    std::lock_guard<std::recursive_mutex> lg(m_mutex);

    User_DB::Config config = m_user_config;
    config.measurement_period = period;
    if (m_main_user_db->set_config(config))
    {
        m_user_config = config;
        return true;
    }
    return false;
}
Sensors::Clock::duration Sensors::get_measurement_period() const
{
    std::lock_guard<std::recursive_mutex> lg(m_mutex);
    return m_user_config.measurement_period;
}

Sensors::Clock::time_point Sensors::compute_next_measurement_time_point(Sensor_Id id) const
{
    std::lock_guard<std::recursive_mutex> lg(m_mutex);

    Sensor const* sensor = find_sensor_by_id(id);
    if (!sensor)
    {
        std::cerr << "Cannot find sensor id " << id << "\n";
        return Clock::time_point(Clock::duration::zero());
    }

    uint32_t next_measurement_index = compute_next_measurement_index(id);

    Clock::time_point tp = m_user_config.baseline_time_point + m_user_config.measurement_period * next_measurement_index;
    return tp;
}

uint32_t Sensors::compute_next_measurement_index(Sensor_Id id) const
{
    std::lock_guard<std::recursive_mutex> lg(m_mutex);

    Sensor const* sensor = find_sensor_by_id(id);
    if (!sensor)
    {
        std::cerr << "Cannot find sensor id " << id << "\n";
        return 0;
    }

    uint32_t next_sensor_measurement_index = sensor->first_recorded_measurement_index + sensor->recorded_measurement_count;
    return std::max(next_sensor_measurement_index, compute_next_measurement_index());
}

uint32_t Sensors::compute_next_measurement_index() const
{
    std::lock_guard<std::recursive_mutex> lg(m_mutex);

    auto now = Clock::now();
    uint32_t index = std::ceil((now - m_user_config.baseline_time_point) / m_user_config.measurement_period);
    return index;
}

uint32_t Sensors::compute_last_confirmed_measurement_index(Sensor_Id id) const
{
    std::lock_guard<std::recursive_mutex> lg(m_mutex);

    Sensor const* sensor = find_sensor_by_id(id);
    if (!sensor)
    {
        std::cerr << "Cannot find sensor id " << id << "\n";
        return 0;
    }

    return sensor->max_confirmed_measurement_index;
}

Sensors::Clock::time_point Sensors::compute_next_comms_time_point(Sensor_Id id) const
{
    std::lock_guard<std::recursive_mutex> lg(m_mutex);

    auto it = std::find_if(m_sensor_cache.begin(), m_sensor_cache.end(), [id](Sensor const& sensor)
    {
        return sensor.id == id;
    });
    if (it == m_sensor_cache.end())
    {
        std::cerr << "Cannot find sensor id " << id << "\n";
        return Clock::now() + std::chrono::hours(99999999);
    }

    Clock::duration period = compute_comms_period();

    auto now = Clock::now();
    uint32_t index = std::ceil(((now - m_user_config.baseline_time_point) / period)) + 1;

    Clock::time_point start = m_user_config.baseline_time_point + period * index;
    return start + std::distance(m_sensor_cache.begin(), it) * COMMS_DURATION;
}

bool Sensors::remove_sensor(Sensor_Id id)
{
    std::lock_guard<std::recursive_mutex> lg(m_mutex);

    auto it = std::find_if(m_sensor_cache.begin(), m_sensor_cache.end(), [id](Sensor const& sensor)
    {
        return sensor.id == id;
    });
    if (it == m_sensor_cache.end())
    {
        std::cerr << "Cannot find sensor id " << id << "\n";
        return false;
    }

    if (!m_main_system_db->remove_sensor(id))
    {
        std::cerr << "Failed to remove sensor from the system DB\n";
        return false;
    }
    if (!m_main_user_db->remove_sensor(id))
    {
        std::cerr << "Failed to remove sensor from the user DB\n";
        return false;
    }

    m_sensor_cache.erase(it);
    return true;
}

Sensors::Sensor const* Sensors::find_sensor_by_id(Sensor_Id id) const
{
    std::lock_guard<std::recursive_mutex> lg(m_mutex);

    auto it = std::find_if(m_sensor_cache.begin(), m_sensor_cache.end(), [id](Sensor const& sensor)
    {
        return sensor.id == id;
    });
    if (it == m_sensor_cache.end())
    {
        return nullptr;
    }
    return &(*it);
}

Sensors::Sensor* Sensors::_find_sensor_by_id(Sensor_Id id)
{
    std::lock_guard<std::recursive_mutex> lg(m_mutex);

    auto it = std::find_if(m_sensor_cache.begin(), m_sensor_cache.end(), [id](Sensor const& sensor)
    {
        return sensor.id == id;
    });
    if (it == m_sensor_cache.end())
    {
        return nullptr;
    }
    return &(*it);
}

Sensors::Sensor const* Sensors::find_sensor_by_address(Sensor_Address address) const
{
    std::lock_guard<std::recursive_mutex> lg(m_mutex);

    auto it = std::find_if(m_sensor_cache.begin(), m_sensor_cache.end(), [address](Sensor const& sensor)
    {
        return sensor.address == address;
    });
    if (it == m_sensor_cache.end())
    {
        return nullptr;
    }
    return &(*it);
}

void Sensors::add_measurements(Sensor_Id id, std::vector<Measurement> const& measurements)
{
    std::vector<Worker::Item> items;

    {
        std::lock_guard<std::recursive_mutex> lg(m_mutex);

        Sensor* sensor = _find_sensor_by_id(id);
        if (!sensor)
        {
            std::cerr << "Cannot find sensor id " << id << "\n";
            return;
        }

        for (Measurement const& measurement: measurements)
        {
            if (measurement.index > sensor->max_confirmed_measurement_index)
            {
                Worker::Item item;
                item.id = id;
                item.measurement = measurement;
                item.time_point = m_user_config.baseline_time_point + m_user_config.measurement_period * measurement.index;
                items.push_back(item);
            }
            else
            {
                std::cout << "Sensor " << id << " has reported an already confirmed measurement (" << measurement.index << ")\n";
            }
        }
    }

    if (!items.empty())
    {
        {
            std::lock_guard<std::mutex> lg(m_worker.items_mutex);
            std::copy(items.begin(), items.end(), std::back_inserter(m_worker.items));
        }

        m_worker.items_cv.notify_all();
    }
}

void Sensors::refresh_config()
{
    boost::optional<User_DB::Config> config = m_worker.user_db->get_config();
    if (config)
    {
        std::lock_guard<std::recursive_mutex> lg(m_mutex);
        m_user_config = *config;
    }
    m_worker.last_config_refresh_time_point = Clock::now();
}


void Sensors::process_worker_thread()
{
    std::vector<Worker::Item> items;

    while (!m_worker.stop_request)
    {
        items.clear();

        //make a copy of the items so we don't have to lock the mutex for too long
        {
            std::unique_lock<std::mutex> lg(m_worker.items_mutex);
            m_worker.items_cv.wait(lg, [this] { return m_worker.items.empty() == false; });

            items = std::move(m_worker.items);
            m_worker.items.clear();
        }

        //send them all to the db
        if (!items.empty())
        {
            std::cout << "Sending " << items.size() << " measurements...";

            auto start = Clock::now();
            for (Worker::Item const& item: items)
            {
                if (m_worker.user_db->add_measurement(item.id, item.time_point, item.measurement))
                {
                    std::lock_guard<std::recursive_mutex> lg(m_mutex);

                    Sensor* sensor = _find_sensor_by_id(item.id);
                    if (!sensor)
                    {
                        std::cerr << "Cannot find sensor id " << item.id << "\n";
                        continue;
                    }

                    if (item.measurement.index == sensor->max_confirmed_measurement_index + 1)
                    {
                        sensor->max_confirmed_measurement_index = item.measurement.index;
                    }
                }
            }

            std::cout << "done in " << std::chrono::duration<float>(Clock::now() - start).count() << " seconds\n";
            items.clear();
        }

        if (Clock::now() - m_worker.last_config_refresh_time_point > CONFIG_REFRESH_PERIOD)
        {
            refresh_config();
        }
    }
}

