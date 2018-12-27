#include "Sensors.h"
#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>
#include <cassert>

#include "Log.h"

///////////////////////////////////////////////////////////////////////////////////////////

std::chrono::system_clock::time_point string_to_time_point(std::string const& str)
{
    using namespace std;
    using namespace std::chrono;

    int yyyy, mm, dd, HH, MM, SS;

    sscanf(str.c_str(), "%4d-%2d-%2d %2d:%2d:%2d", &yyyy, &mm, &dd, &HH, &MM, &SS);

    tm ttm = tm();
    ttm.tm_year = yyyy - 1900; // Year since 1900
    ttm.tm_mon = mm - 1; // Month since January
    ttm.tm_mday = dd; // Day of the month [1-31]
    ttm.tm_hour = HH; // Hour of the day [00-23]
    ttm.tm_min = MM;
    ttm.tm_sec = SS;

    time_t ttime_t = mktime(&ttm);

    system_clock::time_point time_point_result = std::chrono::system_clock::from_time_t(ttime_t);

    return time_point_result;
}

///////////////////////////////////////////////////////////////////////////////////////////

std::string time_point_to_string(std::chrono::system_clock::time_point tp)
{
    using namespace std;
    using namespace std::chrono;

    auto ttime_t = system_clock::to_time_t(tp);
    auto tp_sec = system_clock::from_time_t(ttime_t);
    milliseconds ms = duration_cast<milliseconds>(tp - tp_sec);

    std::tm * ttm = localtime(&ttime_t);

    char date_time_format[] = "%Y-%m-%d %H:%M:%S";

    char time_str[256];
    strftime(time_str, sizeof(time_str), date_time_format, ttm);
    string result(time_str);

    char ms_str[256];
    sprintf(ms_str, ".%03d", ms.count());
    result.append(ms_str);

    return result;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

const Sensors::Clock::duration Sensors::MEASUREMENT_JITTER = std::chrono::seconds(10);
const Sensors::Clock::duration Sensors::COMMS_DURATION = std::chrono::seconds(10);

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
    LOGI << "Sensors initialized" << std::endl;

    m_is_initialized = true;
    return true;
}

///////////////////////////////////////////////////////////////////////////////////////////

bool Sensors::is_initialized() const
{
    return m_is_initialized && cb_report_measurements != nullptr && cb_sensor_bound != nullptr;
}

///////////////////////////////////////////////////////////////////////////////////////////

void Sensors::set_unbound_sensor_data(Unbound_Sensor_Data const& data)
{
    if (!is_initialized())
    {
        LOGE << "Sensors not initialized" << std::endl;
        return;
    }

    m_unbound_sensor_data_opt = data;
}

///////////////////////////////////////////////////////////////////////////////////////////

