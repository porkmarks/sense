#include "Buttons.h"
#include "Sleep.h"

bool is_pressed(Button b)
{
    return digitalRead(static_cast<uint8_t>(b)) == LOW;
}
void wait_for_press(Button b)
{
    while (!is_pressed(b))
    {
        sleep(chrono::millis(120), true);
    }
}
void wait_for_release(Button b)
{
    while (is_pressed(b))
    {
        sleep(chrono::millis(120), true);
    }
}
