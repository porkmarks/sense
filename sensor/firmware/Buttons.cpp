#include "Buttons.h"
#include "Sleep.h"
#include "Arduino_Compat.h"

void init_buttons()
{
    Scope_Sync ss;
    pinModeFast(static_cast<uint8_t>(Button::BUTTON1), INPUT);
    digitalWriteFast(static_cast<uint8_t>(Button::BUTTON1), HIGH);
}

bool is_pressed(Button b)
{
    return digitalReadFast(static_cast<uint8_t>(b)) == LOW;
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
