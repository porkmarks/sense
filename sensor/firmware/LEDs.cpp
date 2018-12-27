#include "LEDs.h"
#include "Sleep.h"
#include "Arduino_Compat.h"


constexpr uint8_t RED_LED_PIN  = 14;
constexpr uint8_t GREEN_LED_PIN = 15;


static constexpr uint8_t k_red_led_time_table[2] = {200, 20};

uint8_t s_red_led_value = LOW;
ISR (TIMER0_COMPA_vect)
{
    digitalWriteFast(RED_LED_PIN, s_red_led_value);
    OCR0A = k_red_led_time_table[s_red_led_value];
    s_red_led_value = !s_red_led_value;
}

void init_leds()
{
    pinModeFast(RED_LED_PIN, OUTPUT);
    digitalWriteFast(RED_LED_PIN, LOW);
    pinModeFast(GREEN_LED_PIN, OUTPUT);
    digitalWriteFast(GREEN_LED_PIN, LOW);

    // Set the Timer Mode to CTC
    TCCR0A |= bit(WGM01);
    TIMSK0 |= bit(OCIE0A);    //Set the ISR COMPA vect
    //TCCR0B |= bit(CS01) | bit(CS00); // set prescaler to 64 and start the timer
}

void set_led(Led led)
{
    if (led == Led::Green)
    {
      TCCR0B &= ~(bit(CS01) | bit(CS00));
      digitalWriteFast(GREEN_LED_PIN, HIGH);
      digitalWriteFast(RED_LED_PIN, LOW);
    }
    else if (led == Led::Red)
    {
      TCCR0B &= ~(bit(CS01) | bit(CS00));
      digitalWriteFast(GREEN_LED_PIN, LOW);
      digitalWriteFast(RED_LED_PIN, HIGH);
    }
    else if (led == Led::Yellow)
    {
      TCNT0 = 0;
      OCR0A = k_red_led_time_table[0];
      TCCR0B |= (bit(CS01) | bit(CS00));
      digitalWriteFast(RED_LED_PIN, LOW);
      digitalWriteFast(GREEN_LED_PIN, HIGH);
    }
    else
    {
      TCCR0B &= ~(bit(CS01) | bit(CS00));
      digitalWriteFast(GREEN_LED_PIN, LOW);
      digitalWriteFast(RED_LED_PIN, LOW);
    }
}

void blink_led(Blink_Led blink_led, uint8_t times, chrono::millis period)
{
    Led led = Led(blink_led);
    chrono::millis hp(period.count >> 1);
    for (uint8_t i = 0; i < times; i++)
    {
        set_led(led);
        chrono::delay(hp);
        set_led(Led::None);
        chrono::delay(hp);
    }
    set_led(Led::None);
}
