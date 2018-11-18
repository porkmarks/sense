#pragma once

#include "Chrono.h"

constexpr uint8_t RED_LED_PIN  = 14;
constexpr uint8_t GREEN_LED_PIN = 15;

constexpr uint8_t GREEN_LED = 1 << 0;
constexpr uint8_t RED_LED = 1 << 1;
constexpr uint8_t YELLOW_LED = GREEN_LED | RED_LED;

void set_leds(uint8_t leds);
void blink_leds(uint8_t leds, uint8_t times, chrono::millis period);

