#pragma once

#include "Arduino.h"


// Uncomment to enable printing out nice debug messages.
//#define DHT_DEBUG

// Define where debug output will be printed.
#define DEBUG_PRINTER Serial

// Setup debug printing macros.
#ifdef DHT_DEBUG
#define DEBUG_PRINT(...) DEBUG_PRINTER.print(__VA_ARGS__);
#define DEBUG_PRINTLN(...) DEBUG_PRINTER.println(__VA_ARGS__);
#else
#define DEBUG_PRINT(...)
#define DEBUG_PRINTLN(...)
#endif

// Define types of sensors.
#define DHT11 11
#define DHT22 22
#define DHT21 21
#define AM2301 21

class DHT
{
public:
    DHT(uint8_t pin, uint8_t type);
    void begin(void);
    bool read(float& t, float& h);

private:
    bool read_sensor(void);

    uint8_t m_data[6];
    uint8_t m_pin = 0, m_type = 0, m_bit = 0, m_port = 0;
    uint32_t m_lastreadtime = 0;
    uint16_t m_maxcycles = 0;

    uint16_t expect_pulse(uint8_t bit);
};

class Interrupt_Lock
{
public:
    Interrupt_Lock()
    {
        noInterrupts();
    }
    ~Interrupt_Lock()
    {
        interrupts();
    }
};
