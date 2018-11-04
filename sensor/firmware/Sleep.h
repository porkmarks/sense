#pragma once

#include "Chrono.h"

void setup_clock(uint32_t freq);

chrono::micros sleep(chrono::micros us, bool allow_button);

template< typename T>
chrono::micros sleep(T duration, bool allow_button)
{
    return sleep(chrono::micros(duration), allow_button);
}

void sleep_exact(chrono::micros us);
template< typename T>
void sleep_exact(T duration)
{
    return sleep_exact(chrono::micros(duration));
}

void soft_reset();

