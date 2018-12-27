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

enum class Led_Color
{
    None,
    Green,
    Red,
    Yellow,
};

enum class Led_Blink
{
    None,
    Slow_Green,
    Slow_Red,
    Slow_Yellow,
    Fast_Green,
    Fast_Red,
    Fast_Yellow,
};

static Led_Color s_led_color = Led_Color::None;
static std::atomic<Led_Blink> s_led_blink;
static std::chrono::system_clock::time_point s_led_blink_time_point;
static volatile bool s_led_thread_on = true;

constexpr int k_red_pwm = 30;

void set_led_color(Led_Color color)
{
    gpioPWM(RED_LED_PIN, 0);
    gpioWrite(RED_LED_PIN, 0);
    gpioWrite(GREEN_LED_PIN, 0);

    if (color == Led_Color::Green)
    {
        gpioWrite(RED_LED_PIN, 0);
        gpioWrite(GREEN_LED_PIN, 1);
    }
    else if (color == Led_Color::Red)
    {
        gpioWrite(RED_LED_PIN, 1);
        gpioWrite(GREEN_LED_PIN, 0);
    }
    else if (color == Led_Color::Yellow)
    {
        gpioPWM(RED_LED_PIN, k_red_pwm);
        gpioWrite(GREEN_LED_PIN, 1);
    }
    s_led_color = color;
}

void led_thread_func()
{
    gpioSetMode(RED_LED_PIN, PI_OUTPUT);
    gpioSetMode(GREEN_LED_PIN, PI_OUTPUT);
    gpioWrite(RED_LED_PIN, 0);
    gpioWrite(GREEN_LED_PIN, 0);

    while (s_led_thread_on)
    {
        auto now = std::chrono::system_clock::now();
        if (s_led_blink_time_point > now && s_led_blink != Led_Blink::None)
        {
            gpioPWM(RED_LED_PIN, 0);
            gpioWrite(RED_LED_PIN, 0);
            gpioWrite(GREEN_LED_PIN, 0);

            if (s_led_blink == Led_Blink::Slow_Red)
            {
                gpioWrite(RED_LED_PIN, 0);
                gpioWrite(GREEN_LED_PIN, 0);
                chrono::delay(chrono::millis(200));
                gpioWrite(RED_LED_PIN, 1);
                gpioWrite(GREEN_LED_PIN, 0);
                chrono::delay(chrono::millis(200));
            }
            else if (s_led_blink == Led_Blink::Slow_Green)
            {
                gpioWrite(RED_LED_PIN, 0);
                gpioWrite(GREEN_LED_PIN, 0);
                chrono::delay(chrono::millis(200));
                gpioWrite(RED_LED_PIN, 0);
                gpioWrite(GREEN_LED_PIN, 1);
                chrono::delay(chrono::millis(200));
            }
            else if (s_led_blink == Led_Blink::Slow_Yellow)
            {
                gpioPWM(RED_LED_PIN, 0);
                gpioWrite(GREEN_LED_PIN, 0);
                chrono::delay(chrono::millis(200));
                gpioPWM(RED_LED_PIN, k_red_pwm);
                gpioWrite(GREEN_LED_PIN, 1);
                chrono::delay(chrono::millis(200));
            }
            else if (s_led_blink == Led_Blink::Fast_Red)
            {
                gpioWrite(RED_LED_PIN, 0);
                gpioWrite(GREEN_LED_PIN, 0);
                chrono::delay(chrono::millis(50));
                gpioWrite(RED_LED_PIN, 1);
                gpioWrite(GREEN_LED_PIN, 0);
                chrono::delay(chrono::millis(50));
            }
            else if (s_led_blink == Led_Blink::Fast_Green)
            {
                gpioWrite(RED_LED_PIN, 0);
                gpioWrite(GREEN_LED_PIN, 0);
                chrono::delay(chrono::millis(50));
                gpioWrite(RED_LED_PIN, 0);
                gpioWrite(GREEN_LED_PIN, 1);
                chrono::delay(chrono::millis(50));
            }
            else if (s_led_blink == Led_Blink::Fast_Yellow)
            {
                gpioPWM(RED_LED_PIN, 0);
                gpioWrite(GREEN_LED_PIN, 0);
                chrono::delay(chrono::millis(50));
                gpioPWM(RED_LED_PIN, k_red_pwm);
                gpioWrite(GREEN_LED_PIN, 1);
                chrono::delay(chrono::millis(50));
            }
        }
        else
        {
            set_led_color(s_led_color);
            chrono::delay(chrono::millis(10));
        }
    }
}

void set_led_blink(Led_Blink blink, std::chrono::system_clock::duration duration, bool force = false)
{
    auto now = std::chrono::system_clock::now();
    if (now >= s_led_blink_time_point || s_led_blink == Led_Blink::None || force)
    {
        s_led_blink = blink;
        s_led_blink_time_point = std::chrono::system_clock::now() + duration;
    }
}

void fatal_error()
{
    while (true)
    {
    }
}

static data::Server_State s_state = data::Server_State::NORMAL;
static std::chrono::system_clock::time_point s_revert_state_to_normal_tp = std::chrono::system_clock::now();
data::Server_State set_state(data::Server_State new_state)
{
    if (s_state == new_state)
    {
        return s_state;
    }
    LOGI << "Changing state to " << (int)new_state << std::endl;
    s_state = new_state;

    if (s_state == data::Server_State::PAIRING)
    {
        s_sensor_comms.stop_async_receive();
        s_sensor_comms.set_frequency(869.f);
    }
    else
    {
        s_sensor_comms.stop_async_receive();
        s_sensor_comms.set_frequency(868.f);
    }
    s_revert_state_to_normal_tp = std::chrono::system_clock::now() + std::chrono::seconds(180);
    return s_state;
}

