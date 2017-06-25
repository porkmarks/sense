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
#include <boost/thread.hpp>
#include <pigpio.h>

#include "CRC.h"
#include "Comms.h"
#include "Sensors.h"
#include "Server.h"

#include "rfm22b/rfm22b.h"


#define LOG(x) std::cout << x
#define LOG_LN(x) std::cout << x << std::endl

static Comms s_sensor_comms;
static Sensors s_sensors;
static Server s_server(s_sensors);

std::stringstream s_cerr_buffer;

typedef std::chrono::high_resolution_clock Clock;

extern void run_tests();
extern std::chrono::high_resolution_clock::time_point string_to_time_point(const std::string& str);
extern std::string time_point_to_string(std::chrono::high_resolution_clock::time_point tp);

////////////////////////////////////////////////////////////////////////

void fill_config_packet(data::Config& packet, Sensors::Sensor const& sensor)
{
    Clock::time_point now = Clock::now();
    packet.next_measurement_delay = chrono::seconds(std::chrono::duration_cast<std::chrono::seconds>(s_sensors.compute_next_measurement_time_point(sensor.id) - now).count());
    packet.measurement_period = chrono::seconds(std::chrono::duration_cast<std::chrono::seconds>(s_sensors.get_config().measurement_period).count());
    packet.next_comms_delay = chrono::seconds(std::chrono::duration_cast<std::chrono::seconds>(s_sensors.compute_next_comms_time_point(sensor.id) - now).count());
    packet.comms_period = chrono::seconds(std::chrono::duration_cast<std::chrono::seconds>(s_sensors.compute_comms_period()).count());
    packet.last_confirmed_measurement_index = s_sensors.compute_last_confirmed_measurement_index(sensor.id);

    std::cout << "\tnext measurement delay: " << packet.next_measurement_delay.count
              << "\n\tmeasurement period: " << packet.measurement_period.count
              << "\n\tnext comms delay: " << packet.next_comms_delay.count
              << "\n\tcomms period: " << packet.comms_period.count
              << "\n\tlast confirmed measurement index: " << packet.last_confirmed_measurement_index
              << "\n";
}

////////////////////////////////////////////////////////////////////////

