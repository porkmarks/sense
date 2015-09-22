#include <iostream>
#include <thread>
#include <chrono>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <stdio.h>
#include <time.h>
#include "CRC.h"
#include "Comms.h"

#include "rfm22b/rfm22b.h"

static Comms s_comms;

uint32_t s_last_address = Comms::SLAVE_ADDRESS_BEGIN;

typedef std::chrono::high_resolution_clock Clock;


struct Measurement
{
    float humidity = 0;
    float temperature = 0.f;
    float vcc = 0.f;
    float rssi = 0.f;
};

struct Sensor
{
    uint32_t address = 0;
    std::string name;
    uint32_t time_slot = 0;
    Clock::time_point next_comm_tp = Clock::now();

    std::vector<Measurement> measurements;
};

static Clock::duration s_measurement_period = std::chrono::minutes(5);
static Clock::duration s_comms_period = std::chrono::minutes(20);
static Clock::duration s_time_slot_duration = std::chrono::seconds(10);

static std::vector<Sensor> s_sensors;
static std::set<uint32_t> s_time_slots;

static bool is_slot_taken(uint32_t slot)
{
    for (const Sensor& sensor: s_sensors)
    {
        if (sensor.time_slot == slot)
        {
            return true;
        }
    }
    return false;
}

static uint32_t get_max_slot_count()
{
    return static_cast<uint32_t>(s_comms_period / s_time_slot_duration);
}

static int32_t find_free_slot()
{
    uint32_t count = get_max_slot_count();
    for (uint32_t i = 0; i < count; i++)
    {
        if (!is_slot_taken(i))
        {
            return static_cast<int32_t>(i);
        }
    }
    return -1;
}

std::unique_ptr<Sensor> new_sensor()
{
    int32_t slot = find_free_slot();
    if (slot < 0)
    {
        uint32_t count = get_max_slot_count();
        std::cout << "No more empty slots. There is a max number of slots supported: " << count << "\n";
        return nullptr;
    }

    std::unique_ptr<Sensor> sensor(new Sensor);
    sensor->time_slot = slot;
    return sensor;
}


// Get current date/time, format is YYYY-MM-DD HH:mm:ss
std::string date_time(time_t tp)
{
    struct tm  tstruct;
    char       buf[80];
    tstruct = *localtime(&tp);
    // Visit http://en.cppreference.com/w/cpp/chrono/c/strftime
    // for more information about date/time format
    strftime(buf, sizeof(buf), "%Y-%m-%d %X", &tstruct);
    return buf;
}
std::string current_date_time()
{
    return date_time(time(nullptr));
}

int main()
{
    std::cout << "Starting...\n";

    while (!s_comms.init(5))
    {
      std::cout << "init failed \n";
      return -1;
    }

    s_comms.set_address(Comms::SERVER_ADDRESS);

    size_t i = 0;
    while (true)
    {
        uint8_t size = s_comms.receive_packet(1000);
        if (size > 0)
        {
            data::Type type = s_comms.get_packet_type();
            if (type == data::Type::PAIR_REQUEST && size == sizeof(data::Pair_Request))
            {
                data::Pair_Response packet;
                packet.address = s_last_address++;
                packet.server_timestamp = time(nullptr);
                s_comms.begin_packet(data::Type::PAIR_RESPONSE);
                s_comms.pack(packet);
                s_comms.send_packet(3);
            }
            else if (type == data::Type::MEASUREMENT && size == sizeof(data::Measurement))
            {
                const data::Measurement* ptr = reinterpret_cast<const data::Measurement*>(s_comms.get_packet_payload());
                uint32_t timestamp;
                uint8_t index;
                float vcc, t, h;
                ptr->unpack(vcc, h, t);
                std::cout << date_time(ptr->timestamp) << " / " << current_date_time() << "\t" << int(ptr->index) << "\tVcc:" << vcc << "V\tH:" << h << "%\tT:" << t << "C" << "\n";
            }
        }
        std::cout << std::flush;
    }


    return 0;
}

