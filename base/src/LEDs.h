#pragma once

#include <chrono>
#include <atomic>
#include <thread>

class LEDs
{
public:
    LEDs();
    ~LEDs();

    enum class Color
    {
        None,
        Green,
        Red,
        Yellow,
    };

    enum class Blink
    {
        None,
        Slow_Green,
        Slow_Red,
        Slow_Yellow,
        Slow_Green_Yellow,
        Fast_Green,
        Fast_Red,
        Fast_Yellow,
    };

    void set_color(Color color);
    void set_blink(Blink blink, std::chrono::system_clock::duration duration, bool force = false);

private:
    void thread_func();

    Color m_color = Color::None;
    std::atomic<Blink> m_blink;
    std::chrono::system_clock::time_point m_blink_time_point;
    volatile bool m_thread_on = true;
    std::thread m_thread;
};

