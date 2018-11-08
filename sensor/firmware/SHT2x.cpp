/*
  SHT2x - A Humidity Library for Arduino.

  Supported Sensor modules:
    SHT21-Breakout Module - http://www.moderndevice.com/products/sht21-humidity-sensor
    SHT2x-Breakout Module - http://www.misenso.com/products/001

  Created by Christopher Ladden at Modern Device on December 2009.
  Modified by Paul Badger March 2010

  Modified by www.misenso.com on October 2011:
    - code optimisation
    - compatibility with Arduino 1.0

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <inttypes.h>
#include "SHT2x.h"
#include "SoftI2CMaster.h"

bool _i2c_init() { return i2c_init(); }
//#include "Wire.h"


/******************************************************************************
 * Global Functions
 ******************************************************************************/

/**********************************************************
 * GetHumidity
 *  Gets the current humidity from the sensor.
 *
 * @return float - The relative humidity in %RH
 **********************************************************/
bool SHT2x::GetHumidity(float& value)
{
    uint16_t raw = 0;
    if (!readSensor(eRHumidityHoldCmd, raw))
    {
        return false;
    }
    value = (-6.0 + 125.0 / 65536.0 * (float)(raw));
    return true;
}

/**********************************************************
 * GetTemperature
 *  Gets the current temperature from the sensor.
 *
 * @return float - The temperature in Deg C
 **********************************************************/
bool SHT2x::GetTemperature(float& value)
{
    uint16_t raw = 0;
    if (!readSensor(eTempHoldCmd, raw))
    {
        return false;
    }
    value = (-46.85 + 175.72 / 65536.0 * (float)(raw));
    return true;
}


/******************************************************************************
 * Private Functions
 ******************************************************************************/
constexpr uint16_t POLYNOMIAL = 0x131;  // P(x)=x^8+x^5+x^4+1 = 100110001

bool SHT2x::readSensor(uint8_t command, uint16_t& result)
{
    if (!i2c_start((eSHT2xAddress << 1) | I2C_WRITE))	//begin
    {
        return false;
    }
    if (!i2c_write(command))					//send the pointer location
    {
        i2c_stop();                 //end
        return false;
    }
    if (!i2c_rep_start((eSHT2xAddress << 1) | I2C_READ))  //begin
    {
        i2c_stop();                 //end
        return false;
    }

    //Store the result
    uint8_t data[2];
    data[0] = i2c_read(false);
    data[1] = i2c_read(false);
    result = data[0] << 8;
    result += data[1];
    result &= ~0x0003;   // clear two low bits (status bits)
    uint8_t checksum = i2c_read(true);

    i2c_stop();                 //end

    //calculates 8-Bit checksum with given polynomial
    uint8_t crc = 0;
    for (uint8_t i = 0; i < 2; ++i)
    { 
        crc ^= (data[i]);
        for (uint8_t bit = 8; bit > 0; --bit)
        { 
            crc = (crc & 0x80) ? (crc << 1) ^ POLYNOMIAL : (crc << 1);
        }
    }
    return crc == checksum;
}


