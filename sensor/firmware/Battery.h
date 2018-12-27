#pragma once

#include <stdint.h>

void init_battery();
float read_battery(float vref);
float read_battery(uint8_t vref);
void battery_guard_auto(float vref);
void battery_guard_auto(uint8_t vref);
void battery_guard_manual(float vcc);

void start_battery_monitor();
float stop_battery_monitor(float vref);
float stop_battery_monitor(uint8_t vref);

class Battery_Monitor
{
public:
    Battery_Monitor(float vref);
    Battery_Monitor(uint8_t vref);
    Battery_Monitor(float vref, float& vcc);
    Battery_Monitor(uint8_t vref, float& vcc);
    ~Battery_Monitor();
private:
  float vref;
  float* vcc = nullptr;
};
