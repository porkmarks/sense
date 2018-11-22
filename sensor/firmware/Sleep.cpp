#include "Sleep.h"
#include "Buttons.h"
#include "LEDs.h"
#include "Arduino_Compat.h"
#include "digitalWriteFast.h"
#include <avr/pgmspace.h>
#include <stdio.h>
#include "avr_stdio.h"

//#include <Arduino.h>
#include <avr/sleep.h>
#include <avr/power.h>

//////////////////////////////////////////////////////////////////////////////////////////

void soft_reset()
{
    printf_P(PSTR("********** resetting..."));
    //cli();
    //wdt_enable(WDTO_15MS);
    //while(1);
    void (*resetptr)( void ) = 0x0000;
    resetptr();
}

//////////////////////////////////////////////////////////////////////////////////////////

namespace chrono
{
    volatile time_us s_time_point;
}

//////////////////////////////////////////////////////////////////////////////////////////

//extern volatile uint16_t s_xxx;

static uint32_t s_main_oscillator_frequency = F_CPU;
static volatile bool s_timer2_interrupt_fired = false;
static volatile uint8_t s_ocr2a = 0;
static volatile uint8_t s_next_ocr2a = 0;

constexpr uint8_t k_min_osccal = 0;
constexpr uint8_t k_max_osccal = 127;

constexpr uint32_t k_crystal_div = 1024; //the normal RTC div
static_assert(1900000ULL * k_crystal_div / 32768ULL < 60000, "Overflow");
static_assert(500000ULL * k_crystal_div / 32768ULL > 30, "Underflow");

static uint16_t s_timer_target_min = 0;
static uint16_t s_timer_target_max = 0;
static volatile int8_t s_timer_pause_cycles = 3; //pause the calibration for this many cycles because there is a glitch with timer1 after waking up

constexpr uint32_t k_min_sleep_period32 = uint32_t(k_min_sleep_period.count);


ISR(TIMER2_COMPA_vect)
{
    //bool ocr2a_changed = s_ocr2a != s_next_ocr2a;
    //digitalWriteFast(14, HIGH);

    //if the clock period didn't change since the prev interrupt
    //and it matches the hot calibration period then calibrate
    if (s_timer_pause_cycles-- == 0)
    {
        s_timer_pause_cycles = 0;
        uint16_t counter = TCNT1;
        if ((TIFR1 & bit(TOV1)) == 0) //no overflow
        {
            //s_xxx = counter;
            if (counter > s_timer_target_max) //too fast, decrease the OSCCAL
            {
                if (OSCCAL > k_min_osccal)
                {
                  OSCCAL--;
                }
            }
            else if (counter < s_timer_target_min) //too slow, increase the OSCCAL
            {
                if (OSCCAL < k_max_osccal)
                {
                  OSCCAL++;
                }
            }
        }          
    }
    TCNT1 = 0;     // clear timer1 counter
    TIFR1 = 0xFF;
    OCR2A = s_next_ocr2a;
  
    chrono::s_time_point.ticks += uint32_t(s_ocr2a + 1) * k_min_sleep_period32;
    s_timer2_interrupt_fired = true;
    s_ocr2a = s_next_ocr2a;

    //digitalWriteFast(14, LOW);
}

volatile bool s_button_interrupt_fired = false;
ISR(INT0_vect)
{
//    for (volatile int i = 0; i < 200; i++)
//      digitalWrite(RED_LED_PIN, HIGH);
//    digitalWrite(RED_LED_PIN, LOW);                
    s_button_interrupt_fired = true;
}

//////////////////////////////////////////////////////////////////////////////////////////

#define SYNC_W_ALL() while (ASSR & (bit(TCN2UB) | bit(TCR2AUB) | bit(TCR2BUB) | bit(OCR2AUB)))
#define SYNC_R_TCNT() do { TCCR2A = TCCR2A; while (ASSR & bit(TCR2AUB)); } while(false)
#define SYNC_W_TCNT() while (ASSR & bit(TCN2UB))
#define DISABLE_INTR() TIMSK2 = 0
#define ENABLE_INTR() do { TIFR2 = 0xFF; TIMSK2 = bit(OCIE2A); } while(false)
#define START_TIMER() do { TCCR2B = bit(CS22) | bit(CS21) | bit(CS20); } while (false)
#define STOP_TIMER() do { TCCR2B = 0; } while (false)

