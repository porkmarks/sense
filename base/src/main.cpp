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
#include "DB.h"
#include "Comms.h"
#include "Storage.h"
#include "Sensors.h"
#include "Scheduler.h"

#include "DB.h"

#include "rfm22b/rfm22b.h"


#define LOG(x) std::cout << x
#define LOG_LN(x) std::cout << x << std::endl

static Comms s_comms;
static DB s_db;
static Sensors s_sensors(s_db);

typedef std::chrono::high_resolution_clock Clock;

extern void run_tests();
extern std::chrono::high_resolution_clock::time_point string_to_time_point(const std::string& str);
extern std::string time_point_to_string(std::chrono::high_resolution_clock::time_point tp);


////////////////////////////////////////////////////////////////////////

void fill_config_packet(data::Config& packet, Sensors::Sensor const& sensor)
{
    packet.base_time_point = chrono::time_s(Clock::to_time_t(Clock::now()));
    packet.measurement_period = chrono::seconds(std::chrono::duration_cast<std::chrono::seconds>(s_sensors.get_measurement_period()).count());
    packet.next_measurement_time_point = chrono::time_s(Clock::to_time_t(s_sensors.compute_next_measurement_time_point()));
    packet.comms_period = chrono::seconds(std::chrono::duration_cast<std::chrono::seconds>(s_sensors.compute_comms_period()).count());
    packet.next_comms_time_point = chrono::time_s(Clock::to_time_t(s_sensors.compute_next_comms_time_point(sensor.id)));
    packet.last_confirmed_measurement_index = s_sensors.compute_last_confirmed_measurement_index(sensor.id);

    std::cout << "Config requested for id: " << sensor.id
                                            << "\n\tbase time point: " << packet.base_time_point.ticks
                                            << "\n\tmeasurement period: " << packet.measurement_period.count
                                            << "\n\tnext measurement time point: " << packet.next_measurement_time_point.ticks
                                            << "\n\tcomms period: " << packet.comms_period.count
                                            << "\n\tnext comms time point: " << packet.next_comms_time_point.ticks
                                            << "\n\tlast confirmed measurement index: " << packet.last_confirmed_measurement_index
                                            << "\n";
}

////////////////////////////////////////////////////////////////////////

int main(int, const char**)
{
    std::cout << "Starting...\n";

    srand(time(nullptr));

    size_t tries = 0;

    gpioInitialise();

//    run_tests();

    //s_sensors.load_sensors("sensors.config");

    static const std::string db_server = "192.168.1.36";
    static const std::string db_db = "sensor-test";
    static const std::string db_username = "admin";
    static const std::string db_password = "admin";
    static const uint16_t port = 0;

    if (!s_db.init(db_server, db_db, db_username, db_password, port))
    {
        std::cerr << "DB init failed\n";
        return -1;
    }

    if (!s_sensors.init())
    {
        std::cerr << "Sensors init failed\n";
        return -1;
    }

    tries = 0;
    while (!s_comms.init(1))
    {
        tries++;
        std::cerr << "comms init failed. Trying again: " << tries << "\n";
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    s_comms.set_address(Comms::BASE_ADDRESS);


    while (true)
    {
        //std::string cmd = read_fifo();
        //process_command(cmd);
        //client.process();

        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        uint8_t size = s_comms.receive_packet(1000);
        if (size > 0)
        {
            data::Type type = s_comms.get_packet_type();
            //LOG_LN("Received packed of " << (int)size << " bytes. Type: "<< (int)type);
            if (type == data::Type::PAIR_REQUEST && size == sizeof(data::Pair_Request))
            {
                boost::optional<DB::Expected_Sensor> expected_sensor = s_db.get_expected_sensor();
                if (!expected_sensor)
                {
                    std::cerr << "Pair request but not expecting any sensor!\n";
                }
                else
                {
                    Sensors::Sensor const* sensor = s_sensors.add_expected_sensor();
                    if (sensor)
                    {
                        std::cout << "Adding sensor " << sensor->name << ", id " << sensor->id << ", address " << sensor->address << "\n";

                        data::Pair_Response packet;
                        packet.address = sensor->address;
                        s_comms.set_destination_address(s_comms.get_packet_source_address());
                        s_comms.begin_packet(data::Type::PAIR_RESPONSE);
                        s_comms.pack(packet);
                        if (s_comms.send_packet(10))
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
            }
            else if (type == data::Type::MEASUREMENT && size == sizeof(data::Measurement))
            {
                Sensors::Sensor_Address address = s_comms.get_packet_source_address();
                std::cout << "Measurement reported by " << address << "\n";
                const Sensors::Sensor* sensor = s_sensors.find_sensor_by_address(address);
                if (sensor)
                {
                    Sensors::Measurement m;
                    data::Measurement const& measurement = *reinterpret_cast<data::Measurement const*>(s_comms.get_packet_payload());

                    //m.time_point = Clock::from_time_t(time_t(ptr->time_point));
                    m.index = measurement.index;
                    m.flags = measurement.flags;
                    m.tx_rssi = 0;
                    m.rx_rssi = 0;
                    measurement.unpack(m.vcc, m.humidity, m.temperature);
//                    m.index = measurement.index;

                    //Clock::time_point tp = Clock::from_time_t(measurement.time_point);

                    s_sensors.add_measurement(sensor->id, m);
                }
                else
                {
                    std::cerr << "\tSensor not found!\n";
                }
            }
            else if (type == data::Type::FIRST_CONFIG_REQUEST && size == sizeof(data::First_Config_Request))
            {
                Sensors::Sensor_Address address = s_comms.get_packet_source_address();
                std::cout << "First config requested by " << address << "\n";
                const Sensors::Sensor* sensor = s_sensors.find_sensor_by_address(address);
                if (sensor)
                {
                    data::First_Config packet;
                    packet.first_measurement_index = s_sensors.compute_next_measurement_index();
                    s_sensors.set_sensor_measurement_range(sensor->id, packet.first_measurement_index, 0);
                    fill_config_packet(packet.config, *sensor);

                    s_comms.set_destination_address(s_comms.get_packet_source_address());
                    s_comms.begin_packet(data::Type::FIRST_CONFIG);
                    s_comms.pack(packet);
                    if (s_comms.send_packet(3))
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
                data::Config_Request const& config_request = *reinterpret_cast<data::Config_Request const*>(s_comms.get_packet_payload());

                Sensors::Sensor_Address address = s_comms.get_packet_source_address();
                std::cout << "Config requested by " << address << "\n";
                const Sensors::Sensor* sensor = s_sensors.find_sensor_by_address(address);
                if (sensor)
                {
                    s_sensors.set_sensor_measurement_range(sensor->id, config_request.first_measurement_index, config_request.measurement_count);

                    data::Config packet;
                    fill_config_packet(packet, *sensor);

                    s_comms.set_destination_address(s_comms.get_packet_source_address());
                    s_comms.begin_packet(data::Type::CONFIG);
                    s_comms.pack(packet);
                    if (s_comms.send_packet(3))
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

    return 0;
}