boost::optional<Sensors::Unbound_Sensor_Data> Sensors::get_unbound_sensor_data() const
{
    if (!is_initialized())
    {
        LOGE << "Sensors not initialized" << std::endl;
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

Sensors::Sensor const* Sensors::bind_sensor(uint32_t serial_number, Sensors::Calibration const& calibration)
{
    if (!is_initialized())
    {
        LOGE << "Sensors not initialized" << std::endl;
        return nullptr;
    }

    if (!m_unbound_sensor_data_opt.is_initialized())
    {
        LOGE << "Not expecting any sensor!" << std::endl;
        return nullptr;
    }

    Sensor const* sensor = add_sensor(m_unbound_sensor_data_opt->id, m_unbound_sensor_data_opt->name, ++m_last_address, serial_number, calibration);
    if (sensor)
    {
        cb_sensor_bound(sensor->id, sensor->address, sensor->serial_number, sensor->calibration);
        m_unbound_sensor_data_opt = boost::none;
    }

    return sensor;
}

///////////////////////////////////////////////////////////////////////////////////////////

void Sensors::set_sensor_calibration(Sensor_Id id, Calibration const& calibration)
{
    if (!is_initialized())
    {
        LOGE << "Sensors not initialized" << std::endl;
        return;
    }

    Sensor* sensor = _find_sensor_by_id(id);
    if (!sensor)
    {
        LOGE << "Cannot find sensor id " << id << std::endl;
        return;
    }

    sensor->calibration = calibration;
}

///////////////////////////////////////////////////////////////////////////////////////////

void Sensors::set_sensor_measurement_range(Sensor_Id id, uint32_t first_measurement_index, uint32_t measurement_count)
{
    if (!is_initialized())
    {
        LOGE << "Sensors not initialized" << std::endl;
        return;
    }

    Sensor* sensor = _find_sensor_by_id(id);
    if (!sensor)
    {
        LOGE << "Cannot find sensor id " << id << std::endl;
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
        LOGE << "Sensors not initialized" << std::endl;
        return;
    }

    Sensor* sensor = _find_sensor_by_id(id);
    if (!sensor)
    {
        LOGE << "Cannot find sensor id " << id << std::endl;
        return;
    }

    sensor->b2s_input_dBm = dBm;
}

///////////////////////////////////////////////////////////////////////////////////////////

void Sensors::set_sensor_sleeping(Sensor_Id id, bool sleeping)
{
    if (!is_initialized())
    {
        LOGE << "Sensors not initialized" << std::endl;
        return;
    }

    Sensor* sensor = _find_sensor_by_id(id);
    if (!sensor)
    {
        LOGE << "Cannot find sensor id " << id << std::endl;
        return;
    }

    sensor->sleeping = sleeping;
}

///////////////////////////////////////////////////////////////////////////////////////////

void Sensors::set_sensor_last_comms_time_point(Sensor_Id id, Clock::time_point tp)
{
    if (!is_initialized())
    {
        LOGE << "Sensors not initialized" << std::endl;
        return;
    }

    Sensor* sensor = _find_sensor_by_id(id);
    if (!sensor)
    {
        LOGE << "Cannot find sensor id " << id << std::endl;
        return;
    }

    sensor->last_comms_tp = tp;
}

///////////////////////////////////////////////////////////////////////////////////////////

void Sensors::notify_sensor_details_changed(Sensor_Id id)
{
    if (!is_initialized())
    {
        LOGE << "Sensors not initialized" << std::endl;
        return;
    }

    Sensor* sensor = _find_sensor_by_id(id);
    if (!sensor)
    {
        LOGE << "Cannot find sensor id " << id << std::endl;
        return;
    }

    if (cb_sensor_details_changed)
    {
        cb_sensor_details_changed(id);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////

Sensors::Clock::time_point Sensors::get_sensor_last_comms_time_point(Sensor_Id id) const
{
    if (!is_initialized())
    {
        LOGE << "Sensors not initialized" << std::endl;
        return Clock::time_point(Clock::duration::zero());
    }

    const Sensor* sensor = find_sensor_by_id(id);
    if (!sensor)
    {
        LOGE << "Cannot find sensor id " << id << std::endl;
        return Clock::time_point(Clock::duration::zero());
    }

    return sensor->last_comms_tp;
}

///////////////////////////////////////////////////////////////////////////////////////////

Sensors::Sensor const* Sensors::add_sensor(Sensor_Id id, std::string const& name, Sensor_Address address, uint32_t serial_number, Calibration const& calibration)
{
    if (!is_initialized())
    {
        LOGE << "Sensors not initialized" << std::endl;
        return nullptr;
    }

    if (address == 0)
    {
        LOGE << "Invalid sensor address" << std::endl;
        return nullptr;
    }

    assert(address >= Sensor_Comms::SLAVE_ADDRESS_BEGIN && address < Sensor_Comms::SLAVE_ADDRESS_END);
    Sensor sensor;
    sensor.id = id;
    sensor.name = name;
    sensor.address = address;
    sensor.calibration = calibration;
    sensor.serial_number = serial_number;
    m_sensors.emplace_back(std::move(sensor));

    m_last_address = std::max(m_last_address, sensor.address);

    return &m_sensors.back();
}

///////////////////////////////////////////////////////////////////////////////////////////

std::vector<Sensors::Sensor> Sensors::get_sensors() const
{
    if (!is_initialized())
    {
        LOGE << "Sensors not initialized" << std::endl;
        return {};
    }

    return m_sensors;
}

///////////////////////////////////////////////////////////////////////////////////////////

void Sensors::set_config(Config config)
{
    if (!is_initialized())
    {
        LOGE << "Sensors not initialized" << std::endl;
        return;
    }

    if (config.baseline_measurement_index == 0 && config.baseline_measurement_time_point.time_since_epoch() == Clock::duration::zero())
    {
        LOGI << "Config requires baseline calculations" << std::endl;

        uint32_t next_measurement_index = 0;
        Clock::time_point next_measurement_tp = Clock::now();

        if (!m_configs.empty())
        {
            next_measurement_index = compute_next_measurement_index();

            Config c = find_config_for_measurement_index(next_measurement_index);
            next_measurement_tp = c.baseline_measurement_time_point + c.measurement_period * (next_measurement_index - c.baseline_measurement_index);

            m_configs.erase(std::remove_if(m_configs.begin(), m_configs.end(), [&next_measurement_index](Config const& c)
            {
                return c.baseline_measurement_index == next_measurement_index;
            }), m_configs.end());
        }

        config.baseline_measurement_index = next_measurement_index;
        config.baseline_measurement_time_point = next_measurement_tp;
    }
    LOGI << "Config baseline calculations: index = " << config.baseline_measurement_index << std::endl;

    m_configs.push_back(config);
    while (m_configs.size() > 100)
    {
        m_configs.pop_front();
    }
}

///////////////////////////////////////////////////////////////////////////////////////////

Sensors::Config Sensors::get_config() const
{
    if (!m_configs.empty())
    {
        return m_configs.back();
    }
    return Config();
}

///////////////////////////////////////////////////////////////////////////////////////////

std::vector<Sensors::Config> Sensors::get_configs() const
{
    std::vector<Config> configs;
    configs.resize(m_configs.size());
    std::copy(m_configs.begin(), m_configs.end(), configs.begin());
    return configs;
}

///////////////////////////////////////////////////////////////////////////////////////////

void Sensors::remove_all_configs()
{
    m_configs.clear();
}

///////////////////////////////////////////////////////////////////////////////////////////

Sensors::Config Sensors::find_config_for_measurement_index(uint32_t measurement_index) const
{
    for (auto it = m_configs.rbegin(); it != m_configs.rend(); ++it)
    {
        Config const& c = *it;
        if (measurement_index >= c.baseline_measurement_index)
        {
            return c;
        }
    }
    return get_config();
}

///////////////////////////////////////////////////////////////////////////////////////////

Sensors::Clock::duration Sensors::compute_comms_period() const
{
    if (!is_initialized())
    {
        LOGE << "Sensors not initialized" << std::endl;
        return Clock::duration::zero();
    }

    Config config = get_config();

    Clock::duration max_period = m_sensors.size() * COMMS_DURATION;
    Clock::duration period = std::max(config.comms_period, max_period);
    return std::max(period, config.measurement_period + MEASUREMENT_JITTER);
}

///////////////////////////////////////////////////////////////////////////////////////////

uint32_t Sensors::compute_baseline_measurement_index() const
{
    //check for sleeping configs
    if (!m_configs.empty())
    {
        //find the last non-sleeping config
        auto sleeping_rit = m_configs.rend();
        for (auto rit = m_configs.rbegin(); rit != m_configs.rend(); ++rit)
        {
            Config const& c = *rit;
            if (c.sensors_sleeping)
            {
                sleeping_rit = rit;
                break;
            }
        }

        //not the last config?
        if (sleeping_rit != m_configs.rbegin())
        {
            Config const& last_non_sleeping_config = *(sleeping_rit - 1);
            return last_non_sleeping_config.baseline_measurement_index;
        }
        return sleeping_rit->baseline_measurement_index;
    }

    return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////

Sensors::Clock::time_point Sensors::compute_next_measurement_time_point(Sensor_Id id) const
{
    if (!is_initialized())
    {
        LOGE << "Sensors not initialized" << std::endl;
        return Clock::time_point(Clock::duration::zero());
    }

    Sensor const* sensor = find_sensor_by_id(id);
    if (!sensor)
    {
        LOGE << "Cannot find sensor id " << id << std::endl;
        return Clock::time_point(Clock::duration::zero());
    }

    uint32_t next_measurement_index = compute_next_measurement_index(id);

    Config config = find_config_for_measurement_index(next_measurement_index);

    Clock::time_point tp = config.baseline_measurement_time_point + config.measurement_period * (next_measurement_index - config.baseline_measurement_index);
    return tp;
}

///////////////////////////////////////////////////////////////////////////////////////////

uint32_t Sensors::compute_next_measurement_index(Sensor_Id id) const
{
    if (!is_initialized())
    {
        LOGE << "Sensors not initialized" << std::endl;
        return 0u;
    }

    Sensor const* sensor = find_sensor_by_id(id);
    if (!sensor)
    {
        LOGE << "Cannot find sensor id " << id << std::endl;
        return 0;
    }

    uint32_t next_sensor_measurement_index = sensor->first_recorded_measurement_index + sensor->recorded_measurement_count;

    //make sure the next measurement index doesn't fall below the last non-sleeping config
    next_sensor_measurement_index = std::max(next_sensor_measurement_index, compute_baseline_measurement_index());

    return next_sensor_measurement_index;
}

///////////////////////////////////////////////////////////////////////////////////////////

uint32_t Sensors::compute_next_measurement_index() const
{
    if (!is_initialized())
    {
        LOGE << "Sensors not initialized" << std::endl;
        return 0u;
    }

    Config config = get_config();

    auto now = Clock::now();
    uint32_t index = static_cast<uint32_t>(std::ceil((now - config.baseline_measurement_time_point) / config.measurement_period)) + config.baseline_measurement_index;
    return index;
}

///////////////////////////////////////////////////////////////////////////////////////////

uint32_t Sensors::compute_last_confirmed_measurement_index(Sensor_Id id) const
{
    if (!is_initialized())
    {
        LOGE << "Sensors not initialized" << std::endl;
        return 0u;
    }

    Sensor const* sensor = find_sensor_by_id(id);
    if (!sensor)
    {
        LOGE << "Cannot find sensor id " << id << std::endl;
        return 0;
    }

    return sensor->max_confirmed_measurement_index;
}

///////////////////////////////////////////////////////////////////////////////////////////

Sensors::Clock::time_point Sensors::compute_next_comms_time_point(Sensor_Id id) const
{
    if (!is_initialized())
    {
        LOGE << "Sensors not initialized" << std::endl;
        return Clock::time_point(Clock::duration::zero());
    }

    auto it = std::find_if(m_sensors.begin(), m_sensors.end(), [id](Sensor const& sensor)
    {
        return sensor.id == id;
    });
    if (it == m_sensors.end())
    {
        LOGE << "Cannot find sensor id " << id << std::endl;
        return Clock::now() + std::chrono::hours(99999999);
    }

    Config config = get_config();

    Clock::duration period = compute_comms_period();

    auto now = Clock::now();
    uint32_t index = static_cast<uint32_t>(std::ceil(((now - config.baseline_measurement_time_point) / period))) + 1;

    Clock::time_point start = config.baseline_measurement_time_point + period * index;
    return start + std::distance(m_sensors.begin(), it) * COMMS_DURATION;
}

///////////////////////////////////////////////////////////////////////////////////////////

bool Sensors::remove_sensor(Sensor_Id id)
{
    if (!is_initialized())
    {
        LOGE << "Sensors not initialized" << std::endl;
        return false;
    }

    auto it = std::find_if(m_sensors.begin(), m_sensors.end(), [id](Sensor const& sensor)
    {
        return sensor.id == id;
    });
    if (it == m_sensors.end())
    {
        LOGE << "Cannot find sensor id " << id << std::endl;
        return false;
    }

    m_sensors.erase(it);

    return true;
}

///////////////////////////////////////////////////////////////////////////////////////////

void Sensors::remove_all_sensors()
{
    if (!is_initialized())
    {
        LOGE << "Sensors not initialized" << std::endl;
        return;
    }

    m_sensors.clear();
}

///////////////////////////////////////////////////////////////////////////////////////////

Sensors::Sensor const* Sensors::find_sensor_by_id(Sensor_Id id) const
{
    if (!is_initialized())
    {
        LOGE << "Sensors not initialized" << std::endl;
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
        LOGE << "Sensors not initialized" << std::endl;
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
        LOGE << "Sensors not initialized" << std::endl;
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
        LOGE << "Sensors not initialized" << std::endl;
        return;
    }

    Sensor* sensor = _find_sensor_by_id(id);
    if (!sensor)
    {
        LOGE << "Cannot find sensor id " << id << std::endl;
        return;
    }

    std::vector<Reported_Measurement> rms;
    rms.reserve(measurements.size());
    for (Measurement const& measurement: measurements)
    {
        if (measurement.index > sensor->max_confirmed_measurement_index)
        {
            Config config = find_config_for_measurement_index(measurement.index);
            if (config.sensors_sleeping)
            {
                LOGI << "Sensor " << id << " has reported measurement " << measurement.index << " when it should have been SLEEPING. Discarded." << std::endl;
                if (measurement.index == sensor->max_confirmed_measurement_index + 1)
                {
                    sensor->max_confirmed_measurement_index = measurement.index;
                }
            }
            else
            {
                Clock::time_point tp = config.baseline_measurement_time_point + config.measurement_period * (measurement.index - config.baseline_measurement_index);
                LOGI << "Sensor " << id << " has reported measurement " << measurement.index << " from " << time_point_to_string(tp) << std::endl;
                rms.push_back({id, tp, measurement});
            }
        }
        else
        {
            LOGI << "Sensor " << id << " has reported an already confirmed measurement (" << measurement.index << ")" << std::endl;
        }
    }

    if (!rms.empty())
    {
        cb_report_measurements(rms);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////

void Sensors::confirm_measurement(Sensor_Id id, uint32_t measurement_index)
{
    if (!is_initialized())
    {
        LOGE << "Sensors not initialized" << std::endl;
        return;
    }

    Sensor* sensor = _find_sensor_by_id(id);
    if (!sensor)
    {
        LOGE << "Cannot find sensor id " << id << std::endl;
        return;
    }

    if (measurement_index == sensor->max_confirmed_measurement_index + 1)
    {
        sensor->max_confirmed_measurement_index = measurement_index;
    }
}
