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
    : m_main_db(new DB())
{
    m_worker.db.reset(new DB());

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

bool Sensors::init(std::string const& server, std::string const& db, std::string const& username, std::string const& password, uint16_t port)
{
    std::lock_guard<std::recursive_mutex> lg(m_mutex);

    if (!m_main_db->init(server, db, username, password, port))
    {
        std::cerr << "Failed to initialize main DB\n";
        return false;
    }

    if (!m_worker.db->init(server, db, username, password, port))
    {
        std::cerr << "Failed to initialize worker DB\n";
        return false;
    }

    boost::optional<DB::Config> config = m_main_db->get_config();
    if (!config)
    {
        m_config.measurement_period = std::chrono::seconds(10);
        m_config.comms_period = m_config.measurement_period * 3;
        m_config.baseline_time_point = Clock::now();
        if (!m_main_db->set_config(m_config))
        {
            std::cerr << "Cannot set config\n";
            return false;
        }
    }
    else
    {
        m_config = *config;
    }
    m_worker.last_config_refresh_time_point = Clock::now();

    m_next_measurement_time_point = m_config.baseline_time_point;
    m_next_comms_time_point = m_config.baseline_time_point;

    boost::optional<std::vector<DB::Sensor>> sensors = m_main_db->get_sensors();
    if (!sensors)
    {
        std::cerr << "Cannot load sensors\n";
        return false;
    }

    for (DB::Sensor const& s: *sensors)
    {
        m_sensor_cache.emplace_back(s);
        m_last_address = std::max<uint32_t>(m_last_address, s.address);
    }

    m_worker.thread = boost::thread([this]()
    {
        process_worker_thread();
    });

    return true;
}

Sensors::Sensor const* Sensors::add_expected_sensor()
{
    std::lock_guard<std::recursive_mutex> lg(m_mutex);

    boost::optional<DB::Expected_Sensor> expected_sensor = m_main_db->get_expected_sensor();
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

    sensor->max_confirmed_measurement_index = std::max(sensor->max_confirmed_measurement_index, sensor->first_recorded_measurement_index);
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

    uint16_t address = ++m_last_address;

    boost::optional<Sensors::Sensor_Id> opt_id = m_main_db->add_sensor(name, address);
    if (!opt_id)
    {
        std::cerr << "Failed to add sensor in the DB\n";
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

    DB::Config config = m_config;
    config.comms_period = period;
    if (m_main_db->set_config(config))
    {
        m_config = config;
        return true;
    }
    return false;
}

Sensors::Clock::duration Sensors::compute_comms_period() const
{
    std::lock_guard<std::recursive_mutex> lg(m_mutex);

    Clock::duration max_period = m_sensor_cache.size() * COMMS_DURATION;
    Clock::duration period = std::max(m_config.comms_period, max_period);
    return std::max(period, m_config.measurement_period + MEASUREMENT_JITTER);
}

bool Sensors::set_measurement_period(Clock::duration period)
{
    std::lock_guard<std::recursive_mutex> lg(m_mutex);

    DB::Config config = m_config;
    config.measurement_period = period;
    if (m_main_db->set_config(config))
    {
        m_config = config;
        return true;
    }
    return false;
}
Sensors::Clock::duration Sensors::get_measurement_period() const
{
    std::lock_guard<std::recursive_mutex> lg(m_mutex);
    return m_config.measurement_period;
}

Sensors::Clock::time_point Sensors::compute_next_measurement_time_point()
{
    std::lock_guard<std::recursive_mutex> lg(m_mutex);

    auto now = Clock::now();
    while (m_next_measurement_time_point <= now)
    {
        m_next_measurement_time_point += m_config.measurement_period;
        m_next_measurement_index++;
    }

    return m_next_measurement_time_point;
}

uint32_t Sensors::compute_next_measurement_index()
{
    std::lock_guard<std::recursive_mutex> lg(m_mutex);

    compute_next_measurement_time_point();
    return m_next_measurement_index;
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
    while (m_next_comms_time_point <= now)
    {
        m_next_comms_time_point += period;
    }

    Clock::time_point start = m_next_comms_time_point;
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

    if (!m_main_db->remove_sensor(id))
    {
        std::cerr << "Failed to remove sensor from the DB\n";
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

void Sensors::add_measurement(Sensor_Id id, Measurement const& measurement)
{
    boost::optional<Worker::Item> item;

    {
        std::lock_guard<std::recursive_mutex> lg(m_mutex);

        Sensor* sensor = _find_sensor_by_id(id);
        if (!sensor)
        {
            std::cerr << "Cannot find sensor id " << id << "\n";
            return;
        }

        if (measurement.index > sensor->max_confirmed_measurement_index)
        {
            item = Worker::Item();
            item->id = id;
            item->measurement = measurement;
            item->time_point = m_config.baseline_time_point + m_config.measurement_period * measurement.index;
        }
        else
        {
            std::cout << "Sensor " << id << " has reported an already confirmed measurement (" << measurement.index << ")\n";
        }
    }

    if (item)
    {
        std::lock_guard<std::recursive_mutex> lg(m_worker.items_mutex);
        m_worker.items.push_back(*item);
    }
}

void Sensors::refresh_config()
{
    boost::optional<DB::Config> config = m_worker.db->get_config();
    if (config)
    {
        std::lock_guard<std::recursive_mutex> lg(m_mutex);
        m_config = *config;
    }
    m_worker.last_config_refresh_time_point = Clock::now();
}


void Sensors::process_worker_thread()
{
    while (!m_worker.stop_request)
    {
        std::vector<Worker::Item> items;

        //make a copy of the items so we don't have to lock the mutex for too long
        {
            std::lock_guard<std::recursive_mutex> lg(m_worker.items_mutex);
            items = std::move(m_worker.items);
            m_worker.items.clear();
        }

        //send them all to the db
        if (!items.empty())
        {
            auto start = Clock::now();
            for (Worker::Item const& item: items)
            {
                if (m_worker.db->add_measurement(item.id, item.time_point, item.measurement))
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
                        sensor->max_confirmed_measurement_index++;
                    }
                }
            }
            std::cout << "Sent " << items.size() << " measurements in " << std::chrono::duration<float>(Clock::now() - start).count() << " seconds\n";
        }
        else
        {
            boost::this_thread::sleep_for(boost::chrono::milliseconds(1000));
        }

        if (Clock::now() - m_worker.last_config_refresh_time_point > CONFIG_REFRESH_PERIOD)
        {
            refresh_config();
        }
    }
}