void setup_clock(uint32_t freq)
{
    s_main_oscillator_frequency = freq;
    //attachInterrupt(digitalPinToInterrupt(static_cast<uint8_t>(Button::BUTTON1)), &button_interrupt, FALLING);

    uint16_t target = (s_main_oscillator_frequency * k_crystal_div) >> 15; //>> 15 is divided by 32768
    uint16_t margin = target >> 8; // 0.39% on both sides
    s_timer_target_max = target + margin;
    s_timer_target_min = target - margin;

    //main clock, ~1Mhz
    CLKPR = bit(CLKPCE);
    CLKPR = 0x3;
    OSCCAL = (k_min_osccal + k_max_osccal) / 2;

    //hot calibration setup (timer1)
    TIMSK1 = 0;
    TCCR1A = 0; //normal mode for timer 1
    TCCR1B = bit(CS10);
    
    //setup timer2 crystal oscillator
    DISABLE_INTR();
    STOP_TIMER();
    SYNC_W_ALL();
        
    ASSR |= bit(AS2);

    //1024 prescaler, resulting in a tick every 31.25 millis (1000 / 32 == 31.25)
    //if you change this, make sure to change the chrono::k_period as well
    TCCR2A &= ~(bit(WGM21) | bit(WGM20));
    TCCR2A |= bit(WGM21);
    TCCR2B &= ~bit(WGM22);

    TCNT2 = 0;
    s_ocr2a = 1;
    OCR2A = s_ocr2a;
    s_next_ocr2a = 0;
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

chrono::micros sleep(chrono::micros us, bool allow_button)
{
//    printf_P(PSTR("entering sleep for %ld\n"), (uint32_t)us.count);
    if (us.count < 0)
    {
        printf_P(PSTR("Invalid sleep duration: %ld\n"), (uint32_t)us.count);
        soft_reset();
    }

    uint64_t count = us.count / k_min_sleep_period.count;
    if (count == 0)
    {
        return us;
    }

    s_button_interrupt_fired = false;
    uint8_t button_pin = static_cast<uint8_t>(Button::BUTTON1);
    if (allow_button && (digitalReadFast(button_pin) == LOW || s_button_interrupt_fired == true))
    {
        return us;
    }

    //printf_P(PSTR(">osccal %d\n"), (int)OSCCAL);

    uart_flush();

    //turn off adc
    uint8_t adcsra = ADCSRA;
    ADCSRA &= ~bit(ADEN);

    PRR0 = (uint8_t)(~bit(PRTIM2)); //turn off everything except timer2


    uint16_t q = 0; //this is zero on purpose, so that the first round we don't substract any time as it's a dummy sleep
    uint16_t next_q = count <= 256 ? count : 256;
    bool done = false;
//    printf_P(PSTR("1st %ld\n"), (uint32_t)next_q);
//    Serial.flush();
    for (uint32_t i = 0; done == false; i++)
    {
        s_timer2_interrupt_fired = false;

        if (!allow_button || s_button_interrupt_fired == false)
        {
            s_timer_pause_cycles = 127; //pause calibration
            //this will sleep q, and prepare the next sleep to be next_q
            set_sleep_mode(SLEEP_MODE_PWR_SAVE); 

            cli();
            s_next_ocr2a = uint8_t(next_q - 1); //set the duration of the next next sleep (the one after the next one)
            sleep_enable();
            sei();
            sleep_cpu();
            sleep_disable();
        }
        

        if (allow_button && s_button_interrupt_fired == true)
        {
            //check what is left
            DISABLE_INTR();
            STOP_TIMER();
            SYNC_W_ALL();
            SYNC_R_TCNT();
            uint8_t cycles_done = TCNT2;
            TCNT2 = 0;
            s_ocr2a = 1;
            OCR2A = s_ocr2a;
            s_next_ocr2a = 0;
            START_TIMER();
            SYNC_W_ALL();
            ENABLE_INTR();

            uint64_t us_done = uint64_t(cycles_done) * k_min_sleep_period.count;
            chrono::s_time_point.ticks += us_done;
            us.count -= us_done;
            
            done = true;

//            for (volatile int i = 0; i < 200; i++)
//              digitalWrite(RED_LED_PIN, HIGH);
//            digitalWrite(RED_LED_PIN, LOW);                
                
        }
        else if (s_timer2_interrupt_fired)
        {
            s_timer2_interrupt_fired = false;
            us.count -= uint64_t(q) * k_min_sleep_period.count;

//            printf_P(PSTR("loop1 %ld, %ld, %ld\n"), (uint32_t)count, (uint32_t)next_q, (uint32_t)q);
//            Serial.flush();
            if (count == 0) //we slept the last segment
            {
                done = true; 
            }
            else 
            {
                q = next_q;
                count -= q; //if this reaches zero here, we still do the next round
                if (count == 0) //this is the last round, make sure the next_q will be 1 to go back to normal timer operation (period == 31.25ms or 1q)
                {
                    next_q = 1;
                }
                else
                {
                    next_q = count <= 256 ? count : 256;
                }
            }
//            printf_P(PSTR("loop2 %ld, %ld, %ld\n"), (uint32_t)count, (uint32_t)next_q, (uint32_t)q);

//            for (volatile int i = 0; i < 20; i++)
//              digitalWrite(GREEN_LED_PIN, HIGH);
//            digitalWrite(GREEN_LED_PIN, LOW);
        }
    }
    
    PRR0 = 0;
    //restore adc
    ADCSRA = adcsra;

    s_timer_pause_cycles = 3;

    //printf_P(PSTR("<osccal %d\n"), (int)OSCCAL);

    return us;
}

//////////////////////////////////////////////////////////////////////////////////////////

void sleep_exact(chrono::micros us)
{
    chrono::micros left = sleep(us, false);

    //busy loop the rest
    if (left.count > 0)
    {
        chrono::delay(left);
    }
}

//////////////////////////////////////////////////////////////////////////////////////////

