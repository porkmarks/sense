#include "LEDs.h"
#include "Sleep.h"

void set_leds(uint8_t leds)
{
    digitalWrite(GREEN_LED_PIN, (leds & GREEN_LED) ? HIGH : LOW);
    digitalWrite(RED_LED_PIN, (leds & RED_LED) ? HIGH : LOW);
}

void blink_leds(uint8_t leds, uint8_t times, chrono::millis period)
{
    for (uint8_t i = 0; i < times; i++)
    {
        set_leds(leds);
        chrono::micros r = sleep(chrono::millis(period.count >> 1), true);
        chrono::delay(r);
        set_leds(0);
        r = sleep(chrono::millis(period.count >> 1), true);
        chrono::delay(r);
    }
    set_leds(0);
}

void fade_in_leds(uint8_t leds, chrono::millis duration)
{
    if (duration.count == 0)
    {
        set_leds(leds);
        return;
    }

    chrono::time_ms start = chrono::now();
    do
    {
        chrono::millis elapsed = chrono::now() - start;
        if (elapsed > duration)
        {
            break;
        }

        uint32_t duty_cycle = 1024ULL * elapsed.count / duration.count;
        set_leds(leds);
        delayMicroseconds(duty_cycle);
        set_leds(0);
        delayMicroseconds(1024ULL - duty_cycle);
    } while (true);

    set_leds(leds);
}

void fade_out_leds(uint8_t leds, chrono::millis duration)
{
    if (duration.count == 0)
    {
        set_leds(0);
        return;
    }

    chrono::time_ms start = chrono::now();
    do
    {
        chrono::millis elapsed = chrono::now() - start;
        if (elapsed > duration)
        {
            break;
        }

        uint32_t duty_cycle = 1024ULL * elapsed.count / duration.count;
        set_leds(0);
        delayMicroseconds(duty_cycle);
        set_leds(leds);
        delayMicroseconds(1024ULL - duty_cycle);
    } while (true);

    set_leds(0);
}