void process_state()
{
    if (s_state != data::Server_State::PAIRING)
    {
        if (std::chrono::system_clock::now() >= s_revert_state_to_normal_tp)
        {
            set_state(data::Server_State::NORMAL);
        }
    }
}

int main(int, const char**)
{
    LOGI << "Starting..." << std::endl;

    srand(time(nullptr));

    if (gpioInitialise() < 0)
    {
        LOGE << "GPIO init failed." << std::endl;
        s_led_thread_on = false;
        return EXIT_FAILURE;
    }

    std::thread led_thread(&led_thread_func);

    size_t tries = 0;
    while (!s_sensor_comms.init(10, 20))
    {
        tries++;
        LOGE << "Comms init failed. Trying again: " << tries << std::endl;
        set_led_blink(Led_Blink::Fast_Red, std::chrono::hours(24 * 1000), true);
        std::this_thread::sleep_for(std::chrono::seconds(1));
        if (tries > 10)
        {
            s_led_thread_on = false;
            return EXIT_FAILURE;
        }
    }

    s_sensor_comms.set_address(Sensor_Comms::BASE_ADDRESS);

    std::vector<uint8_t> raw_packet_data(packet_raw_size(Sensor_Comms::MAX_USER_DATA_SIZE));


    /*
    //comms testing
    {
        s_sensor_comms.set_destination_address(Sensor_Comms::BROADCAST_ADDRESS);
        constexpr size_t sz = 10;
        char data[sz];
        memset(data, 0, sz);

        uint32_t counter = 0;
        while (true)
        {
            LOGI << "Sending " << counter << " ..." << std::endl;
            std::cout.flush();

            s_sensor_comms.begin_packet(raw_packet_data.data(), 1, true);
            s_sensor_comms.pack(raw_packet_data.data(), &counter, sizeof(counter));
            s_sensor_comms.pack(raw_packet_data.data(), data, sz);
            bool ok = s_sensor_comms.send_packed_packet(raw_packet_data.data(), true);
            LOGI << "\t\t" << (ok ? "done" : "failed") << ". RX ..." << std::endl;
            std::cout.flush();

            uint8_t size = Sensor_Comms::MAX_USER_DATA_SIZE;
            uint8_t* packet_data = s_sensor_comms.receive_packet(raw_packet_data.data(), size, chrono::millis(500));
            if (packet_data)
            {
                uint32_t rcounter = *((uint32_t*)s_sensor_comms.get_rx_packet_payload(packet_data));
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
        s_sensor_comms.set_destination_address(Sensor_Comms::BROADCAST_ADDRESS);
        s_sensor_comms.begin_packet(raw_packet_data.data(), 0, false);
        s_sensor_comms.send_packed_packet(raw_packet_data.data(), true);
    }
    */

    tries = 0;
    while (!s_server.init(4444, 5555))
    {
        tries++;
        LOGE << "Server init failed. Trying again: " << tries << std::endl;
        set_led_blink(Led_Blink::Fast_Yellow, std::chrono::hours(24 * 1000), true);
        std::this_thread::sleep_for(std::chrono::seconds(1));
        if (tries > 10)
        {
            s_led_thread_on = false;
            return EXIT_FAILURE;
        }
    }

    set_led_color(Led_Color::Green);

    s_server.on_state_requested = &set_state;

    Server::Sensor_Request request;
    Server::Sensor_Response response;
    while (true)
    {
        if (s_server.is_connected())
        {
            set_led_color(Led_Color::Green);
        }
        else
        {
            set_led_blink(Led_Blink::Slow_Green, std::chrono::seconds(10));
        }

        s_sensor_comms.start_async_receive();

        uint8_t size = Sensor_Comms::MAX_USER_DATA_SIZE;
        uint8_t* packet_data = s_sensor_comms.async_receive_packet(raw_packet_data.data(), size);
        if (packet_data)
        {
            set_led_color(Led_Color::Yellow);

            request.type = s_sensor_comms.get_rx_packet_type(packet_data);
            request.signal_s2b = s_sensor_comms.get_input_dBm();
            request.address = s_sensor_comms.get_rx_packet_source_address(packet_data);
            request.needs_response = s_sensor_comms.get_rx_packet_needs_response(packet_data);
            LOGI << "Incoming type " << (int)request.type
                 << ", size " << (int)size
                 << ", address " << (int)request.address
                 << ", signal strength " << (int)request.signal_s2b << "dBm. Sending to manager..." << std::endl;

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
            Server::Result result = s_server.send_sensor_message(request, response);
            if (result == Server::Result::Has_Response)
            {
                LOGI << "\tdone. Sending response back..." << std::endl;
                s_sensor_comms.set_destination_address(response.address);
                s_sensor_comms.begin_packet(raw_packet_data.data(), response.type, false);
                if (!response.payload.empty())
                {
                    s_sensor_comms.pack(raw_packet_data.data(), response.payload.data(), static_cast<uint8_t>(response.payload.size()));
                }

                if (s_sensor_comms.send_packed_packet(raw_packet_data.data(), true))
                {
                    set_led_blink(Led_Blink::Fast_Yellow, std::chrono::seconds(1), true);
                    LOGI << "\tdone" << std::endl;
                }
                else
                {
                    set_led_blink(Led_Blink::Fast_Red, std::chrono::seconds(1), true);
                    LOGE << "\tfailed" << std::endl;
                }
            }
            else if (result != Server::Result::Ok)
            {
                set_led_blink(Led_Blink::Fast_Red, std::chrono::seconds(1), true);
                LOGE << "\tfailed: " << int(result) << std::endl;
            }

            //start again
            s_sensor_comms.start_async_receive();
        }

        s_server.process();
    }

    s_led_thread_on = false;
    return EXIT_SUCCESS;
}

