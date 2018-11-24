#pragma once

#include "Chrono.h"

void init_sleep();


constexpr chrono::micros k_sleep_period(chrono::k_max_timerh_increment);

chrono::time_ms get_next_wakeup_time_point();

void sleep(bool allow_button);
void soft_reset();

