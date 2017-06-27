#include "Sensors.h"
#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>
#include <cassert>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

constexpr std::chrono::seconds CONFIG_REFRESH_PERIOD(10);

///////////////////////////////////////////////////////////////////////////////////////////

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

///////////////////////////////////////////////////////////////////////////////////////////

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

static const std::string k_settings_file = "settings.json";

///////////////////////////////////////////////////////////////////////////////////////////

Sensors::Sensors()
{
    m_sensors.reserve(100);
}

///////////////////////////////////////////////////////////////////////////////////////////

Sensors::~Sensors()
{
}

///////////////////////////////////////////////////////////////////////////////////////////

bool Sensors::init()
{
    if (!load_settings(k_settings_file))
    {
        m_config.measurement_period = std::chrono::minutes(5);
        m_config.comms_period = std::chrono::minutes(5);
        m_config.baseline_time_point = Clock::now();

        m_sensors.clear();
    }

    save_settings(k_settings_file);

    std::cout << "Sensors initialized\n";

    m_is_initialized = true;

    return true;
}

///////////////////////////////////////////////////////////////////////////////////////////

bool Sensors::is_initialized() const
{
    return m_is_initialized && cb_report_measurement != nullptr && cb_sensor_bound != nullptr;
}

///////////////////////////////////////////////////////////////////////////////////////////

void Sensors::set_unbound_sensor_data(Unbound_Sensor_Data const& data)
{
    if (!is_initialized())
    {
        std::cerr << "Sensors not initialized\n";
        return;
    }

    m_unbound_sensor_data_opt = data;
}

///////////////////////////////////////////////////////////////////////////////////////////

boost::optional<Sensors::Unbound_Sensor_Data> Sensors::get_unbound_sensor_data() const
{
    if (!is_initialized())
    {
        std::cerr << "Sensors not initialized\n";
        return boost::none;
    }
    return m_unbound_sensor_data_opt;
}

///////////////////////////////////////////////////////////////////////////////////////////

