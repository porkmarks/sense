#include <iostream>
#include <sstream>
#include <thread>
#include <chrono>
#include <cstring>
#include <string>
#include <map>
#include <set>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <pigpio.h>

#include "Server.h"
#include "Log.h"


typedef std::chrono::system_clock Clock;

extern void run_tests();

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

int main(int, const char**)
{
    LOGI << "Starting..." << std::endl;

    srand(time(nullptr));

    //run_tests();

    if (gpioInitialise() < 0)
    {
        LOGE << "GPIO init failed." << std::endl;
        return EXIT_FAILURE;
    }

    LEDs leds;
    Radio radio;
    Server server(radio, leds);

    size_t tries = 0;
    while (!radio.init(10, 20))       
    {
        tries++;
        LOGE << "Comms init failed. Trying again: " << tries << std::endl;
        leds.set_blink(LEDs::Blink::Fast_Red, std::chrono::hours(24 * 1000), true);
        std::this_thread::sleep_for(std::chrono::seconds(1));
        if (tries > 10)
        {
            return EXIT_FAILURE;
        }
    }


    /*
    //comms testing
    {
        radio.set_address(Radio::BASE_ADDRESS);
        std::vector<uint8_t> raw_packet_data(packet_raw_size(Radio::MAX_USER_DATA_SIZE));

        radio.set_destination_address(Sensor_Comms::BROADCAST_ADDRESS);
        constexpr size_t sz = 10;
        char data[sz];
        memset(data, 0, sz);

        uint32_t counter = 0;
        while (true)
        {
            LOGI << "Sending " << counter << " ..." << std::endl;
            std::cout.flush();

            radio.begin_packet(raw_packet_data.data(), 1, true);
            radio.pack(raw_packet_data.data(), &counter, sizeof(counter));
            radio.pack(raw_packet_data.data(), data, sz);
            bool ok = radio.send_packed_packet(raw_packet_data.data(), true);
            LOGI << "\t\t" << (ok ? "done" : "failed") << ". RX ..." << std::endl;
            std::cout.flush();

            uint8_t size = Sensor_Comms::MAX_USER_DATA_SIZE;
            uint8_t* packet_data = radio.receive_packet(raw_packet_data.data(), size, chrono::millis(500));
            if (packet_data)
            {
                uint32_t rcounter = *((uint32_t*)radio.get_rx_packet_payload(packet_data));
                LOGI << "\t\t" << "done: " << rcounter << std::endl;
                std::cout.flush();
            }
            else
            {
                LOGI << "\t\t" << "timeout " << std::endl;
                std::cout.flush();
            }

            counter++;
        }
    }
//*/

    /*
    //send some test garbage for frequency measurements
    {
        radio.set_destination_address(Sensor_Comms::BROADCAST_ADDRESS);
        radio.begin_packet(raw_packet_data.data(), 0, false);
        radio.send_packed_packet(raw_packet_data.data(), true);
    }
    */

    tries = 0;
    while (!server.init(4444, 5555))
    {
        tries++;
        LOGE << "Server init failed. Trying again: " << tries << std::endl;
        leds.set_blink(LEDs::Blink::Fast_Yellow, std::chrono::hours(24 * 1000), true);
        std::this_thread::sleep_for(std::chrono::seconds(1));
        if (tries > 10)
        {
            return EXIT_FAILURE;
        }
    }

    leds.set_color(LEDs::Color::Green);

    while (true)
    {
        server.process();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    return EXIT_SUCCESS;
}

