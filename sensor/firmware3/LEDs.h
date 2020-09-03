#pragma once

#include "Chrono.h"

enum class Led
{
  None    = 0,
  Green   = 1,
  Red     = 2,
  Yellow  = 3
};

enum class Blink_Led
{
  Green   = 1, //to match the Led enum
  Red     = 2,
  Yellow  = 3
};

void init_leds();
void set_led(Led led);
void blink_led(Blink_Led led, uint8_t times, chrono::millis period);
