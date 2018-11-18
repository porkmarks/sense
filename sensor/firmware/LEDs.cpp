#include "LEDs.h"
#include "Sleep.h"
#include "Arduino_Compat.h"

void set_leds(uint8_t leds)
{
    digitalWriteFast(GREEN_LED_PIN, (leds & GREEN_LED) ? HIGH : LOW);
    digitalWriteFast(RED_LED_PIN, (leds & RED_LED) ? HIGH : LOW);
}

void blink_leds(uint8_t leds, uint8_t times, chrono::millis period)
{
    chrono::millis hp(period.count >> 1);
    for (uint8_t i = 0; i < times; i++)
    {
        set_leds(leds);
        sleep_exact(hp);
        set_leds(0);
        sleep_exact(hp);
    }
    set_leds(0);
}

