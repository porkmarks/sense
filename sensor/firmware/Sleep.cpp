#include "Sleep.h"
#include "Buttons.h"
#include "LEDs.h"

#include <Arduino.h>
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

static const uint64_t k_period_us = 31250ULL;

namespace chrono
{
    volatile time_us s_time_point;
}

//////////////////////////////////////////////////////////////////////////////////////////

static uint32_t s_main_oscillator_frequency = F_CPU;
volatile bool s_timer2_interrupt_fired = false;
volatile uint8_t s_ocr2a = 0;
volatile uint8_t s_next_ocr2a = 0;

ISR(TIMER2_COMPA_vect)
{
    bool ocr2a_changed = s_ocr2a != s_next_ocr2a;

    //if the clock period didn't change since the prev interrupt
    //and it matches the hot calibration period then calibrate
    if (s_ocr2a == 0) 
    {
        constexpr uint32_t k_crystal_div = 1024; //the normal RTC div
        static_assert(1900000ULL / (32768ULL / k_crystal_div) < 60000, "Overflow");
        static_assert(500000ULL / (32768ULL / k_crystal_div) > 30, "Underflow");
        constexpr uint8_t k_min_v = 0;
        constexpr uint8_t k_max_v = 127;
      
        if ((TIFR1 & bit(TOV1)) == 0) //no overflow
        {
            const uint16_t target = s_main_oscillator_frequency / (32768ULL / k_crystal_div);
            const uint16_t margin = (target / 100) / 2; //0.5% on both sides
            uint16_t counter = TCNT1;
            if (counter > target + margin && OSCCAL > k_min_v) //too fast, decrease the OSCCAL
            {
                OSCCAL--;
            }
            else if (counter < target - margin && OSCCAL < k_max_v) //too slow, increase the OSCCAL
            {
                OSCCAL++;
            }
        }          
    }
    TCNT1 = 0;     // clear timer1 counter
    TIFR1 = 0xFF;
  
    chrono::s_time_point.ticks += uint64_t(s_ocr2a + 1) * k_period_us;
    s_timer2_interrupt_fired = true;
    if (ocr2a_changed)
    {
        OCR2A = s_next_ocr2a;
        s_ocr2a = s_next_ocr2a;
    }
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
    //main clock, ~1Mhz
    clock_prescale_set(clock_div_8);

    //hot calibration setup (timer1)
    TIMSK1 = 0;
    TCCR1A = 0; //normal mode for timer 1
    TCCR1B = bit(CS10);
    s_main_oscillator_frequency = freq;
    
    //setup timer2 crystal oscillator
    DISABLE_INTR();
    STOP_TIMER();
    SYNC_W_ALL();
        
    ASSR |= bit(AS2);

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

    uint64_t count = us.count / k_period_us;
    if (count == 0)
    {
        return us;
    }

    s_button_interrupt_fired = false;
    uint8_t button_pin = static_cast<uint8_t>(Button::BUTTON1);
    if (allow_button && (digitalRead(button_pin) == LOW || s_button_interrupt_fired == true))
    {
        return us;
    }

    //turn off adc
    uint8_t adcsra = ADCSRA;
    ADCSRA &= ~bit(ADEN);

//    PRR = (uint8_t)(~bit(PRTIM2)); //turn off everything except timer2


    uint16_t q = 0; //this is zero on purpose, so that the first round we don't substract any time as it's a dummy sleep
    uint16_t next_q = count <= 256 ? count : 256;
    bool done = false;
//    printf_P(PSTR("1st %ld\n"), (uint32_t)next_q);
//    Serial.flush();
    for (uint32_t i = 0; done == false; i++)
    {
        s_timer2_interrupt_fired = false;

        //set the duration of the next next sleep (the one after the next one)
        {
            Scope_Sync ss;
            s_next_ocr2a = uint8_t(next_q - 1);
        }

        if (!allow_button || s_button_interrupt_fired == false)
        {
            //this will sleep q, and prepare the next sleep to be next_q
            set_sleep_mode(SLEEP_MODE_PWR_SAVE); 
            sleep_mode();
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

            uint64_t us_done = uint64_t(cycles_done) * k_period_us;
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
            us.count -= uint64_t(q) * k_period_us;

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
    
    PRR = 0;
    //restore adc
    ADCSRA = adcsra;

    return us;
}

//////////////////////////////////////////////////////////////////////////////////////////

void sleep_exact(chrono::micros us)
{
    chrono::micros left = sleep(us, false);

    //busy loop the rest
    if (left.count > 0)
    {
        chrono::millis ms(left);
        if (ms.count > 0)
        {
            ::delay(ms.count);
            left.count -= ms.count * 1000;
        }
        if (left.count > 0)
        {
            ::delayMicroseconds(left.count);
        }
    }
}

//////////////////////////////////////////////////////////////////////////////////////////

