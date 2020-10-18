#include "Sleep.h"
#include "Buttons.h"
#include "LEDs.h"
#include "Arduino_Compat.h"
#include "digitalWriteFast.h"
#include <avr/pgmspace.h>
#include <stdio.h>
#include "avr_stdio.h"
#include "i2c.h"

//#include <Arduino.h>
#include <avr/sleep.h>
#include <avr/power.h>
#include "Log.h"

//////////////////////////////////////////////////////////////////////////////////////////

void soft_reset()
{
    LOG(PSTR("XOXOXOXO"));
    //cli();
    //wdt_enable(WDTO_15MS);
    //while(1);
    void (*resetptr)( void ) = 0x0000;
    resetptr();
}

//////////////////////////////////////////////////////////////////////////////////////////

//extern volatile uint16_t s_xxx;

//////////////////////////////////////////////////////////////////////////////////////////

volatile bool s_button_interrupt_fired = false;
ISR(INT0_vect)
{
//    for (volatile int i = 0; i < 200; i++)
//      digitalWriteFast(14, HIGH);
//    digitalWriteFast(14, LOW);                
    
    s_button_interrupt_fired = true;
}

volatile bool s_pcint2_interrupt_fired = false;
ISR(PCINT2_vect)
{
    s_pcint2_interrupt_fired = true;
}

//////////////////////////////////////////////////////////////////////////////////////////

//stuff that is always disabled since I don't use
constexpr uint8_t k_prr0Value = bit(PRUSART1) | bit(PRTIM2);
constexpr uint8_t k_prr1Value = bit(PRTWI1) | bit(PRPTC) | bit(PRTIM4) | bit(PRSPI1);

void init_sleep()
{
    //main clock, ~1Mhz
    CLKPR = bit(CLKPCE);
    CLKPR = 0x3;
    
    //button 0 interrupt - falling edge
    EICRA &= ~(bit(ISC00) | bit(ISC01));
    EICRA |= bit(ISC01);
    EIMSK |= bit(INT0);

    PCICR |= bit(PCIE2);
    PCMSK2 |= bit(3); //PCINT19/PD3/CK_IRQ1
    PCMSK2 |= bit(4); //PCINT20/PD4/CK_IRQ2

    PRR0 = k_prr0Value;
    PRR1 = k_prr1Value;

    sei();
}

//////////////////////////////////////////////////////////////////////////////////////////

void sleep_until_interrupt()
{
    s_button_interrupt_fired = false;
    uint8_t button_pin = static_cast<uint8_t>(Button::BUTTON1);
    if (s_button_interrupt_fired == true || digitalReadFast(button_pin) == LOW)
    {
        return;
    }

    uart_shutdown();
    i2c_shutdown();

    cli();

    s_pcint2_interrupt_fired = false;

    //LOG(PSTR(">osccal %d\n"), (int)OSCCAL);

    uint8_t adcsra = ADCSRA;
    ADCSRA &= ~bit(ADEN); //turn off adc
    
    set_sleep_mode(SLEEP_MODE_PWR_SAVE);
    sleep_enable();
    sei();
    sleep_cpu();
    cli();
    sleep_disable();

    sei();
    
    ADCSRA = adcsra; //restore adc

    i2c_init();
    uart_init();

    //LOG(PSTR("Butt:%d, INT:%d"), s_button_interrupt_fired ? 1 : 0, s_pcint2_interrupt_fired ? 1 : 0);
    
    //LOG(PSTR("s: %ld -> %ld, %d, %d, %ld\n"), ((uint32_t)start_cycle * chrono::k_timer2_period_us) / 1000, ((uint32_t)end_cycle * chrono::k_timer2_period_us) / 1000, s_timer2_interrupt_fired ? 1 : 0, s_button_interrupt_fired ? 1 : 0, (uint32_t)TCNT1);
}

//////////////////////////////////////////////////////////////////////////////////////////
