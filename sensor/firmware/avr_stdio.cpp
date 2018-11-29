#ifndef RASPBERRI_PI

#define BAUD 57600
#include <util/setbaud.h>

#include "Arduino_Compat.h"
#include "avr_stdio.h"
#include <avr/io.h>

void uart_init()
{
    UCSR0B |= bit(RXEN0) | bit(TXEN0); 
    UBRR0L = UBRRL_VALUE;
    UBRR0H = UBRRH_VALUE;
}

void uart_shutdown()
{
  uart_flush();
  UCSR0B &= ~(bit(RXEN0) | bit(TXEN0));
}

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

void uart_flush()
{
    loop_until_bit_is_set(UCSR0A, UDRE0);
}


#endif
