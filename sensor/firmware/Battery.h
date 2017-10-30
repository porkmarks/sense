#pragma once

inline float read_vcc()
{
    // Read 1.1V reference against AVcc
    // set the reference to Vcc and the measurement to the internal 1.1V reference
    ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);

    delay(2); // Wait for Vref to settle
    ADCSRA |= _BV(ADSC); // Start conversion
    while (bit_is_set(ADCSRA,ADSC)); // measuring

    uint8_t low  = ADCL; // must read ADCL first - it then locks ADCH
    uint8_t high = ADCH; // unlocks both

    int32_t result = (high<<8) | low;

    float vcc = 1.1f * 1023.f / result;//1125300L / result; // Calculate Vcc (in mV); 1125300 = 1.1*1023*1000
    return vcc;
}
