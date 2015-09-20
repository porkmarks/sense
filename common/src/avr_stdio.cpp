#ifndef RASPBERRI_PI

#include "avr_stdio.h"
#include <Arduino.h>

int uart_putchar(char c, FILE* stream)
{
    if (c == '\n')
    {
        Serial.print('\r');
    }
    Serial.print(c);
    Serial.flush();
    return 0;
}

char uart_getchar(FILE* stream)
{
    return 0;
}


#endif
