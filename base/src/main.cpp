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
#include "CRC.h"
#include "Comms.h"
#include "Storage.h"
#include "Sensors.h"
#include "Scheduler.h"

#include "rfm22b/rfm22b.h"


#define LOG(x) std::cout << x
#define LOG_LN(x) std::cout << x << std::endl

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

std::string s_fifo_name = "base_cmd";
int s_fifo = -1;
std::string s_fifo_buffer;


static void create_fifo()
{
    remove(s_fifo_name.c_str());

    int status = mkfifo(s_fifo_name.c_str(), S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH);
    if (status < 0)
    {
        std::cerr << "Cannot create " << s_fifo_name << " fifo!\n";
        exit(1);
    }

    s_fifo = open(s_fifo_name.c_str(), O_RDONLY | O_NONBLOCK);
    if (s_fifo < 0)
    {
        std::cerr << "Cannot open " << s_fifo_name << " fifo!\n";
        exit(1);
    }
}

static std::string read_fifo()
{
    char buffer[256];
    while (true)
    {
        int result = read(s_fifo, buffer, sizeof(buffer));
        if (result < 0)
        {
            if (errno == EAGAIN)
            {
                break;
            }
            std::cerr << "Error reading the fifo: " << errno << "\n";
            s_fifo_buffer.clear();
            return std::string();
        }
        if (result > 0)
        {
            s_fifo_buffer.append(buffer, buffer + result);
        }
        break;
    }

    //did we get an enter key?
    size_t enter_off = s_fifo_buffer.find('\n');
    if (enter_off != std::string::npos)
    {
        std::string res = s_fifo_buffer.substr(0, enter_off);
        s_fifo_buffer.erase(s_fifo_buffer.begin(), s_fifo_buffer.begin() + enter_off + 1);
        return res;
    }
    return std::string();
//    return std::move(s_fifo_buffer);
}

static void process_command(const std::string& command)
{
    if (command.empty())
    {
        return;
    }

    std::cout << "Command: '" << command << "'\n";

    std::stringstream ss(command);

    std::string cmd;
    if (!(ss >> cmd))
    {
        std::cerr << "Invalid command!\n";
        return;
    }

    if (cmd == "as") //add sensor
    {
        if (!(ss >> s_add_sensor.name))
        {
            std::cerr << "Invalid command '" << cmd << "'!\n";
            std::cerr << "Syntax: as name [timeout]\n";
            return;
        }

        uint32_t timeout_seconds;
        if (!(ss >> timeout_seconds))
        {
            timeout_seconds = 30;
        }
        s_add_sensor.timeout = std::chrono::seconds(timeout_seconds);
        s_add_sensor.start = Clock::now();
        std::cout << "Listening for '" << s_add_sensor.name << "' for " << timeout_seconds << " seconds.\n";
    }
    else if (cmd == "rs") //remove sensor
    {
        Sensors::Sensor_Id id;
        if (!(ss >> id))
        {
            std::cerr << "Invalid command '" << cmd << "'!\n";
            std::cerr << "Syntax: rs id\n";
            return;
        }
        std::cout << "Removing sensor " << id << "\n";
        s_sensors.remove_sensor(id);
    }
    else if (cmd == "ls") //list sensors
    {
        std::string filename;
        if (!(ss >> filename))
        {
            std::cerr << "Invalid command '" << cmd << "'!\n";
            std::cerr << "Syntax: ls filename\n";
            return;
        }
        std::cout << "Saving sensors to " << filename << "\n";
        s_sensors.save_sensors(filename);
    }
    else if (cmd == "lm") //list measurements
    {
        std::string filename;
        Sensors::Sensor_Id id;
        if (!(ss >> filename) || !(ss >> id))
        {
            std::cerr << "Invalid command '" << cmd << "'!\n";
            std::cerr << "Syntax: lm filename id <days>\n";
            return;
        }
        Clock::time_point end = Clock::now();
        Clock::time_point begin = end - std::chrono::hours(24) * 30 * 12; //last year
        uint32_t days;
        if (ss >> days)
        {
            begin = end - std::chrono::hours(24) * days;
        }
        std::cout << "Saving sensor " << id << " measurements from '" << time_point_to_string(begin) << "' to '" << time_point_to_string(end) << "' to " << filename << "\n";
        s_sensors.save_sensor_measurements_tsv(filename, id, begin, end);
    }
    else if (cmd == "mp") //measurement period
    {
        uint32_t minutes;
        if (!(ss >> minutes))
        {
            std::cerr << "Invalid command '" << cmd << "'!\n";
            std::cerr << "Syntax: mp minutes\n";
            return;
        }
        minutes = std::max(minutes, 1u);
        s_sensors.set_measurement_period(std::chrono::minutes(minutes));
        std::cout << "Measurement period is now " << std::chrono::duration_cast<std::chrono::minutes>(s_sensors.get_measurement_period()).count() << " minutes\n";
    }
    else if (cmd == "mps") //measurement period seconds
    {
        uint32_t seconds;
        if (!(ss >> seconds))
        {
            std::cerr << "Invalid command '" << cmd << "'!\n";
            std::cerr << "Syntax: mp seconds\n";
            return;
        }
        seconds = std::max(seconds, 10u);
        s_sensors.set_measurement_period(std::chrono::seconds(seconds));
        std::cout << "Measurement period is now " << std::chrono::duration_cast<std::chrono::seconds>(s_sensors.get_measurement_period()).count() << " seconds\n";
    }
    else if (cmd == "x")
    {
        std::cout << "Exiting...\n";
        exit(0);
    }
    else
    {
        std::cerr << "Unrecognized command '" << cmd << "'!\n";
    }
}


int main()
{
    std::cout << "Starting...\n";

    srand(time(nullptr));

    create_fifo();

    run_tests();

    //s_sensors.load_sensors("sensors.config");

    while (!s_comms.init(5))
    {
      std::cout << "init failed \n";
      return -1;
    }

    s_comms.set_address(Comms::BASE_ADDRESS);

    while (true)
    {
        std::string cmd = read_fifo();
        process_command(cmd);

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

                    packet.base_time_point_s = Clock::to_time_t(Clock::now());
                    packet.measurement_period_s = std::chrono::duration_cast<std::chrono::seconds>(s_sensors.get_measurement_period()).count();
                    packet.next_measurement_time_point_s = Clock::to_time_t(s_sensors.compute_next_measurement_time_point());
                    packet.comms_period_s = std::chrono::duration_cast<std::chrono::seconds>(s_sensors.compute_comms_period()).count();
                    packet.next_comms_time_point_s = Clock::to_time_t(s_sensors.compute_next_comms_time_point(id));
                    packet.last_confirmed_measurement_index = s_sensors.compute_last_confirmed_measurement_index(id);
                    packet.next_measurement_index = s_sensors.compute_next_measurement_index();

                    std::cout << "Config requested for id: " << id
                                                            << "\n\tbase time point: " << packet.base_time_point_s
                                                            << "\n\tmeasurement period: " << packet.measurement_period_s
                                                            << "\n\tnext measurement time point: " << packet.next_measurement_time_point_s
                                                            << "\n\tcomms period: " << packet.comms_period_s
                                                            << "\n\tnext comms time point: " << packet.next_comms_time_point_s
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

