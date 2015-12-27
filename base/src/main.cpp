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
#include <boost/asio.hpp>
#include <boost/thread.hpp>

#include "CRC.h"
#include "Client.h"
#include "Comms.h"
#include "Storage.h"
#include "Sensors.h"
#include "Scheduler.h"

#include "rfm22b/rfm22b.h"


#define LOG(x) std::cout << x
#define LOG_LN(x) std::cout << x << std::endl

boost::asio::io_service s_io_service;
boost::asio::ip::tcp::endpoint s_server_endpoint;

static Client s_client(s_io_service);
static Comms s_comms;
static Sensors s_sensors;

typedef std::chrono::high_resolution_clock Clock;

struct Add_Sensor
{
    std::string name;
    Clock::duration timeout;
    Clock::time_point start;
} s_add_sensor;


extern void run_tests();
extern std::chrono::high_resolution_clock::time_point string_to_time_point(const std::string& str);
extern std::string time_point_to_string(std::chrono::high_resolution_clock::time_point tp);


////////////////////////////////////////////////////////////////////////

int main(int, const char**)
{
    std::cout << "Starting...\n";

    srand(time(nullptr));

    boost::shared_ptr<boost::asio::io_service::work> work(new boost::asio::io_service::work(s_io_service));
    boost::thread_group worker_threads;
    for(int x = 0; x < 1; ++x)
    {
      worker_threads.create_thread(boost::bind(&boost::asio::io_service::run, &s_io_service));
    }

    s_server_endpoint = boost::asio::ip::tcp::endpoint(boost::asio::ip::address::from_string("127.0.0.1"), 50000);

    size_t tries = 0;
    while (!s_client.init(s_server_endpoint))
    {
        tries++;
        std::cerr << "client init failed. Trying again: " << tries << "\n";
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

//    run_tests();

    //s_sensors.load_sensors("sensors.config");

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
                if (s_add_sensor.name.empty())
                {
                    std::cerr << "Pair request but not expecting any sensor!\n";
                }
                else if (Clock::now() > s_add_sensor.start + s_add_sensor.timeout)
                {
                    std::cerr << "Pair requested for sensor '" << s_add_sensor.name << "' but it timed out!\n";
                }
                else
                {
                    Sensors::Sensor_Id id = s_sensors.add_sensor(s_add_sensor.name);
                    const Sensors::Sensor* sensor = s_sensors.find_sensor_by_id(id);
                    if (!sensor)
                    {
                        std::cerr << "Failed to add sensor '" << s_add_sensor.name << "'!\n";
                    }
                    else
                    {
                        data::Pair_Response packet;
                        packet.address = id;
                        s_comms.begin_packet(data::Type::PAIR_RESPONSE);
                        s_comms.pack(packet);
                        if (s_comms.send_packet(3))
                        {
                            s_add_sensor.name.clear();
                            std::cout << "Pair successful\n";
                        }
                        else
                        {
                            std::cerr << "Pair failed\n";
                            s_sensors.remove_sensor(id);
                        }
                    }

                    s_sensors.save_sensors("sensors.config");
                }
            }
            else if (type == data::Type::MEASUREMENT && size == sizeof(data::Measurement))
            {
                Sensors::Sensor_Id id = s_comms.get_packet_source_address();
                std::cout << "Measurement reported by " << id << "\n";
                const Sensors::Sensor* sensor = s_sensors.find_sensor_by_id(id);
                if (sensor)
                {
                    Sensors::Measurement m;
                    const data::Measurement* ptr = reinterpret_cast<const data::Measurement*>(s_comms.get_packet_payload());

                    //m.time_point = Clock::from_time_t(time_t(ptr->time_point));
                    m.flags = ptr->flags;
                    m.tx_rssi = 0;
                    m.rx_rssi = 0;
                    ptr->unpack(m.vcc, m.humidity, m.temperature);
//                    m.index = ptr->index;

                    //Clock::time_point tp = Clock::from_time_t(ptr->time_point);

                    s_sensors.add_measurement(ptr->index, id, m);
                }
                else
                {
                    std::cerr << "\tSensor not found!\n";
                }
            }
            else if (type == data::Type::CONFIG_REQUEST && size == sizeof(data::Config_Request))
            {
                Sensors::Sensor_Id id = s_comms.get_packet_source_address();
                std::cout << "Config requested by " << id << "\n";
                const Sensors::Sensor* sensor = s_sensors.find_sensor_by_id(id);
                if (sensor)
                {
                    data::Config packet;

                    packet.base_time_point = chrono::time_s(Clock::to_time_t(Clock::now()));
                    packet.measurement_period = chrono::seconds(std::chrono::duration_cast<std::chrono::seconds>(s_sensors.get_measurement_period()).count());
                    packet.next_measurement_time_point = chrono::time_s(Clock::to_time_t(s_sensors.compute_next_measurement_time_point()));
                    packet.comms_period = chrono::seconds(std::chrono::duration_cast<std::chrono::seconds>(s_sensors.compute_comms_period()).count());
                    packet.next_comms_time_point = chrono::time_s(Clock::to_time_t(s_sensors.compute_next_comms_time_point(id)));
                    packet.last_confirmed_measurement_index = s_sensors.compute_last_confirmed_measurement_index(id);
                    packet.next_measurement_index = s_sensors.compute_next_measurement_index();

                    std::cout << "Config requested for id: " << id
                                                            << "\n\tbase time point: " << packet.base_time_point.ticks
                                                            << "\n\tmeasurement period: " << packet.measurement_period.count
                                                            << "\n\tnext measurement time point: " << packet.next_measurement_time_point.ticks
                                                            << "\n\tcomms period: " << packet.comms_period.count
                                                            << "\n\tnext comms time point: " << packet.next_comms_time_point.ticks
                                                            << "\n\tlast confirmed measurement index: " << packet.last_confirmed_measurement_index
                                                            << "\n\tnext measurement index: " << packet.next_measurement_index << "\n";

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

