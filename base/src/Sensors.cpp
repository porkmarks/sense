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

    m_sensors.reserve(100);
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

    //////////////////////////////////////////////////////////////////////////////////////////////////
    //get the config
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


    //////////////////////////////////////////////////////////////////////////////////////////////////
    //create the sensors
    boost::optional<std::vector<DB::Sensor>> sensors = m_main_db->get_valid_sensors();
    if (!sensors)
    {
        std::cerr << "Cannot load sensors\n";
        return false;
    }

    for (DB::Sensor const& s: *sensors)
    {
        Sensor_Data sensor_data;
        sensor_data.sensor = s;
        m_sensors.emplace_back(sensor_data);
        std::cout << "Adding sensor " << s.id << ", address " << s.address << " name " << s.name << "\n";
        m_last_address = std::max<uint32_t>(m_last_address, s.address);
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////
    // Download the indices and sompute the last confirmed one
    boost::optional<std::map<Sensor_Id, DB::Measurement_Indices>> indices = m_main_db->get_all_measurement_indices();
    if (!indices)
    {
        std::cerr << "Cannot load indices\n";
        return false;
    }

    for (auto& pair: *indices)
    {
        Sensor_Id id = pair.first;
        DB::Measurement_Indices& indices = pair.second;
        Sensor_Data* sensor_data = _find_sensor_data_by_id(id);
        if (!sensor_data)
        {
            continue;
        }
        sensor_data->measurement_indices = std::move(indices.indices);
        size_t db_index_count = sensor_data->measurement_indices.size();
        sensor_data->max_confirmed_measurement_index = remove_unused_measurement_indices(*sensor_data);
        size_t crt_index_count = sensor_data->measurement_indices.size();

        std::cout << "Sensor " << id << " has " << db_index_count << " indices in DB, " << (db_index_count - crt_index_count) << " unconfirmed. Last confirmed one is " << sensor_data->max_confirmed_measurement_index << "\n";
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////
    //start the worker thread
    m_worker.thread = boost::thread([this]()
    {
        process_worker_thread();
    });

    return true;
}


uint32_t Sensors::remove_unused_measurement_indices(Sensor_Data& sensor_data)
{
    if (sensor_data.measurement_indices.empty())
    {
        return 0;
    }

    uint32_t last_index = 0;
    while (!sensor_data.measurement_indices.empty())
    {
        uint32_t index = *sensor_data.measurement_indices.begin();
        if (last_index != 0 && last_index + 1 != index)
        {
            break;
        }
        last_index = index;
        sensor_data.measurement_indices.erase(sensor_data.measurement_indices.begin());
    }

    return last_index;
}

void Sensors::remove_confirmed_indices(Sensor_Data& sensor_data, uint32_t max_confirmed)
{
    while (!sensor_data.measurement_indices.empty() && *sensor_data.measurement_indices.begin() <= max_confirmed)
    {
        sensor_data.measurement_indices.erase(sensor_data.measurement_indices.begin());
    }
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

    uint16_t address = ++m_last_address;
    assert(address >= Comms::SLAVE_ADDRESS_BEGIN && address < Comms::SLAVE_ADDRESS_END);

    bool res = m_main_db->add_expected_sensor(expected_sensor->id, address);
    if (!res)
    {
        std::cerr << "Failed to add expected sensor " << expected_sensor->name << " in the DB\n";
        return nullptr;
    }

    Sensor_Data sensor_data;
    sensor_data.sensor.id = expected_sensor->id;
    sensor_data.sensor.name = expected_sensor->name;
    sensor_data.sensor.address = address;
    m_sensors.emplace_back(sensor_data);

    return &m_sensors.back().sensor;
}

void Sensors::set_sensor_measurement_range(Sensor_Id id, uint32_t first_measurement_index, uint32_t measurement_count)
{
    std::lock_guard<std::recursive_mutex> lg(m_mutex);

    Sensor_Data* sensor_data = _find_sensor_data_by_id(id);
    if (!sensor_data)
    {
        std::cerr << "Cannot find sensor id " << id << "\n";
        return;
    }

    sensor_data->first_recorded_measurement_index = first_measurement_index;
    sensor_data->recorded_measurement_count = measurement_count;

    sensor_data->max_confirmed_measurement_index = std::max(sensor_data->max_confirmed_measurement_index, sensor_data->first_recorded_measurement_index);

    //remove already confirmed indices
    remove_confirmed_indices(*sensor_data, sensor_data->max_confirmed_measurement_index);
}

void Sensors::set_sensor_b2s_input_dBm(Sensor_Id id, int8_t dBm)
{
    std::lock_guard<std::recursive_mutex> lg(m_mutex);

    Sensor_Data* sensor_data = _find_sensor_data_by_id(id);
    if (!sensor_data)
    {
        std::cerr << "Cannot find sensor id " << id << "\n";
        return;
    }

    sensor_data->b2s_input_dBm = dBm;
}

Sensors::Sensor const* Sensors::add_sensor(std::string const& name)
{
    std::lock_guard<std::recursive_mutex> lg(m_mutex);

    uint16_t address = ++m_last_address;
    assert(address >= Comms::SLAVE_ADDRESS_BEGIN && address < Comms::SLAVE_ADDRESS_END);

    boost::optional<Sensors::Sensor_Id> opt_id = m_main_db->add_sensor(name, address);
    if (!opt_id)
    {
        std::cerr << "Failed to add sensor in the DB\n";
        return nullptr;
    }

    Sensor_Data sensor_data;
    sensor_data.sensor.id = *opt_id;
    sensor_data.sensor.name = name;
    sensor_data.sensor.address = address;
    m_sensors.emplace_back(sensor_data);

    return &m_sensors.back().sensor;
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

    Clock::duration max_period = m_sensors.size() * COMMS_DURATION;
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

    Sensor_Data const* sensor_data = _find_sensor_data_by_id(id);
    if (!sensor_data)
    {
        std::cerr << "Cannot find sensor id " << id << "\n";
        return 0;
    }

    return sensor_data->max_confirmed_measurement_index;
}

Sensors::Clock::time_point Sensors::compute_next_comms_time_point(Sensor_Id id) const
{
    std::lock_guard<std::recursive_mutex> lg(m_mutex);

    auto it = std::find_if(m_sensors.begin(), m_sensors.end(), [id](Sensor_Data const& sensor_data)
    {
        return sensor_data.sensor.id == id;
    });
    if (it == m_sensors.end())
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
    return start + std::distance(m_sensors.begin(), it) * COMMS_DURATION;
}

bool Sensors::remove_sensor(Sensor_Id id)
{
    std::lock_guard<std::recursive_mutex> lg(m_mutex);

    auto it = std::find_if(m_sensors.begin(), m_sensors.end(), [id](Sensor_Data const& sensor_data)
    {
        return sensor_data.sensor.id == id;
    });
    if (it == m_sensors.end())
    {
        std::cerr << "Cannot find sensor id " << id << "\n";
        return false;
    }

    if (!m_main_db->remove_sensor(id))
    {
        std::cerr << "Failed to remove sensor from the DB\n";
        return false;
    }

    m_sensors.erase(it);
    return true;
}

bool Sensors::revert_to_expected_sensor(Sensor_Id id)
{
    std::lock_guard<std::recursive_mutex> lg(m_mutex);

    auto it = std::find_if(m_sensors.begin(), m_sensors.end(), [id](Sensor_Data const& sensor_data)
    {
        return sensor_data.sensor.id == id;
    });
    if (it == m_sensors.end())
    {
        std::cerr << "Cannot find sensor id " << id << "\n";
        return false;
    }

    if (!m_main_db->revert_to_expected_sensor(id))
    {
        std::cerr << "Failed to revert to expected sensor from the DB\n";
        return false;
    }

    m_sensors.erase(it);
    return true;
}

Sensors::Sensor const* Sensors::find_sensor_by_id(Sensor_Id id) const
{
    std::lock_guard<std::recursive_mutex> lg(m_mutex);

    auto it = std::find_if(m_sensors.begin(), m_sensors.end(), [id](Sensor_Data const& sensor_data)
    {
        return sensor_data.sensor.id == id;
    });
    if (it == m_sensors.end())
    {
        return nullptr;
    }
    return &(it->sensor);
}

Sensors::Sensor_Data* Sensors::_find_sensor_data_by_id(Sensor_Id id)
{
    std::lock_guard<std::recursive_mutex> lg(m_mutex);

    auto it = std::find_if(m_sensors.begin(), m_sensors.end(), [id](Sensor_Data const& sensor_data)
    {
        return sensor_data.sensor.id == id;
    });
    if (it == m_sensors.end())
    {
        return nullptr;
    }
    return &(*it);
}

Sensors::Sensor_Data const* Sensors::_find_sensor_data_by_id(Sensor_Id id) const
{
    std::lock_guard<std::recursive_mutex> lg(m_mutex);

    auto it = std::find_if(m_sensors.begin(), m_sensors.end(), [id](Sensor_Data const& sensor_data)
    {
        return sensor_data.sensor.id == id;
    });
    if (it == m_sensors.end())
    {
        return nullptr;
    }
    return &(*it);
}

Sensors::Sensor const* Sensors::find_sensor_by_address(Sensor_Address address) const
{
    std::lock_guard<std::recursive_mutex> lg(m_mutex);

    auto it = std::find_if(m_sensors.begin(), m_sensors.end(), [address](Sensor_Data const& sensor_data)
    {
        return sensor_data.sensor.address == address;
    });
    if (it == m_sensors.end())
    {
        return nullptr;
    }
    return &(it->sensor);
}

void Sensors::add_measurement(Sensor_Id id, Measurement const& measurement)
{
    boost::optional<Worker::Item> item;

    {
        std::lock_guard<std::recursive_mutex> lg(m_mutex);

        Sensor_Data* sensor_data = _find_sensor_data_by_id(id);
        if (!sensor_data)
        {
            std::cerr << "Cannot find sensor id " << id << "\n";
            return;
        }


        if (sensor_data->measurement_indices.find(measurement.index) == sensor_data->measurement_indices.end())
        {
            item = Worker::Item();
            item->sensor_id = id;
            item->measurement = measurement;
            item->measurement.b2s_input_dBm = sensor_data->b2s_input_dBm;
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

        //sort them in index order
        std::sort(items.begin(), items.end(), [](Worker::Item const& a, Worker::Item const& b)
        {
            return a.measurement.index < b.measurement.index;
        });

        //send them all to the db
        if (!items.empty())
        {
            auto start = Clock::now();
            for (Worker::Item const& item: items)
            {
                if (m_worker.db->add_measurement(item.sensor_id, item.time_point, item.measurement))
                {
                    std::lock_guard<std::recursive_mutex> lg(m_mutex);

                    Sensor_Data* sensor_data = _find_sensor_data_by_id(item.sensor_id);
                    if (!sensor_data)
                    {
                        std::cerr << "Cannot find sensor id " << item.sensor_id << "\n";
                        continue;
                    }

                    sensor_data->measurement_indices.insert(item.measurement.index);
                    if (item.measurement.index == sensor_data->max_confirmed_measurement_index + 1)
                    {
                        sensor_data->max_confirmed_measurement_index = item.measurement.index;
                    }

                    remove_confirmed_indices(*sensor_data, sensor_data->max_confirmed_measurement_index);
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

