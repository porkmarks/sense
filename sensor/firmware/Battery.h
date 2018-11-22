#pragma once

void init_adc()
{
#if defined(ADCSRA)
  // set a2d prescaler so we are inside the desired 50-200 KHz range.
  #if F_CPU >= 16000000 // 16 MHz / 128 = 125 KHz
    sbi(ADCSRA, ADPS2);
    sbi(ADCSRA, ADPS1);
    sbi(ADCSRA, ADPS0);
  #elif F_CPU >= 8000000 // 8 MHz / 64 = 125 KHz
    sbi(ADCSRA, ADPS2);
    sbi(ADCSRA, ADPS1);
    cbi(ADCSRA, ADPS0);
  #elif F_CPU >= 4000000 // 4 MHz / 32 = 125 KHz
    sbi(ADCSRA, ADPS2);
    cbi(ADCSRA, ADPS1);
    sbi(ADCSRA, ADPS0);
  #elif F_CPU >= 2000000 // 2 MHz / 16 = 125 KHz
    sbi(ADCSRA, ADPS2);
    cbi(ADCSRA, ADPS1);
    cbi(ADCSRA, ADPS0);
  #elif F_CPU >= 1000000 // 1 MHz / 8 = 125 KHz
    cbi(ADCSRA, ADPS2);
    sbi(ADCSRA, ADPS1);
    sbi(ADCSRA, ADPS0);
  #else // 128 kHz / 2 = 64 KHz -> This is the closest you can get, the prescaler is 2
    cbi(ADCSRA, ADPS2);
    cbi(ADCSRA, ADPS1);
    sbi(ADCSRA, ADPS0);
  #endif
  // enable a2d conversions
  sbi(ADCSRA, ADEN);
#endif  
}

float read_vcc()
{
    // Read 1.1V reference against AVcc
    // set the reference to Vcc and the measurement to the internal 1.1V reference
    ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);

    chrono::delay(chrono::millis(2)); // Wait for Vref to settle
    ADCSRA |= _BV(ADSC); // Start conversion
    while (bit_is_set(ADCSRA,ADSC)); // measuring

    uint8_t low  = ADCL; // must read ADCL first - it then locks ADCH
    uint8_t high = ADCH; // unlocks both

    int32_t result = (high<<8) | low;

    float vcc = 1.1f * 1023.f / result;//1125300L / result; // Calculate Vcc (in mV); 1125300 = 1.1*1023*1000
    return vcc;
}
