#pragma once

#include <time.h>
#include <inttypes.h>
#include "Chrono.h"

class RX8010S
{
public:
  RX8010S();
  bool init();

  bool setAlarm(chrono::millis duration);

  enum class Frequency
  {
    _4096Hz         = 0,
    _64Hz           = 1,
    _1Hz            = 2
  };
  //IRQ2
  bool enablePeriodicTimer(Frequency freq, uint16_t cycles, chrono::micros& actualDuration);
  bool disablePeriodicTimer();
  bool getTime(time_t& time);
  bool getTime(tm& time);
  bool setTime(const time_t& time);
  bool setTime(const tm& time);

private:
};
