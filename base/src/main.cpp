#include <iostream>
#include <sstream>
#include <thread>
#include <chrono>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <pigpio.h>

#include "CRC.h"
#include "Sensor_Comms.h"
#include "Sensors.h"
#include "Server.h"
#include "Log.h"

static Sensor_Comms s_sensor_comms;
static Sensors s_sensors;
static Server s_server(s_sensors);

typedef std::chrono::system_clock Clock;

extern void run_tests();
extern std::chrono::system_clock::time_point string_to_time_point(const std::string& str);
extern std::string time_point_to_string(std::chrono::system_clock::time_point tp);

////////////////////////////////////////////////////////////////////////

void fill_config_packet(data::sensor::Config& packet, Sensors::Calibration const& reported_calibration, Sensors::Sensor const& sensor)
{
    Clock::time_point now = Clock::now();
    packet.next_measurement_delay = chrono::seconds(std::chrono::duration_cast<std::chrono::seconds>(s_sensors.compute_next_measurement_time_point(sensor.id) - now).count());
    packet.measurement_period = chrono::seconds(std::chrono::duration_cast<std::chrono::seconds>(s_sensors.get_config().measurement_period).count());
    packet.next_comms_delay = chrono::seconds(std::chrono::duration_cast<std::chrono::seconds>(s_sensors.compute_next_comms_time_point(sensor.id) - now).count());
    packet.comms_period = chrono::seconds(std::chrono::duration_cast<std::chrono::seconds>(s_sensors.compute_comms_period()).count());
    packet.last_confirmed_measurement_index = s_sensors.compute_last_confirmed_measurement_index(sensor.id);
    packet.calibration_change.temperature_bias = static_cast<int16_t>((sensor.calibration.temperature_bias - reported_calibration.temperature_bias) * 100.f);
    packet.calibration_change.humidity_bias = static_cast<int16_t>((sensor.calibration.humidity_bias - reported_calibration.humidity_bias) * 100.f);

    LOGI << "\tnext measurement delay: " << packet.next_measurement_delay.count
              << "\n\tmeasurement period: " << packet.measurement_period.count
              << "\n\tnext comms delay: " << packet.next_comms_delay.count
              << "\n\tcomms period: " << packet.comms_period.count
              << "\n\tlast confirmed measurement index: " << packet.last_confirmed_measurement_index
              << "\n\ttemperature bias calibration change: " << sensor.calibration.temperature_bias - reported_calibration.temperature_bias
              << "\n\thumidity bias calibration change: " << sensor.calibration.humidity_bias - reported_calibration.humidity_bias
              << "\n";
}

////////////////////////////////////////////////////////////////////////

