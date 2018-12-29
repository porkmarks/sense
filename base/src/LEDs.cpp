#include "LEDs.h"
#include <cstdint>
#include <functional>
#include <pigpio.h>
#include "Chrono.h"

constexpr uint32_t RED_LED_PIN = 4;
constexpr uint32_t GREEN_LED_PIN = 17;

constexpr int k_red_pwm = 30;


LEDs::LEDs()
{
    m_thread = std::thread(std::bind(&LEDs::thread_func, this));
}

LEDs::~LEDs()
{
    m_thread_on = false;
    if (m_thread.joinable())
    {
        m_thread.join();
    }
}


void LEDs::set_color(Color color)
{
    gpioPWM(RED_LED_PIN, 0);
    gpioWrite(RED_LED_PIN, 0);
    gpioWrite(GREEN_LED_PIN, 0);

    if (color == Color::Green)
    {
        gpioWrite(RED_LED_PIN, 0);
        gpioWrite(GREEN_LED_PIN, 1);
    }
    else if (color == Color::Red)
    {
        gpioWrite(RED_LED_PIN, 1);
        gpioWrite(GREEN_LED_PIN, 0);
    }
    else if (color == Color::Yellow)
    {
        gpioPWM(RED_LED_PIN, k_red_pwm);
        gpioWrite(GREEN_LED_PIN, 1);
    }
    m_color = color;
}

void LEDs::thread_func()
{
    gpioSetMode(RED_LED_PIN, PI_OUTPUT);
    gpioSetMode(GREEN_LED_PIN, PI_OUTPUT);
    gpioWrite(RED_LED_PIN, 0);
    gpioWrite(GREEN_LED_PIN, 0);

    while (m_thread_on)
    {
        auto now = std::chrono::system_clock::now();
        if (m_blink_time_point > now && m_blink != Blink::None)
        {
            gpioPWM(RED_LED_PIN, 0);
            gpioWrite(RED_LED_PIN, 0);
            gpioWrite(GREEN_LED_PIN, 0);

            if (m_blink == Blink::Slow_Red)
            {
                gpioWrite(RED_LED_PIN, 0);
                gpioWrite(GREEN_LED_PIN, 0);
                chrono::delay(chrono::millis(200));
                gpioWrite(RED_LED_PIN, 1);
                gpioWrite(GREEN_LED_PIN, 0);
                chrono::delay(chrono::millis(200));
            }
            else if (m_blink == Blink::Slow_Green)
            {
                gpioWrite(RED_LED_PIN, 0);
                gpioWrite(GREEN_LED_PIN, 0);
                chrono::delay(chrono::millis(200));
                gpioWrite(RED_LED_PIN, 0);
                gpioWrite(GREEN_LED_PIN, 1);
                chrono::delay(chrono::millis(200));
            }
            else if (m_blink == Blink::Slow_Yellow)
            {
                gpioPWM(RED_LED_PIN, 0);
                gpioWrite(GREEN_LED_PIN, 0);
                chrono::delay(chrono::millis(200));
                gpioPWM(RED_LED_PIN, k_red_pwm);
                gpioWrite(GREEN_LED_PIN, 1);
                chrono::delay(chrono::millis(200));
            }
            else if (m_blink == Blink::Slow_Green_Yellow)
            {
                gpioWrite(RED_LED_PIN, 0);
                gpioWrite(GREEN_LED_PIN, 1);
                chrono::delay(chrono::millis(200));
                gpioPWM(RED_LED_PIN, k_red_pwm);
                gpioWrite(GREEN_LED_PIN, 1);
                chrono::delay(chrono::millis(200));
            }
            else if (m_blink == Blink::Fast_Red)
            {
                gpioWrite(RED_LED_PIN, 0);
                gpioWrite(GREEN_LED_PIN, 0);
                chrono::delay(chrono::millis(50));
                gpioWrite(RED_LED_PIN, 1);
                gpioWrite(GREEN_LED_PIN, 0);
                chrono::delay(chrono::millis(50));
            }
            else if (m_blink == Blink::Fast_Green)
            {
                gpioWrite(RED_LED_PIN, 0);
                gpioWrite(GREEN_LED_PIN, 0);
                chrono::delay(chrono::millis(50));
                gpioWrite(RED_LED_PIN, 0);
                gpioWrite(GREEN_LED_PIN, 1);
                chrono::delay(chrono::millis(50));
            }
            else if (m_blink == Blink::Fast_Yellow)
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
            set_color(m_color);
            chrono::delay(chrono::millis(10));
        }
    }
}

void LEDs::set_blink(Blink blink, std::chrono::system_clock::duration duration, bool force)
{
    auto now = std::chrono::system_clock::now();
    if (now >= m_blink_time_point || m_blink == Blink::None || force)
    {
        m_blink = blink;
        m_blink_time_point = std::chrono::system_clock::now() + duration;
    }
}



