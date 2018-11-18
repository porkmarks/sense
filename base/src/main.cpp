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
#include "Server.h"
#include "Log.h"

static Sensor_Comms s_sensor_comms;
static Server s_server;

typedef std::chrono::system_clock Clock;

extern void run_tests();
extern std::chrono::system_clock::time_point string_to_time_point(const std::string& str);
extern std::string time_point_to_string(std::chrono::system_clock::time_point tp);

////////////////////////////////////////////////////////////////////////

constexpr uint32_t RED_LED_PIN = 4;
constexpr uint32_t GREEN_LED_PIN = 17;

void fatal_error()
{
    gpioWrite(GREEN_LED_PIN, 0);
    while (true)
    {
        gpioWrite(RED_LED_PIN, 0);
        chrono::delay(chrono::millis(200));
        gpioWrite(RED_LED_PIN, 1);
        chrono::delay(chrono::millis(200));
    }
}

int main(int, const char**)
{
    LOGI << "Starting..." << std::endl;

    srand(time(nullptr));

    gpioInitialise();

    {
        gpioSetMode(RED_LED_PIN, PI_OUTPUT);
        gpioSetMode(GREEN_LED_PIN, PI_OUTPUT);
        gpioWrite(RED_LED_PIN, 0);
        gpioWrite(GREEN_LED_PIN, 1);
    }

    size_t tries = 0;
    while (!s_sensor_comms.init(10, 20))
    {
        tries++;
        LOGE << "comms init failed. Trying again: " << tries << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
        if (tries > 10)
        {
            fatal_error();
            return EXIT_FAILURE;
        }
    }
    gpioWrite(RED_LED_PIN, 0);
    gpioWrite(GREEN_LED_PIN, 1);

    s_sensor_comms.set_address(Sensor_Comms::BASE_ADDRESS);

    std::vector<uint8_t> raw_packet_data(packet_raw_size(Sensor_Comms::MAX_USER_DATA_SIZE));

    //send some test garbage for frequency measurements
    {
        s_sensor_comms.set_destination_address(Sensor_Comms::BROADCAST_ADDRESS);
        s_sensor_comms.begin_packet(raw_packet_data.data(), 0);
        s_sensor_comms.send_packet(raw_packet_data.data(), 2);
    }


    if (!s_server.init(4444, 5555))
    {
        LOGE << "Server init failed" << std::endl;
        fatal_error();
        return EXIT_FAILURE;
    }

    Server::Sensor_Request request;
    Server::Sensor_Response response;
    while (true)
    {
        uint8_t size = Sensor_Comms::MAX_USER_DATA_SIZE;
        uint8_t* packet_data = s_sensor_comms.receive_packet(raw_packet_data.data(), size, 1000);
        if (packet_data)
        {
            request.type = s_sensor_comms.get_rx_packet_type(packet_data);
            request.signal_s2b = s_sensor_comms.get_input_dBm();
            request.address = s_sensor_comms.get_rx_packet_source_address(packet_data);
            LOGI << "Signal strength " << (int)request.signal_s2b << "dBm" << std::endl;

//            if (request.address != 1004 && request.address >= 1000)
//            {
//                //XXXYYY
//                continue;
//            }

            //TODO - add a marker in the request in indicate if the sensor waits for a response.
            //Like this I can avoid the RTT time for measurement batches, since they don't need a response

            request.payload.resize(size);
            if (size > 0)
            {
                memcpy(request.payload.data(), s_sensor_comms.get_rx_packet_payload(packet_data), size);
            }
            if (s_server.send_sensor_message(request, response))
            {
                s_sensor_comms.set_destination_address(response.address);
                s_sensor_comms.begin_packet(raw_packet_data.data(), response.type);
                if (!response.payload.empty())
                {
                    s_sensor_comms.pack(raw_packet_data.data(), response.payload.data(), static_cast<uint8_t>(response.payload.size()));
                }
                if (!s_sensor_comms.send_packet(raw_packet_data.data(), response.retries))
                {
                    LOGE << "Sensor comms failed" << std::endl;
                }
            }
        }
        else
        {
            s_server.process();
        }
    }

    return 0;
}

