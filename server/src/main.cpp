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
static Scheduler s_scheduler;
static Sensors s_sensors(s_scheduler);

typedef std::chrono::high_resolution_clock Clock;


static Clock::duration s_measurement_period = std::chrono::minutes(5);

struct Add_Sensor
{
    std::string name;
    Clock::duration timeout;
    Clock::time_point start;
} s_add_sensor;


extern void test_storage();
extern std::chrono::high_resolution_clock::time_point string_to_time_point(const std::string& str);
extern std::string time_point_to_string(std::chrono::high_resolution_clock::time_point tp);


////////////////////////////////////////////////////////////////////////

std::string s_fifo_name = "server_cmd";
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
        s_measurement_period = std::chrono::minutes(minutes);
        std::cout << "Measurement period is now " << minutes << "\n";
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

    //test_storage();

    s_sensors.load_sensors("sensors.config");

//    while (!s_comms.init(5))
//    {
//      std::cout << "init failed \n";
//      return -1;
//    }

    s_comms.set_address(Comms::SERVER_ADDRESS);

    while (true)
    {
        std::string cmd = read_fifo();
        process_command(cmd);

        //std::this_thread::sleep_for(std::chrono::milliseconds(10));

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
                        packet.server_timestamp = time(nullptr);
                        std::cout << "Pair requested. Replying with address " << packet.address << " and timestamp " << packet.server_timestamp;
                        s_comms.begin_packet(data::Type::PAIR_RESPONSE);
                        s_comms.pack(packet);
                        s_comms.send_packet(3);
                    }

                    s_sensors.save_sensors("sensors.config");
                }

                s_add_sensor.name.clear();
            }
            else if (type == data::Type::MEASUREMENT && size == sizeof(data::Measurement))
            {
                const data::Measurement* ptr = reinterpret_cast<const data::Measurement*>(s_comms.get_packet_payload());
                float vcc, t, h;
                ptr->unpack(vcc, h, t);
                //std::cout << s_comms.get_packet_source_address() << "::: " << date_time(ptr->timestamp) << " / " << current_date_time() << "\t" << int(ptr->index) << "\tVcc:" << vcc << "V\tH:" << h << "%\tT:" << t << "C" << "\n";

                //s_last_address = std::max(s_comms.get_packet_source_address() + 1u, s_last_address);
            }
        }
        std::cout << std::flush;
    }

    return 0;
}

