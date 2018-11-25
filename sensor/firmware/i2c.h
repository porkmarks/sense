#pragma once

#include <stdint.h>

// constants for reading & writing
#define I2C_READ    1
#define I2C_WRITE   0

// Init function. Needs to be called once in the beginning.
// Returns false if SDA or SCL are low, which probably means
// a I2C bus lockup or that the lines are not pulled up.
bool __attribute__ ((noinline)) i2c_init(void) __attribute__ ((used));

bool __attribute__ ((noinline)) i2c_recover(void) __attribute__ ((used));

// Start transfer function: <addr> is the 8-bit I2C address (including the R/W
// bit).
// Return: true if the slave replies with an "acknowledge", false otherwise
bool __attribute__ ((noinline)) i2c_start(uint8_t addr) __attribute__ ((used));

// Similar to start function, but wait for an ACK! Will timeout if I2C_MAXWAIT > 0.
bool  __attribute__ ((noinline)) i2c_start_wait(uint8_t addr) __attribute__ ((used));

// Repeated start function: After having claimed the bus with a start condition,
// you can address another or the same chip again without an intervening
// stop condition.
// Return: true if the slave replies with an "acknowledge", false otherwise
bool __attribute__ ((noinline)) i2c_rep_start(uint8_t addr) __attribute__ ((used));

// Issue a stop condition, freeing the bus.
void __attribute__ ((noinline)) i2c_stop(void) asm("ass_i2c_stop") __attribute__ ((used));

// Write one byte to the slave chip that had been addressed
// by the previous start call. <value> is the byte to be sent.
// Return: true if the slave replies with an "acknowledge", false otherwise
bool __attribute__ ((noinline)) i2c_write(uint8_t value) asm("ass_i2c_write") __attribute__ ((used));


// Read one byte. If <last> is true, we send a NAK after having received
// the byte in order to terminate the read sequence.
uint8_t __attribute__ ((noinline)) i2c_read(bool last) __attribute__ ((used));
