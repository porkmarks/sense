/* DHT library

MIT license
written by Adafruit Industries
*/

#include "DHT.h"
#include <avr/eeprom.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/wdt.h>


DHT::DHT(uint8_t pin, uint8_t type)
{
    m_pin = pin;
    m_type = type;
    m_bit = digitalPinToBitMask(pin);
    m_port = digitalPinToPort(pin);
    m_maxcycles = microsecondsToClockCycles(1000);  // 1 millisecond timeout for
    m_lastreadtime = millis();
    // reading pulses from DHT sensor.
    // Note that count is now ignored as the DHT reading algorithm adjusts itself
    // basd on the speed of the processor.
}

void DHT::begin()
{
    // set up the pins!
    pinMode(m_pin, INPUT);
    digitalWrite(m_pin, HIGH);
    m_lastreadtime = 0;
    DEBUG_PRINT("Max clock cycles: "); DEBUG_PRINTLN(m_maxcycles, DEC);
}

//boolean S == Scale.  True == Fahrenheit; False == Celcius
bool DHT::read(float& t, float& h) 
{
    if (!read_sensor())
    {
        return false;
    }

    switch (m_type)
    {
    case DHT11:
        h = m_data[0];
        t = m_data[2];
        break;
    case DHT22:
    case DHT21:
        h = m_data[0];
        h *= 256;
        h += m_data[1];
        h /= 10;

        t = m_data[2] & 0x7F;
        t *= 256;
        t += m_data[3];
        t /= 10;
        if (m_data[2] & 0x80)
        {
            t *= -1;
        }
        break;
    }
    return true;
}

bool DHT::read_sensor(void) 
{
    // Check if sensor was read less than two seconds ago and return early
    // to use last reading.
    uint32_t currenttime = millis();
    if (currenttime < m_lastreadtime)
    {
        // ie there was a rollover
        m_lastreadtime = 0;
    }
    if ((currenttime - m_lastreadtime) < 2000)
    {
        return false;
    }
    m_lastreadtime = millis();

    // Reset 40 bits of received data to zero.
    m_data[0] = m_data[1] = m_data[2] = m_data[3] = m_data[4] = 0;

    // Send start signal.  See DHT datasheet for full signal diagram:
    //   http://www.adafruit.com/datasheets/Digital%20humidity%20and%20temperature%20sensor%20AM2302.pdf

    // Go into high impedence state to let pull-up raise data line level and
    // start the reading process.
    digitalWrite(m_pin, HIGH);
    delay(250);

    // First set data line low for 20 milliseconds.
    pinMode(m_pin, OUTPUT);
    digitalWrite(m_pin, LOW);
    delay(20);

    {
        // Turn off interrupts temporarily because the next sections are timing critical
        // and we don't want any interruptions.
        Interrupt_Lock lock;

        // End the start signal by setting data line high for 40 microseconds.
        digitalWrite(m_pin, HIGH);
        delayMicroseconds(40);

        // Now start reading the data line to get the value from the DHT sensor.
        pinMode(m_pin, INPUT);
        delayMicroseconds(10);  // Delay a bit to let sensor pull data line low.

        // First expect a low signal for ~80 microseconds followed by a high signal
        // for ~80 microseconds again.
        if (expect_pulse(0) == 0)
        {
            DEBUG_PRINTLN(F("Timeout waiting for start signal low pulse."));
            return false;
        }
        if (expect_pulse(m_bit) == 0)
        {
            DEBUG_PRINTLN(F("Timeout waiting for start signal high pulse."));
            return false;
        }

        // Now read the 40 bits sent by the sensor.  Each bit is sent as a 50
        // microsecond low pulse followed by a variable length high pulse.  If the
        // high pulse is ~28 microseconds then it's a 0 and if it's ~70 microseconds
        // then it's a 1.  We measure the cycle count of the initial 50us low pulse
        // and use that to compare to the cycle count of the high pulse to determine
        // if the bit is a 0 (high state cycle count < low state cycle count), or a
        // 1 (high state cycle count > low state cycle count). Note that for speed all
        // the pulses are read into a array and then examined in a later step.

        uint8_t bits[8] = { 128, 64, 32, 16, 8, 4, 2, 1 };

        for (uint8_t byte = 0; byte < 5; byte++)
        {
            for (uint8_t bit = 0; bit < 8; bit++)
            {
                uint16_t lowCycles  = expect_pulse(0);
                uint16_t highCycles = expect_pulse(m_bit);

                if (lowCycles == 0 || highCycles == 0)
                {
                    DEBUG_PRINTLN(F("Timeout waiting for pulse."));
                    return false;
                }

                //m_data[byte] <<= 1;
                // Now compare the low and high cycle times to see if the bit is a 0 or 1.
                if (highCycles > lowCycles)
                {
                    // High cycles are greater than 50us low cycle count, must be a 1.
                    m_data[byte] |= bits[bit];
                }

                // Else high cycles are less than (or equal to, a weird case) the 50us low
                // cycle count so this must be a zero.  Nothing needs to be changed in the
                // stored data.

            }
        }
    } // Timing critical code is now complete.

    DEBUG_PRINTLN(F("Received:"));
    DEBUG_PRINT(m_data[0], HEX); DEBUG_PRINT(F(", "));
    DEBUG_PRINT(m_data[1], HEX); DEBUG_PRINT(F(", "));
    DEBUG_PRINT(m_data[2], HEX); DEBUG_PRINT(F(", "));
    DEBUG_PRINT(m_data[3], HEX); DEBUG_PRINT(F(", "));
    DEBUG_PRINT(m_data[4], HEX); DEBUG_PRINT(F(" =? "));
    DEBUG_PRINTLN((m_data[0] + m_data[1] + m_data[2] + m_data[3]) & 0xFF, HEX);

    // Check we read 40 bits and that the checksum matches.
    if (m_data[4] == ((m_data[0] + m_data[1] + m_data[2] + m_data[3]) & 0xFF))
    {
        return true;
    }
    else
    {
        DEBUG_PRINTLN(F("Checksum failure!"));
        return false;
    }
}

// Expect the signal line to be at the specified level for a period of time and
// return a count of loop cycles spent at that level (this cycle count can be
// used to compare the relative time of two pulses).  If more than a millisecond
// ellapses without the level changing then the call fails with a 0 response.
// This is adapted from Arduino's pulseInLong function (which is only available
// in the very latest IDE versions):
//   https://github.com/arduino/Arduino/blob/master/hardware/arduino/avr/cores/arduino/wiring_pulse.c
uint16_t DHT::expect_pulse(uint8_t bit)
{
    uint16_t count = 0;
    while ((*portInputRegister(m_port) & m_bit) == bit)
    {
        if (count++ >= m_maxcycles)
        {
            return 0; // Exceeded timeout, fail.
        }
    }

    return count;
}
