#pragma once

#include "Log.h"
#include "Sleep.h"

constexpr float k_min_battery_vcc = 2.3f;

void init_adc()
{
    //1000000 /128 = 7.8KHz
    sbi(ADCSRA, ADPS2);
    sbi(ADCSRA, ADPS1);
    sbi(ADCSRA, ADPS0);
}

float read_vcc(float vref)
{
    // enable a2d conversions
    sbi(ADCSRA, ADEN);

    // Read 1.1V reference against AVcc
    // set the reference to Vcc and the measurement to the internal 1.1V reference
    ADMUX = bit(REFS0) | bit(MUX3) | bit(MUX2) | bit(MUX1);
  
    chrono::delay(chrono::millis(20)); // Wait for Vref to settle
  
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

    cbi(ADCSRA, ADEN);


    result /= k_measurement_count;

    float vcc = vref * 1024.f / result;
    return vcc;
}

void battery_guard(float vref)
{
    do 
    {
        float vcc = read_vcc(vref);
        if (vcc > k_min_battery_vcc)
        {
            break;
        }
        LOG(PSTR("VCC failed\n"));
        blink_led(Blink_Led::Red, 5, chrono::millis(200));
        sleep(true);
    } while (true);
}

