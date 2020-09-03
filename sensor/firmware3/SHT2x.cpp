#include <inttypes.h>
#include <stdio.h>
#include <avr/pgmspace.h>

#include "SHT2x.h"
#include "i2c.h"
#include "Chrono.h"

#define Address 0x40

#define HTU21D_USER_REGISTER_WRITE   0xE6      //write user register
#define HTU21D_USER_REGISTER_READ    0xE7      //read  user register

#define HTU21D_TEMP_COEFFICIENT      -0.15f     

/******************************************************************************
 * Global Functions
 ******************************************************************************/

SHT2x::SHT2x()
{
}

void SHT2x::setResolution(Resolution resolution)
{
  uint8_t userRegisterData;
  if (i2c_read_register(Address, HTU21D_USER_REGISTER_READ, userRegisterData))
  {
      //printf_P(PSTR("\nusr %d\n"), (int)userRegisterData);
      
      userRegisterData &= 0x7E;                             //clears current resolution bits with 0
      userRegisterData |= resolution;                 //adds new resolution bits to user register byte

      i2c_write_register(Address, HTU21D_USER_REGISTER_WRITE, userRegisterData);
  }
}

bool SHT2x::getMeasurements(float& temperature, float& humidity)
{
    if (!getTemperature(temperature) || !getHumidity(humidity))
    {
        return false;
    }
    humidity = humidity + (25.0f - temperature) * HTU21D_TEMP_COEFFICIENT;    
    return true;
}

/**********************************************************
 * GetHumidity
 *  Gets the current humidity from the sensor.
 *
 * @return float - The relative humidity in %RH
 **********************************************************/
bool SHT2x::getHumidity(float& value)
{
    uint16_t raw = 0;
    if (!readSensor(RHumidityHoldCmd, raw))
    {
        return false;
    }
    value = (-6.0 + 125.0 * (float)(raw) / 65536.0);
    return true;
}

/**********************************************************
 * GetTemperature
 *  Gets the current temperature from the sensor.
 *
 * @return float - The temperature in Deg C
 **********************************************************/
bool SHT2x::getTemperature(float& value)
{
    uint16_t raw = 0;
    if (!readSensor(TempHoldCmd, raw))
    {
        return false;
    }
    value = (-46.85 + 175.72 * (float)(raw) / 65536.0);
    return true;
}


/******************************************************************************
 * Private Functions
 ******************************************************************************/
#define POLYNOMIAL 0x13100ULL   // P(x)=x^8+x^5+x^4+1 = 100110001

bool SHT2x::readSensor(uint8_t command, uint16_t& result)
{
    if (!i2c_start((Address << 1) | I2C_WRITE))	//begin
    {
        return false;
    }
    if (!i2c_write(command))					//send the pointer location
    {
        i2c_stop();                 //end
        return false;
    }
    if (!i2c_rep_start((Address << 1) | I2C_READ))  //begin
    {
        i2c_stop();                 //end
        return false;
    }

    chrono::delay(chrono::millis(30));

    //Store the result
    uint16_t raw = i2c_read(false) << 8;
    raw |= i2c_read(false);
    uint8_t checksum = i2c_read(true);

    i2c_stop();                 //end

    result = raw & (~0x3); // clear two low bits (status bits)
    
    //calculates 8-Bit checksum with given polynomial
    for (uint8_t bit = 0; bit < 16; bit++)
    {
        if  (raw & 0x8000) raw = (raw << 1) ^ POLYNOMIAL;
        else raw <<= 1;
    }
    uint8_t crc = raw >> 8;
    return crc == checksum;
}
