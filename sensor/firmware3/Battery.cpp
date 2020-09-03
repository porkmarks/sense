#include "Battery.h"
#include "Log.h"
#include "LEDs.h"
#include "Sleep.h"
#include "Arduino_Compat.h"
#include <avr/io.h>
#include <avr/power.h>
#include <compat/deprecated.h>

///////////////////////////////////////////////////////////////////////////////////////

void init_battery()
{
    //1000000 /128 = 7.8KHz
    sbi(ADCSRA, ADPS2);
    sbi(ADCSRA, ADPS1);
    sbi(ADCSRA, ADPS0);
}

///////////////////////////////////////////////////////////////////////////////////////

void enable_battery_adc()
{
    // Read 1.1V reference against AVcc
    // set the reference to Vcc and the measurement to the internal 1.1V reference
    ADMUX = bit(REFS0) | bit(MUX3) | bit(MUX2) | bit(MUX1);

    // enable a2d conversions
    sbi(ADCSRA, ADEN);

    chrono::delay(chrono::millis(20)); // Wait for Vref to settle
}

///////////////////////////////////////////////////////////////////////////////////////

void disable_battery_adc()
{
    cbi(ADCSRA, ADEN);
}

///////////////////////////////////////////////////////////////////////////////////////

float compute_vcc(uint16_t adc, float vref)
{
    float vcc = adc == 0 ? 0.f : vref * 1024.f / adc;
    return vcc;
}

///////////////////////////////////////////////////////////////////////////////////////

float read_battery(uint8_t vref)
{
    return read_battery(vref * 0.01f);
}

///////////////////////////////////////////////////////////////////////////////////////

float read_battery(float vref)
{
    enable_battery_adc();
  
    constexpr uint8_t k_skip_count = 10;
    for (uint8_t i = 0; i < k_skip_count; i++) //skip a bunch of samples... desperate to get this stable
    {
        ADCSRA |= bit(ADSC); // Start conversion
        while (bit_is_set(ADCSRA, ADSC)); // measuring
    }
    
    uint32_t result = 0;
    constexpr uint8_t k_measurement_count = 10;
    for (uint8_t i = 0; i < k_measurement_count; i++) //read a bunch of samples and average them
    {
        ADCSRA |= bit(ADSC); // Start conversion
        while (bit_is_set(ADCSRA, ADSC)); // measuring
        uint8_t low  = ADCL; // must read ADCL first - it then locks ADCH
        uint8_t high = ADCH; // unlocks both
        result += (high<<8) | low;
    }

    disable_battery_adc();

    result /= k_measurement_count;
    return compute_vcc(uint16_t(result), vref);
}

///////////////////////////////////////////////////////////////////////////////////////

void battery_guard_auto(float vref, float min_vcc)
{
    battery_guard_manual(read_battery(vref), min_vcc);
}

///////////////////////////////////////////////////////////////////////////////////////

void battery_guard_auto(uint8_t vref, float min_vcc)
{
    battery_guard_manual(read_battery(vref), min_vcc);
}

///////////////////////////////////////////////////////////////////////////////////////

void battery_guard_manual(float vcc, float min_vcc)
{
    if (vcc > min_vcc)
    {
        return;
    }
    while (true)
    {
        LOG(PSTR("VCC failed\n"));
        blink_led(Blink_Led::Red, 5, chrono::millis(200));
        sleep_until_interrupt();
    }
}

///////////////////////////////////////////////////////////////////////////////////////

volatile uint16_t s_battery_max_adc = 0;
//volatile int xxx = 0;
ISR (TIMER3_COMPA_vect)
{
    while (bit_is_set(ADCSRA, ADSC)); // measuring
    uint8_t low  = ADCL; // must read ADCL first - it then locks ADCH
    uint8_t high = ADCH; // unlocks both
    uint16_t adc = (uint16_t(high) << 8) | uint16_t(low);
    ADCSRA |= bit(ADSC); // Start next conversion
    if (adc > s_battery_max_adc)
    {
        s_battery_max_adc = adc;
    }
//    xxx++;
}

void start_battery_monitor()
{
//    xxx = 0;
    s_battery_max_adc = 0;
    enable_battery_adc();
    
    TCCR3A &= ~(bit(WGM31) | bit(WGM30));
    TCCR3B &= ~(bit(WGM33) | bit(WGM32));
    TCCR3B |= bit(WGM32); //CTC mode

    OCR3A = 3; //so ~128Hz (1024/(2 * (OCR3A + 1))
    
    TIMSK3 |= bit(OCIE3A);
    TCCR3B |= bit(CS32) | bit(CS30); //1024 divider

    ADCSRA |= bit(ADSC); // Start conversion
}

///////////////////////////////////////////////////////////////////////////////////////

float stop_battery_monitor(uint8_t vref)
{
    return stop_battery_monitor(vref * 0.01f);
}

///////////////////////////////////////////////////////////////////////////////////////

float stop_battery_monitor(float vref)
{
    cli();
    
    TCCR3B &= ~(bit(CS32) | bit(CS31) | bit(CS30));
    TIMSK3 &= ~bit(OCIE3A);

    disable_battery_adc();
    
    sei();

    //printf_P(PSTR("vcc count=%d\n"), xxx);

    return compute_vcc(s_battery_max_adc, vref);
}

///////////////////////////////////////////////////////////////////////////////////////

Battery_Monitor::Battery_Monitor(float vref, float min_vcc)
  : vref(vref)
  , min_vcc(min_vcc)
{
  start_battery_monitor();
}
Battery_Monitor::Battery_Monitor(uint8_t vref, float min_vcc)
  : vref(vref * 0.01f)
  , min_vcc(min_vcc)
{
  start_battery_monitor();
}
Battery_Monitor::Battery_Monitor(float vref, float& vcc, float min_vcc)
  : vref(vref)
  , vcc(&vcc)
  , min_vcc(min_vcc)
{
  start_battery_monitor();
}
Battery_Monitor::Battery_Monitor(uint8_t vref, float& vcc, float min_vcc)
  : vref(vref * 0.01f)
  , vcc(&vcc)
  , min_vcc(min_vcc)
{
  start_battery_monitor();
}

#define lerp(a, b, mu) ((1.f - mu) * (a) + mu * (b))

Battery_Monitor::~Battery_Monitor()
{
  float new_vcc = stop_battery_monitor(vref);
//  printf_P(PSTR("monitor done, vcc =%ld, count=%d\n"), (int32_t)(new_vcc * 1000.f), xxx);
  battery_guard_manual(new_vcc, min_vcc);
  if (vcc != nullptr)
  {
    float mu = (new_vcc < *vcc) ? 0.1f : 0.01f;
    *vcc = lerp(*vcc, new_vcc, mu);
  }
}
