#pragma once

constexpr uint8_t RED_LED_PIN  = A1;
constexpr uint8_t GREEN_LED_PIN = A0;

constexpr uint8_t GREEN_LED = 1 << 0;
constexpr uint8_t RED_LED = 1 << 1;
constexpr uint8_t YELLOW_LED = GREEN_LED | RED_LED;

inline void set_leds(uint8_t leds)
{
    digitalWrite(GREEN_LED_PIN, (leds & GREEN_LED) ? HIGH : LOW);
    digitalWrite(RED_LED_PIN, (leds & RED_LED) ? HIGH : LOW);
}

inline void blink_leds(uint8_t leds, uint8_t times, chrono::millis period)
{
    for (uint8_t i = 0; i < times; i++)
    {
        set_leds(leds);
        chrono::millis r = Low_Power::power_down_int(chrono::millis(period.count >> 1));
        chrono::delay(r);
        set_leds(0);
        r = Low_Power::power_down_int(chrono::millis(period.count >> 1));
        chrono::delay(r);
    }
    set_leds(0);
}

inline void fade_in_leds(uint8_t leds, chrono::millis duration)
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

inline void fade_out_leds(uint8_t leds, chrono::millis duration)
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