static void process_sensor_requests(std::chrono::system_clock::duration dt)
{
    std::vector<uint8_t> raw_packet_data(s_sensor_comms.get_payload_raw_buffer_size(Sensor_Comms::MAX_USER_DATA_SIZE));

    uint32_t millis = static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(dt).count());

    uint8_t size = Sensor_Comms::MAX_USER_DATA_SIZE;
    uint8_t* packet_data = s_sensor_comms.receive_packet(raw_packet_data.data(), size, millis);
    if (packet_data)
    {
        data::sensor::Type type = s_sensor_comms.get_rx_packet_type(packet_data);
        if (type == data::sensor::Type::PAIR_REQUEST && size == sizeof(data::sensor::Pair_Request))
        {
            data::sensor::Pair_Request const& pair_request = *reinterpret_cast<data::sensor::Pair_Request const*>(s_sensor_comms.get_rx_packet_payload(packet_data));

            Sensors::Calibration reported_calibration;
            reported_calibration.temperature_bias = static_cast<float>(pair_request.calibration.temperature_bias) / 100.f;
            reported_calibration.humidity_bias = static_cast<float>(pair_request.calibration.humidity_bias) / 100.f;
            uint32_t serial_number = pair_request.serial_number;

            Sensors::Sensor const* sensor = s_sensors.bind_sensor(serial_number, reported_calibration);
            if (sensor)
            {
                LOGI << "Adding sensor " << sensor->name << ", id " << sensor->id << ", address " << sensor->address << ", S/N " << sensor->serial_number << "\n";

                data::sensor::Pair_Response packet;
                packet.address = sensor->address;
                //strncpy(packet.name, sensor->name.c_str(), sizeof(packet.name));
                s_sensor_comms.set_destination_address(s_sensor_comms.get_rx_packet_source_address(packet_data));
                s_sensor_comms.begin_packet(raw_packet_data.data(), data::sensor::Type::PAIR_RESPONSE);
                s_sensor_comms.pack(raw_packet_data.data(), packet);
                if (s_sensor_comms.send_packet(raw_packet_data.data(), 10))
                {
                    LOGI << "Pair successful\n";
                }
                else
                {
                    s_sensors.remove_sensor(sensor->id);
                    LOGE << "Pair failed (comms)\n";
                }
            }
            else
            {
                LOGE << "Pairing failed (db)\n";
            }
        }
        else if (type == data::sensor::Type::MEASUREMENT_BATCH && size == sizeof(data::sensor::Measurement_Batch))
        {
            Sensors::Sensor_Address address = s_sensor_comms.get_rx_packet_source_address(packet_data);
            const Sensors::Sensor* sensor = s_sensors.find_sensor_by_address(address);
            if (sensor)
            {
                data::sensor::Measurement_Batch const& batch = *reinterpret_cast<data::sensor::Measurement_Batch const*>(s_sensor_comms.get_rx_packet_payload(packet_data));

                LOGI << "Measurement batch from " << sensor->id << "\n";
                if (batch.count == 0)
                {
                    LOGI << "\tRange: [empty]\n";
                }
                else if (batch.count == 1)
                {
                    LOGI << "\tRange: [" << batch.start_index << "] : 1 measurement\n";
                }
                else
                {
                    LOGI << "\tRange: [" << batch.start_index << " - " << batch.start_index + batch.count - 1 << "] : " << static_cast<size_t>(batch.count) << " measurements \n";
                }

                std::vector<Sensors::Measurement> measurements;
                size_t count = std::min<size_t>(batch.count, data::sensor::Measurement_Batch::MAX_COUNT);
                for (size_t i = 0; i < count; i++)
                {
                    Sensors::Measurement m;
                    m.index = batch.start_index + i;
                    m.flags = batch.measurements[i].flags;
                    m.b2s_input_dBm = sensor->b2s_input_dBm;
                    m.s2b_input_dBm = s_sensor_comms.get_input_dBm();

                    batch.unpack(m.vcc);
                    batch.measurements[i].unpack(m.humidity, m.temperature);

                    measurements.push_back(m);
                }

                s_sensors.set_sensor_last_comms_time_point(sensor->id, Clock::now());
                s_sensors.report_measurements(sensor->id, measurements);

                //workaround: sleep so the manager can get the measurements and confirm them
                //like this - by the time the sensor asks for the config we have them confirmed
                // proper fix: have the sensor ask for the config 100-200 ms after sending measurements
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
            else
            {
                LOGE << "\tSensor not found!\n";
            }
        }
        else if (type == data::sensor::Type::FIRST_CONFIG_REQUEST && size == sizeof(data::sensor::First_Config_Request))
        {
            Sensors::Sensor_Address address = s_sensor_comms.get_rx_packet_source_address(packet_data);
            const Sensors::Sensor* sensor = s_sensors.find_sensor_by_address(address);
            if (sensor)
            {
                LOGI << "First config for " << sensor->id << "\n";

                data::sensor::First_Config packet;
                packet.first_measurement_index = s_sensors.compute_next_measurement_index();
                s_sensors.set_sensor_measurement_range(sensor->id, packet.first_measurement_index, 0);
                fill_config_packet(packet.config, Sensors::Calibration(), *sensor);

                s_sensor_comms.set_destination_address(s_sensor_comms.get_rx_packet_source_address(packet_data));
                s_sensor_comms.begin_packet(raw_packet_data.data(), data::sensor::Type::FIRST_CONFIG);
                s_sensor_comms.pack(raw_packet_data.data(), packet);
                if (s_sensor_comms.send_packet(raw_packet_data.data(), 3))
                {
                    LOGI << "\tFirst Config successful\n";
                }
                else
                {
                    LOGE << "\tFirst Config failed\n";
                }
            }
            else
            {
                LOGE << "\tSensor not found!\n";
            }
        }
        else if (type == data::sensor::Type::CONFIG_REQUEST && size == sizeof(data::sensor::Config_Request))
        {
            data::sensor::Config_Request const& config_request = *reinterpret_cast<data::sensor::Config_Request const*>(s_sensor_comms.get_rx_packet_payload(packet_data));

            Sensors::Sensor_Address address = s_sensor_comms.get_rx_packet_source_address(packet_data);
            const Sensors::Sensor* sensor = s_sensors.find_sensor_by_address(address);
            if (sensor)
            {
                Sensors::Calibration reported_calibration;
                reported_calibration.temperature_bias = static_cast<float>(config_request.calibration.temperature_bias) / 100.f;
                reported_calibration.humidity_bias = static_cast<float>(config_request.calibration.humidity_bias) / 100.f;

                LOGI << "Config for " << sensor->id << "\n";
                if (config_request.measurement_count == 0)
                {
                    LOGI << "\tStored range: [empty]\n";
                }
                else if (config_request.measurement_count == 1)
                {
                    LOGI << "\tStored range: [" << config_request.first_measurement_index << "] : 1 measurement\n";
                }
                else
                {
                    LOGI << "\tStored range: [" << config_request.first_measurement_index << " - " << config_request.first_measurement_index + config_request.measurement_count - 1 << "] : " << config_request.measurement_count << " measurements \n";
                }

                s_sensors.set_sensor_measurement_range(sensor->id, config_request.first_measurement_index, config_request.measurement_count);
                s_sensors.set_sensor_b2s_input_dBm(sensor->id, config_request.b2s_input_dBm);

                data::sensor::Config packet;
                fill_config_packet(packet, reported_calibration, *sensor);

                s_sensor_comms.set_destination_address(s_sensor_comms.get_rx_packet_source_address(packet_data));
                s_sensor_comms.begin_packet(raw_packet_data.data(), data::sensor::Type::CONFIG);
                s_sensor_comms.pack(raw_packet_data.data(), packet);
                if (s_sensor_comms.send_packet(raw_packet_data.data(), 3))
                {
                    LOGI << "\tSchedule successful\n";
                }
                else
                {
                    LOGE << "\tSchedule failed\n";
                }
            }
            else
            {
                LOGE << "\tSensor not found!\n";
            }
        }
    }
    std::cout << std::flush;
    std::cerr << std::flush;
}

////////////////////////////////////////////////////////////////////////

int main(int, const char**)
{
    LOGI << "Starting...\n";

    srand(time(nullptr));

    gpioInitialise();

    //restart RFM
    {
        gpioSetMode(6, PI_OUTPUT);
        gpioWrite(6, 1);
        std::this_thread::sleep_for(std::chrono::milliseconds(7));
        gpioWrite(6, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    size_t tries = 0;
    while (!s_sensor_comms.init(1, 20))
    {
        tries++;
        LOGE << "comms init failed. Trying again: " << tries << "\n";
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    s_sensor_comms.set_address(Sensor_Comms::BASE_ADDRESS);


    if (!s_server.init(4444, 5555))
    {
        LOGE << "Server init failed\n";
        return -1;
    }

    while (true)
    {
        process_sensor_requests(std::chrono::milliseconds(1000));
        s_server.process();
    }

    return 0;
}

