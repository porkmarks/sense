#pragma once

#include <stdint.h>

// constants for reading & writing
#define I2C_READ    1
#define I2C_WRITE   0

bool i2c_init();
void i2c_shutdown();
bool i2c_start(uint8_t address);
bool i2c_rep_start(uint8_t address);
bool i2c_write(uint8_t data);
uint8_t i2c_read(bool last);
void i2c_stop();
bool i2c_read_registers(uint8_t address7, uint8_t reg, uint8_t* values, uint8_t count);
bool i2c_read_register(uint8_t address7, uint8_t reg, uint8_t& value);
bool i2c_read_register16(uint8_t address7, uint8_t reg, uint16_t& value);

bool i2c_write_registers(uint8_t address7, uint8_t reg, const uint8_t* values, uint8_t count);
bool i2c_write_register(uint8_t address7, uint8_t reg, uint8_t value);
bool i2c_write_register16(uint8_t address7, uint8_t reg, uint16_t value);
