#pragma once

#ifdef __AVR__
#   include "digitalWriteFast.h"
#   include <Arduino.h>
#else
#   include <cstring>
#   include <cmath>
#   include "pigpio.h"
#   define printf_P printf
#   define F
#   define PSTR
#   define pinModeFast gpioSetMode
#   define digitalReadFast gpioRead
#   define digitalWriteFast gpioWrite
#   define INPUT PI_INPUT
#   define LOW PI_LOW
#   define HIGH PI_HIGH
#endif
