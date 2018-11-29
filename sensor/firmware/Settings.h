#pragma once

#include "Arduino_Compat.h"
#include "Data_Defs.h"

struct Settings
{
    uint32_t address = 0;
    data::sensor::Calibration calibration;
};

struct Stable_Settings
{
    uint32_t serial_number = 0;
    uint8_t vref = 110; //*100
};

void reset_settings();
void save_settings(Settings const& settings);
bool load_settings(Settings& settings);

void reset_stable_settings();
void save_stable_settings(Stable_Settings const& settings);
bool load_stable_settings(Stable_Settings& settings);

