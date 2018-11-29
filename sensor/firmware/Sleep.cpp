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
    LOG(PSTR("********** resetting..."));
    //cli();
    //wdt_enable(WDTO_15MS);
    //while(1);
    void (*resetptr)( void ) = 0x0000;
    resetptr();
}

//////////////////////////////////////////////////////////////////////////////////////////

namespace chrono
{
extern volatile uint64_t s_time_pointh;
extern volatile uint32_t s_time_pointl;
}

//////////////////////////////////////////////////////////////////////////////////////////

//extern volatile uint16_t s_xxx;

static volatile bool s_timer2_interrupt_fired = false;


ISR(TIMER2_OVF_vect)
{
//    digitalWriteFast(14, HIGH);

    chrono::s_time_pointh += chrono::k_max_timerh_increment;
    chrono::s_time_pointl = 0;

    PRR0 &= ~bit(PRTIM1); //enable timer1 so we can change its counter
    TCNT1 = 0;
    TIFR1 = 0xFF;
    s_timer2_interrupt_fired = true;

//    digitalWriteFast(14, LOW);
}

//////////////////////////////////////////////////////////////////////////////////////////

volatile bool s_button_interrupt_fired = false;
ISR(INT0_vect)
{
//    for (volatile int i = 0; i < 200; i++)
//      digitalWriteFast(14, HIGH);
//    digitalWriteFast(14, LOW);                
    
    s_button_interrupt_fired = true;
}

//////////////////////////////////////////////////////////////////////////////////////////

#define SYNC_W_ALL() while (ASSR & (bit(TCN2UB) | bit(TCR2AUB) | bit(TCR2BUB) | bit(OCR2AUB)))
#define SYNC_R_TCNT() do { TCCR2A = TCCR2A; while (ASSR & bit(TCR2AUB)); } while(false)
#define SYNC_W_TCNT() while (ASSR & bit(TCN2UB))
#define DISABLE_INTR() TIMSK2 = 0
#define ENABLE_INTR() do { TIFR2 = 0xFF; TIMSK2 = bit(TOIE2); } while(false)
#define START_TIMER() do { TCCR2B = bit(CS22) | bit(CS21) | bit(CS20); } while (false)
#define STOP_TIMER() do { TCCR2B = 0; } while (false)

void init_sleep()
{
    //main clock, ~1Mhz
    CLKPR = bit(CLKPCE);
    CLKPR = 0x3;

    //setup timer2 crystal oscillator
    DISABLE_INTR();
    STOP_TIMER();
    SYNC_W_ALL();
        
    ASSR |= bit(AS2);

    //1024 prescaler, resulting in a tick every 31.25 millis (1000 / 32 == 31.25)
/*    TCCR2A &= ~(bit(WGM21) | bit(WGM20));
    TCCR2A |= bit(WGM21);
    TCCR2B &= ~bit(WGM22);
*/    
    //normal mode
    TCCR2A &= ~(bit(WGM21) | bit(WGM20));
    TCCR2B &= ~bit(WGM22);

    TCNT2 = 0;
    START_TIMER();
    SYNC_W_ALL();

    ENABLE_INTR();

    //button 0 interrupt - falling edge
    EICRA &= ~(bit(ISC00) | bit(ISC01));
    EICRA |= bit(ISC01);
    EIMSK |= bit(INT0);

    sei();
}

//////////////////////////////////////////////////////////////////////////////////////////

void sleep(bool allow_button)
{
    if (allow_button)
    {
        s_button_interrupt_fired = false;
        uint8_t button_pin = static_cast<uint8_t>(Button::BUTTON1);
        if (s_button_interrupt_fired == true || digitalReadFast(button_pin) == LOW)
        {
            return;
        }
    }

    uart_shutdown();
    i2c_shutdown();

    cli();

    //LOG(PSTR(">osccal %d\n"), (int)OSCCAL);

    uint8_t adcsra = ADCSRA;
    ADCSRA &= ~bit(ADEN); //turn off adc
    PRR0 = (uint8_t)(~bit(PRTIM2)); //turn off everything except timer2
    PRR1 = bit(PRTWI1) | bit(PRPTC) | bit(PRTIM4) | bit(PRSPI1) | bit(PRTIM3);
    
    s_timer2_interrupt_fired = false;
    //SYNC_R_TCNT();
    //no need to sync here, as TCNT2 should be readable before sleeping
    uint8_t start_cycle = TCNT2;

    set_sleep_mode(SLEEP_MODE_PWR_SAVE);
    sleep_enable();
    sei();
    sleep_cpu();
    cli();
    sleep_disable();

    //check what is left
    SYNC_R_TCNT();//sync here after sleeping
    uint8_t end_cycle = TCNT2;

    if (allow_button)
    {
      if (s_button_interrupt_fired == true && 
          s_timer2_interrupt_fired == false && 
          end_cycle >= start_cycle)
      {
          chrono::s_time_pointl += uint32_t(end_cycle - start_cycle) * chrono::k_timer2_period_us;
          if (chrono::s_time_pointl > chrono::k_max_timerh_increment)
          {
              chrono::s_time_pointl = chrono::k_max_timerh_increment;
          }
      }
    }
    sei();
    
    ADCSRA = adcsra; //restore adc
    PRR0 = 0;
    PRR1 = 0;

    i2c_init();
    uart_init();

    //LOG(PSTR("s: %ld -> %ld, %d, %d, %ld\n"), ((uint32_t)start_cycle * chrono::k_timer2_period_us) / 1000, ((uint32_t)end_cycle * chrono::k_timer2_period_us) / 1000, s_timer2_interrupt_fired ? 1 : 0, s_button_interrupt_fired ? 1 : 0, (uint32_t)TCNT1);
}

//////////////////////////////////////////////////////////////////////////////////////////

chrono::time_ms get_next_wakeup_time_point()
{
  return chrono::time_ms((chrono::s_time_pointh + chrono::k_max_timerh_increment) / 1000);
}

//////////////////////////////////////////////////////////////////////////////////////////
