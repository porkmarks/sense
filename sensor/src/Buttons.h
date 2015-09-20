#pragma once

#include "Low_Power.h"

enum class Button
{
    BUTTON1 = 2 //the pin!!!
};

inline bool is_pressed(Button b)
{
    return digitalRead(static_cast<uint8_t>(b)) == LOW;
}
inline void wait_for_press(Button b)
{
    while (!is_pressed(b))
    {
        Low_Power::power_down_int(120);
    }
}
inline void wait_for_release(Button b)
{
    while (is_pressed(b))
    {
        Low_Power::power_down_int(120);
    }
}