void Sensors::confirm_sensor_binding(Sensor_Id id, bool confirmed)
{
    if (!confirmed)
    {
        remove_sensor(id);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////

Sensors::Sensor const* Sensors::bind_sensor()
{
    if (!is_initialized())
    {
        std::cerr << "Sensors not initialized\n";
        return nullptr;
    }

    if (!m_unbound_sensor_data_opt.is_initialized())
    {
        std::cerr << "Not expecting any sensor!\n";
        return nullptr;
    }

    Sensor const* sensor = add_sensor(m_unbound_sensor_data_opt->id, m_unbound_sensor_data_opt->name, ++m_last_address);
    if (sensor)
    {
        cb_sensor_bound(sensor->id, sensor->address);
        m_unbound_sensor_data_opt = boost::none;
    }

    return sensor;
}

///////////////////////////////////////////////////////////////////////////////////////////

void Sensors::set_sensor_measurement_range(Sensor_Id id, uint32_t first_measurement_index, uint32_t measurement_count)
{
    if (!is_initialized())
    {
        std::cerr << "Sensors not initialized\n";
        return;
    }

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

///////////////////////////////////////////////////////////////////////////////////////////

void Sensors::set_sensor_b2s_input_dBm(Sensor_Id id, int8_t dBm)
{
    if (!is_initialized())
    {
        std::cerr << "Sensors not initialized\n";
        return;
    }

    Sensor* sensor = _find_sensor_by_id(id);
    if (!sensor)
    {
        std::cerr << "Cannot find sensor id " << id << "\n";
        return;
    }

    sensor->b2s_input_dBm = dBm;
}

///////////////////////////////////////////////////////////////////////////////////////////

Sensors::Sensor const* Sensors::add_sensor(Sensor_Id id, std::string const& name, Sensor_Address address)
{
    if (!is_initialized())
    {
        std::cerr << "Sensors not initialized\n";
        return nullptr;
    }

    if (address == 0)
    {
        std::cerr << "Invalid sensor address\n";
        return nullptr;
    }

    assert(address >= Sensor_Comms::SLAVE_ADDRESS_BEGIN && address < Sensor_Comms::SLAVE_ADDRESS_END);
    Sensor sensor;
    sensor.id = id;
    sensor.name = name;
    sensor.address = address;
    m_sensors.emplace_back(std::move(sensor));

    save_settings(k_settings_file);

    return &m_sensors.back();
}

///////////////////////////////////////////////////////////////////////////////////////////

std::vector<Sensors::Sensor> Sensors::get_sensors() const
{
    if (!is_initialized())
    {
        std::cerr << "Sensors not initialized\n";
        return {};
    }

    return m_sensors;
}

///////////////////////////////////////////////////////////////////////////////////////////

void Sensors::set_config(Config const& config)
{
    if (!is_initialized())
    {
        std::cerr << "Sensors not initialized\n";
        return;
    }

    m_config = config;

    save_settings(k_settings_file);
}

///////////////////////////////////////////////////////////////////////////////////////////

Sensors::Config const& Sensors::get_config()
{
    return m_config;
}

///////////////////////////////////////////////////////////////////////////////////////////

Sensors::Clock::duration Sensors::compute_comms_period() const
{
    if (!is_initialized())
    {
        std::cerr << "Sensors not initialized\n";
        return Clock::duration::zero();
    }


    Clock::duration max_period = m_sensors.size() * COMMS_DURATION;
    Clock::duration period = std::max(m_config.comms_period, max_period);
    return std::max(period, m_config.measurement_period + MEASUREMENT_JITTER);
}

///////////////////////////////////////////////////////////////////////////////////////////

Sensors::Clock::time_point Sensors::compute_next_measurement_time_point(Sensor_Id id) const
{
    if (!is_initialized())
    {
        std::cerr << "Sensors not initialized\n";
        return Clock::time_point(Clock::duration::zero());
    }

    Sensor const* sensor = find_sensor_by_id(id);
    if (!sensor)
    {
        std::cerr << "Cannot find sensor id " << id << "\n";
        return Clock::time_point(Clock::duration::zero());
    }

    uint32_t next_measurement_index = compute_next_measurement_index(id);

    Clock::time_point tp = m_config.baseline_time_point + m_config.measurement_period * next_measurement_index;
    return tp;
}

///////////////////////////////////////////////////////////////////////////////////////////

uint32_t Sensors::compute_next_measurement_index(Sensor_Id id) const
{
    if (!is_initialized())
    {
        std::cerr << "Sensors not initialized\n";
        return 0u;
    }

    Sensor const* sensor = find_sensor_by_id(id);
    if (!sensor)
    {
        std::cerr << "Cannot find sensor id " << id << "\n";
        return 0;
    }

    uint32_t next_sensor_measurement_index = sensor->first_recorded_measurement_index + sensor->recorded_measurement_count;
    return std::max(next_sensor_measurement_index, compute_next_measurement_index());
}

///////////////////////////////////////////////////////////////////////////////////////////

uint32_t Sensors::compute_next_measurement_index() const
{
    if (!is_initialized())
    {
        std::cerr << "Sensors not initialized\n";
        return 0u;
    }

    auto now = Clock::now();
    uint32_t index = std::ceil((now - m_config.baseline_time_point) / m_config.measurement_period);
    return index;
}

///////////////////////////////////////////////////////////////////////////////////////////

uint32_t Sensors::compute_last_confirmed_measurement_index(Sensor_Id id) const
{
    if (!is_initialized())
    {
        std::cerr << "Sensors not initialized\n";
        return 0u;
    }

    Sensor const* sensor = find_sensor_by_id(id);
    if (!sensor)
    {
        std::cerr << "Cannot find sensor id " << id << "\n";
        return 0;
    }

    return sensor->max_confirmed_measurement_index;
}

///////////////////////////////////////////////////////////////////////////////////////////

Sensors::Clock::time_point Sensors::compute_next_comms_time_point(Sensor_Id id) const
{
    if (!is_initialized())
    {
        std::cerr << "Sensors not initialized\n";
        return Clock::time_point(Clock::duration::zero());
    }

    auto it = std::find_if(m_sensors.begin(), m_sensors.end(), [id](Sensor const& sensor)
    {
        return sensor.id == id;
    });
    if (it == m_sensors.end())
    {
        std::cerr << "Cannot find sensor id " << id << "\n";
        return Clock::now() + std::chrono::hours(99999999);
    }

    Clock::duration period = compute_comms_period();

    auto now = Clock::now();
    uint32_t index = std::ceil(((now - m_config.baseline_time_point) / period)) + 1;

    Clock::time_point start = m_config.baseline_time_point + period * index;
    return start + std::distance(m_sensors.begin(), it) * COMMS_DURATION;
}

///////////////////////////////////////////////////////////////////////////////////////////

bool Sensors::remove_sensor(Sensor_Id id)
{
    if (!is_initialized())
    {
        std::cerr << "Sensors not initialized\n";
        return false;
    }


    auto it = std::find_if(m_sensors.begin(), m_sensors.end(), [id](Sensor const& sensor)
    {
        return sensor.id == id;
    });
    if (it == m_sensors.end())
    {
        std::cerr << "Cannot find sensor id " << id << "\n";
        return false;
    }

    m_sensors.erase(it);

    save_settings(k_settings_file);

    return true;
}

///////////////////////////////////////////////////////////////////////////////////////////

Sensors::Sensor const* Sensors::find_sensor_by_id(Sensor_Id id) const
{
    if (!is_initialized())
    {
        std::cerr << "Sensors not initialized\n";
        return nullptr;
    }

    auto it = std::find_if(m_sensors.begin(), m_sensors.end(), [id](Sensor const& sensor)
    {
        return sensor.id == id;
    });
    if (it == m_sensors.end())
    {
        return nullptr;
    }
    return &(*it);
}

///////////////////////////////////////////////////////////////////////////////////////////

Sensors::Sensor* Sensors::_find_sensor_by_id(Sensor_Id id)
{
    if (!is_initialized())
    {
        std::cerr << "Sensors not initialized\n";
        return nullptr;
    }

    auto it = std::find_if(m_sensors.begin(), m_sensors.end(), [id](Sensor const& sensor)
    {
        return sensor.id == id;
    });
    if (it == m_sensors.end())
    {
        return nullptr;
    }
    return &(*it);
}

///////////////////////////////////////////////////////////////////////////////////////////

Sensors::Sensor const* Sensors::find_sensor_by_address(Sensor_Address address) const
{
    if (!is_initialized())
    {
        std::cerr << "Sensors not initialized\n";
        return nullptr;
    }

    auto it = std::find_if(m_sensors.begin(), m_sensors.end(), [address](Sensor const& sensor)
    {
        return sensor.address == address;
    });
    if (it == m_sensors.end())
    {
        return nullptr;
    }
    return &(*it);
}

///////////////////////////////////////////////////////////////////////////////////////////

void Sensors::report_measurements(Sensor_Id id, std::vector<Measurement> const& measurements)
{
    if (!is_initialized())
    {
        std::cerr << "Sensors not initialized\n";
        return;
    }

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
            cb_report_measurement(id, m_config.baseline_time_point + m_config.measurement_period * measurement.index, measurement);
        }
        else
        {
            std::cout << "Sensor " << id << " has reported an already confirmed measurement (" << measurement.index << ")\n";
        }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////

void Sensors::confirm_measurement(Sensor_Id id, uint32_t measurement_index)
{
    if (!is_initialized())
    {
        std::cerr << "Sensors not initialized\n";
        return;
    }

    Sensor* sensor = _find_sensor_by_id(id);
    if (!sensor)
    {
        std::cerr << "Cannot find sensor id " << id << "\n";
        return;
    }

    if (measurement_index == sensor->max_confirmed_measurement_index + 1)
    {
        sensor->max_confirmed_measurement_index = measurement_index;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////

bool Sensors::load_settings(std::string const& filename)
{
    m_sensors.clear();
    {
        std::ifstream f(filename);
        if (!f.is_open())
        {
            return false;
        }
    }

    try
    {
        boost::property_tree::ptree pt;
        boost::property_tree::read_json(filename, pt);

        for (auto const& node: pt.get_child("sensors"))
        {
            Sensor sensor;
            sensor.name = node.first;
            sensor.address = node.second.get<Sensor_Address>("address");
            sensor.id = node.second.get<Sensor_Id>("id");
            m_sensors.push_back(sensor);
        }

        m_config.sensors_sleeping = pt.get<bool>("sensors_sleeping");
        m_config.measurement_period = std::chrono::seconds(pt.get<std::chrono::seconds::rep>("measurement_period"));
        m_config.comms_period = std::chrono::seconds(pt.get<std::chrono::seconds::rep>("comms_period"));
        m_config.baseline_time_point = Clock::time_point(std::chrono::seconds(pt.get<std::chrono::seconds::rep>("baseline_time_point")));
    }
    catch (std::exception const& e)
    {
        std::cerr << "Cannot deserialize settings: " << e.what() << "\n";
        return false;
    }

    return true;
}

///////////////////////////////////////////////////////////////////////////////////////////

bool Sensors::save_settings(std::string const& filename)
{
    try
    {
        boost::property_tree::ptree pt;

        for (Sensor const& sensor: m_sensors)
        {
            auto node = pt.add("sensors." + sensor.name, "");
            node.add("address", sensor.name);
            node.add("id", sensor.id);
        }

        pt.add("sensors_sleeping", m_config.sensors_sleeping);
        pt.add("measurement_period", std::chrono::duration_cast<std::chrono::seconds>(m_config.measurement_period).count());
        pt.add("comms_period", std::chrono::duration_cast<std::chrono::seconds>(m_config.comms_period).count());
        pt.add("baseline_time_point", std::chrono::duration_cast<std::chrono::seconds>(m_config.baseline_time_point.time_since_epoch()).count());

        boost::property_tree::write_json(filename, pt);
        sync();
    }
    catch (std::exception const& e)
    {
        std::cerr << "Cannot deserialize request: " << e.what() << "\n";
        return false;
    }

    return true;
}
