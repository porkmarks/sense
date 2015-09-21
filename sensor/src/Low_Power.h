#pragma once

#include <stdint.h>

enum Period_t
{
    SLEEP_15MS,
    SLEEP_30MS,
    SLEEP_60MS,
    SLEEP_120MS,
    SLEEP_250MS,
    SLEEP_500MS,
    SLEEP_1S,
    SLEEP_2S,
    SLEEP_4S,
    SLEEP_8S,
    SLEEP_FOREVER
};

enum BOD_t
{
    BOD_OFF,
    BOD_ON
};

enum ADC_t
{
    ADC_OFF,
    ADC_ON
};

enum Timer5_t
{
    TIMER5_OFF,
    TIMER5_ON
};

enum Timer4_t
{
    TIMER4_OFF,
    TIMER4_ON
};

enum Timer3_t
{
    TIMER3_OFF,
    TIMER3_ON
};

enum Timer2_t
{
    TIMER2_OFF,
    TIMER2_ON
};

enum Timer1_t
{
    TIMER1_OFF,
    TIMER1_ON
};

enum Timer0_t
{
    TIMER0_OFF,
    TIMER0_ON
};

enum SPI_t
{
    SPI_OFF,
    SPI_ON
};

enum USART0_t
{
    USART0_OFF,
    USART0_ON
};

enum USART1_t
{
    USART1_OFF,
    USART1_ON
};

enum USART2_t
{
    USART2_OFF,
    USART2_ON
};

enum USART3_t
{
    USART3_OFF,
    USART3_ON
};

enum TWI_t
{
    TWI_OFF,
    TWI_ON
};

enum USB_t
{
    USB_OFF,
    USB_ON
};

class Low_Power
{
public:
#if defined (__AVR_ATmega328P__) || defined (__AVR_ATmega168__)
    static void	idle(Period_t period, ADC_t adc, Timer2_t timer2,
                 Timer1_t timer1, Timer0_t timer0, SPI_t spi,
                 USART0_t usart0, TWI_t twi);
#elif defined __AVR_ATmega2560__
    static void	idle(Period_t period, ADC_t adc, Timer5_t timer5,
                 Timer4_t timer4, Timer3_t timer3, Timer2_t timer2,
                 Timer1_t timer1, Timer0_t timer0, SPI_t spi,
                 USART3_t usart3, USART2_t usart2, USART1_t usart1,
                 USART0_t usart0, TWI_t twi);
#elif defined __AVR_ATmega32U4__
    static void	idle(Period_t period, ADC_t adc, Timer4_t timer4, Timer3_t timer3,
                 Timer1_t timer1, Timer0_t timer0, SPI_t spi,
                 USART1_t usart1, TWI_t twi, USB_t usb);
#else
#error "Please ensure chosen MCU is either 328P, 32U4 or 2560."
#endif
    static void	adc_noise_reduction(Period_t period, ADC_t adc, Timer2_t timer2);

    static void	power_down(Period_t period, ADC_t adc = ADC_OFF, BOD_t bod = BOD_OFF);
    static uint32_t power_down(uint32_t millis, ADC_t adc = ADC_OFF, BOD_t bod = BOD_OFF);
    static uint32_t power_down_int(uint32_t millis, ADC_t adc = ADC_OFF, BOD_t bod = BOD_OFF);

    static void	power_save(Period_t period, ADC_t adc = ADC_OFF, BOD_t bod = BOD_OFF, Timer2_t timer2 = TIMER2_OFF);
    static void	power_standby(Period_t period, ADC_t adc = ADC_OFF, BOD_t bod = BOD_OFF);
    static void	power_ext_standby(Period_t period, ADC_t adc = ADC_OFF, BOD_t bod = BOD_OFF, Timer2_t timer2 = TIMER2_OFF);

    static uint64_t s_sleep_time;

    static bool s_interrupt_fired;
};

