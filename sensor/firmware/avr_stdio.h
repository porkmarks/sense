#pragma once

#include <stdio.h>

void uart_init();
void uart_shutdown();
void uart_flush();

int uart_putchar(char c, FILE *stream);
char uart_getchar(FILE *stream);

