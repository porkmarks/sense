#pragma once

#include "Chrono.h"

constexpr uint8_t RED_LED_PIN  = A1;
constexpr uint8_t GREEN_LED_PIN = A0;

constexpr uint8_t GREEN_LED = 1 << 0;
constexpr uint8_t RED_LED = 1 << 1;
constexpr uint8_t YELLOW_LED = GREEN_LED | RED_LED;

void set_leds(uint8_t leds);
void blink_leds(uint8_t leds, uint8_t times, chrono::millis period);
void fade_in_leds(uint8_t leds, chrono::millis duration);
void fade_out_leds(uint8_t leds, chrono::millis duration);

