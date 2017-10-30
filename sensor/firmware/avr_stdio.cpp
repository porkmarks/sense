#ifndef RASPBERRI_PI

#include "avr_stdio.h"
#include <avr/eeprom.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/wdt.h>
//#include <Arduino.h>
#include <HardwareSerial.h>

int uart_putchar(char c, FILE* stream)
{
//    if (c == '\n')
//        uart_putchar('\r', stream);
//    loop_until_bit_is_set(UCSR0A, UDRE0);
//    UDR0 = c;
//    return 0;
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
