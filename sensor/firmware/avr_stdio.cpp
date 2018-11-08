#ifndef RASPBERRI_PI

#include "avr_stdio.h"
#include <avr/io.h>

int uart_putchar(char c, FILE* stream)
{
    if (c == '\n')
    {
        uart_putchar('\r', stream);
    }
    loop_until_bit_is_set(UCSR0A, UDRE0);
    UDR0 = c;
    return 0;
}

char uart_getchar(FILE* stream)
{
    return 0;
}


#endif