static void process_sensor_requests(std::chrono::high_resolution_clock::duration dt)
{
    std::vector<uint8_t> raw_packet_data(s_sensor_comms.get_payload_raw_buffer_size(Comms::MAX_USER_DATA_SIZE));

    uint8_t size = Comms::MAX_USER_DATA_SIZE;
    uint8_t* packet_data = s_sensor_comms.receive_packet(raw_packet_data.data(), size, std::chrono::duration_cast<std::chrono::milliseconds>(dt).count());
    if (packet_data)
    {
        data::Type type = s_sensor_comms.get_rx_packet_type(packet_data);
        //LOG_LN("Received packed of " << (int)size << " bytes. Type: "<< (int)type);
        if (type == data::Type::PAIR_REQUEST && size == sizeof(data::Pair_Request))
        {
            Sensors::Sensor const* sensor = s_sensors.bind_sensor();
            if (sensor)
            {
                std::cout << "Adding sensor " << sensor->name << ", id " << sensor->id << ", address " << sensor->address << "\n";

                data::Pair_Response packet;
                packet.address = sensor->address;
                //strncpy(packet.name, sensor->name.c_str(), sizeof(packet.name));
                s_sensor_comms.set_destination_address(s_sensor_comms.get_rx_packet_source_address(packet_data));
                s_sensor_comms.begin_packet(raw_packet_data.data(), data::Type::PAIR_RESPONSE);
                s_sensor_comms.pack(raw_packet_data.data(), packet);
                if (s_sensor_comms.send_packet(raw_packet_data.data(), 10))
                {
                    std::cout << "Pair successful\n";
                }
                else
                {
                    s_sensors.remove_sensor(sensor->id);
                    std::cerr << "Pair failed (comms)\n";
                }
            }
            else
            {
                std::cerr << "Pairing failed (db)\n";
            }
        }
        else if (type == data::Type::MEASUREMENT_BATCH && size == sizeof(data::Measurement_Batch))
        {
            Sensors::Sensor_Address address = s_sensor_comms.get_rx_packet_source_address(packet_data);
            const Sensors::Sensor* sensor = s_sensors.find_sensor_by_address(address);
            if (sensor)
            {
                data::Measurement_Batch const& batch = *reinterpret_cast<data::Measurement_Batch const*>(s_sensor_comms.get_rx_packet_payload(packet_data));

                std::cout << "Measurement batch from " << sensor->id << "\n";
                if (batch.count == 0)
                {
                    std::cout << "\tRange: [empty]\n";
                }
                else if (batch.count == 1)
                {
                    std::cout << "\tRange: [" << batch.start_index << "] : 1 measurement\n";
                }
                else
                {
                    std::cout << "\tRange: [" << batch.start_index << " - " << batch.start_index + batch.count - 1 << "] : " << static_cast<size_t>(batch.count) << " measurements \n";
                }

                std::vector<Sensors::Measurement> measurements;
                size_t count = std::min<size_t>(batch.count, data::Measurement_Batch::MAX_COUNT);
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

                s_sensors.report_measurements(sensor->id, measurements);
            }
            else
            {
                std::cerr << "\tSensor not found!\n";
            }
        }
        else if (type == data::Type::FIRST_CONFIG_REQUEST && size == sizeof(data::First_Config_Request))
        {
            Sensors::Sensor_Address address = s_sensor_comms.get_rx_packet_source_address(packet_data);
            const Sensors::Sensor* sensor = s_sensors.find_sensor_by_address(address);
            if (sensor)
            {
                std::cout << "First config for " << sensor->id << "\n";

                data::First_Config packet;
                packet.first_measurement_index = s_sensors.compute_next_measurement_index();
                s_sensors.set_sensor_measurement_range(sensor->id, packet.first_measurement_index, 0);
                fill_config_packet(packet.config, *sensor);

                s_sensor_comms.set_destination_address(s_sensor_comms.get_rx_packet_source_address(packet_data));
                s_sensor_comms.begin_packet(raw_packet_data.data(), data::Type::FIRST_CONFIG);
                s_sensor_comms.pack(raw_packet_data.data(), packet);
                if (s_sensor_comms.send_packet(raw_packet_data.data(), 3))
                {
                    std::cout << "\tFirst Config successful\n";
                }
                else
                {
                    std::cerr << "\tFirst Config failed\n";
                }
            }
            else
            {
                std::cerr << "\tSensor not found!\n";
            }
        }
        else if (type == data::Type::CONFIG_REQUEST && size == sizeof(data::Config_Request))
        {
            data::Config_Request const& config_request = *reinterpret_cast<data::Config_Request const*>(s_sensor_comms.get_rx_packet_payload(packet_data));

            Sensors::Sensor_Address address = s_sensor_comms.get_rx_packet_source_address(packet_data);
            const Sensors::Sensor* sensor = s_sensors.find_sensor_by_address(address);
            if (sensor)
            {
                std::cout << "Config for " << sensor->id << "\n";
                if (config_request.measurement_count == 0)
                {
                    std::cout << "\tStored range: [empty]\n";
                }
                else if (config_request.measurement_count == 1)
                {
                    std::cout << "\tStored range: [" << config_request.first_measurement_index << "] : 1 measurement\n";
                }
                else
                {
                    std::cout << "\tStored range: [" << config_request.first_measurement_index << " - " << config_request.first_measurement_index + config_request.measurement_count - 1 << "] : " << config_request.measurement_count << " measurements \n";
                }

                s_sensors.set_sensor_measurement_range(sensor->id, config_request.first_measurement_index, config_request.measurement_count);
                s_sensors.set_sensor_b2s_input_dBm(sensor->id, config_request.b2s_input_dBm);

                data::Config packet;
                fill_config_packet(packet, *sensor);

                s_sensor_comms.set_destination_address(s_sensor_comms.get_rx_packet_source_address(packet_data));
                s_sensor_comms.begin_packet(raw_packet_data.data(), data::Type::CONFIG);
                s_sensor_comms.pack(raw_packet_data.data(), packet);
                if (s_sensor_comms.send_packet(raw_packet_data.data(), 3))
                {
                    std::cout << "\tSchedule successful\n";
                }
                else
                {
                    std::cerr << "\tSchedule failed\n";
                }
            }
            else
            {
                std::cerr << "\tSensor not found!\n";
            }
        }
    }
    std::cout << std::flush;
}

////////////////////////////////////////////////////////////////////////

int main(int, const char**)
{
    std::cout << "Starting...\n";

    srand(time(nullptr));

    gpioInitialise();

    {
        gpioSetMode(6, PI_OUTPUT);
        gpioWrite(6, 1);
        std::this_thread::sleep_for(std::chrono::milliseconds(7));
        gpioWrite(6, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    size_t tries = 0;
    while (!s_sensor_comms.init(1))
    {
        tries++;
        std::cerr << "comms init failed. Trying again: " << tries << "\n";
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    s_sensor_comms.set_address(Comms::BASE_ADDRESS);


    if (!s_server.init(4444, 5555))
    {
        std::cerr << "Server init failed\n";
        return -1;
    }

//    std::streambuf* s_old_cerr = std::cout.rdbuf(s_cerr_buffer.rdbuf());
//    size_t last_cerr_offset = 0;

    while (true)
    {
        process_sensor_requests(std::chrono::milliseconds(1000));
        s_server.process();

//        if (s_cerr_buffer.gcount() > last_cerr_offset)
//        {

//        }
    }

    return 0;
}

