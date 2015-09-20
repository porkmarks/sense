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

inline void blink_leds(uint8_t leds, uint8_t times, uint16_t period)
{
    for (uint8_t i = 0; i < times; i++)
    {
        set_leds(leds);
        uint32_t r = Low_Power::power_down_int(period >> 1);
        delay(r);
        set_leds(0);
        r = Low_Power::power_down_int(period >> 1);
        delay(r);
    }
    set_leds(0);
}

inline void fade_in_leds(uint8_t leds, uint16_t duration)
{
    if (duration == 0)
    {
        set_leds(leds);
        return;
    }

    uint32_t start = millis();
    do
    {
        uint32_t elapsed = millis() - start;
        if (elapsed > duration)
        {
            break;
        }

        uint32_t duty_cycle = 1024ULL * elapsed / duration;
        set_leds(leds);
        delayMicroseconds(duty_cycle);
        set_leds(0);
        delayMicroseconds(1024ULL - duty_cycle);
    } while (true);

    set_leds(leds);
}

inline void fade_out_leds(uint8_t leds, uint16_t duration)
{
    if (duration == 0)
    {
        set_leds(0);
        return;
    }

    uint32_t start = millis();
    do
    {
        uint32_t elapsed = millis() - start;
        if (elapsed > duration)
        {
            break;
        }

        uint32_t duty_cycle = 1024ULL * elapsed / duration;
        set_leds(0);
        delayMicroseconds(duty_cycle);
        set_leds(leds);
        delayMicroseconds(1024ULL - duty_cycle);
    } while (true);

    set_leds(0);
}
