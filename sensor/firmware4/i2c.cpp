#include "i2c.h"
#include <avr/io.h>
#include <util/twi.h>
#include "digitalWriteFast.h"
#include "Arduino_Compat.h"


#define F_SCL 100000UL // SCL frequency
#define Prescaler 1
#define TWBR_val ((((F_CPU / F_SCL) / Prescaler) - 16 ) / 2)

bool i2c_init(void)
{
  TWBR0 = (uint8_t)TWBR_val;

  pinModeFast(SDA, INPUT);
  pinModeFast(SCL, INPUT);
  return (digitalReadFast(SDA) != 0 && digitalReadFast(SCL) != 0);
}

void i2c_shutdown(void)
{
  // turn off I2C
  TWCR0 &= ~((1 << TWEN) | (1 << TWIE) | (1 << TWEA));  
}

bool i2c_start(uint8_t address)
{
  // transmit START condition 
  TWCR0 = (1<<TWINT) | (1<<TWSTA) | (1<<TWEN);
  // wait for end of transmission
  while( !(TWCR0 & (1<<TWINT)) );

  // check if the start condition was successfully transmitted
  uint8_t twst = TWSR0 & 0xF8;
  if ( (twst != TW_START) && (twst != TW_REP_START)) return false;
  
  // load slave address into data register
  TWDR0 = address;
  // start transmission of address
  TWCR0 = (1<<TWINT) | (1<<TWEN);
  // wait for end of transmission
  while( !(TWCR0 & (1<<TWINT)) );
  
  // check if the device has acknowledged the READ / WRITE mode
  twst = TWSR0 & 0xF8;
  if ( (twst != TW_MT_SLA_ACK) && (twst != TW_MR_SLA_ACK) ) return false;
  
  return true;
}

bool i2c_rep_start(uint8_t address)
{
  return i2c_start(address);
}

bool i2c_write(uint8_t data)
{
  // load data into data register
  TWDR0 = data;
  // start transmission of data
  TWCR0 = (1<<TWINT) | (1<<TWEN);
  // wait for end of transmission
  while( !(TWCR0 & (1<<TWINT)) );
  
  if( (TWSR0 & 0xF8) != TW_MT_DATA_ACK ){ return false; }
  
  return true;
}

uint8_t i2c_read(bool last)
{
  // start TWI module and acknowledge data after reception
  TWCR0 = (1<<TWINT) | (1<<TWEN) | (last ? 0 : (1<<TWEA));
  // wait for end of transmission
  while( !(TWCR0 & (1<<TWINT)) );
  // return received data from TWDR0
  return TWDR0;
}

void i2c_stop(void)
{
  // transmit STOP condition
  TWCR0 = (1<<TWINT) | (1<<TWEN) | (1<<TWSTO);
// wait until stop condition is executed and bus released
  while(TWCR & (1<<TWSTO));  
}

bool i2c_read_registers(uint8_t address7, uint8_t reg, uint8_t* values, uint8_t count)
{
  if (i2c_start((address7 << 1) | I2C_WRITE) && 
      i2c_write(reg) && 
      i2c_rep_start((address7 << 1) | I2C_READ))
  {
      for (uint8_t i = 0; i < count; i++)
      {
        values[i] = i2c_read(i + 1 >= count);
      }
      i2c_stop();
      return true;
  }
  i2c_stop();
  return false;
}

bool i2c_read_register(uint8_t address7, uint8_t reg, uint8_t& value)
{
  return i2c_read_registers(address7, reg, &value, 1);
}

bool i2c_write_register(uint8_t address7, uint8_t reg, uint8_t value)
{
  bool res = i2c_start((address7 << 1) | I2C_WRITE) && 
             i2c_write(reg) && 
             i2c_write(value);
  i2c_stop();
  return res;
}

bool i2c_read_register16(uint8_t address7, uint8_t reg, uint16_t& value)
{
  uint8_t data[2];
  if (!i2c_read_registers(address7, reg, data, 2))
  {
    return false;
  }
  value   = data[0];
  value  |= data[1] << 8;
  return true;
}

bool i2c_write_register16(uint8_t address7, uint8_t reg, uint16_t value)
{
  bool res = i2c_start((address7 << 1) | I2C_WRITE) && 
             i2c_write(reg) && 
             i2c_write(value & 0xFF) && 
             i2c_write(value >> 8);
  i2c_stop();
  return res;
}


bool i2c_write_registers(uint8_t address7, uint8_t reg, const uint8_t* values, uint8_t count)
{
  bool res = false;
  if (i2c_start((address7 << 1) | I2C_WRITE) && i2c_write(reg))
  {
      res = true;
      for (uint8_t i = 0; i < count; i++)
      {
        res &= i2c_write(values[i]);
      }
  }
  i2c_stop();
  return res;
}
