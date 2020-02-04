/*
  SHT2x - A Humidity Library for Arduino.

  Supported Sensor modules:
    SHT21-Breakout Module - http://www.moderndevice.com/products/sht21-humidity-sensor
    SHT2x-Breakout Module - http://www.misenso.com/products/001

  Created by Christopher Ladden at Modern Device on December 2009.

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


#ifndef SHT2X_H
#define SHT2X_H

#include <inttypes.h>

class SHT2x
{
public:
  enum MeasurementCmd : uint8_t
  {
      TempHoldCmd    = 0xE3,
      RHumidityHoldCmd = 0xE5,
      TempNoHoldCmd      = 0xF3,
      RHumidityNoHoldCmd = 0xF5,
  };
  
  enum Resolution : uint8_t
  {
    RH12_TEMP14 = 0x00,               //resolution, temperature: 14-Bit & humidity: 12-Bit
    RH8_TEMP12  = 0x01,               //resolution, temperature: 12-Bit & humidity: 8-Bit
    RH10_TEMP13 = 0x80,               //resolution, temperature: 13-Bit & humidity: 10-Bit
    RH11_TEMP11 = 0x81                //resolution, temperature: 11-Bit & humidity: 11-Bit
  };

  SHT2x();

  void setResolution(Resolution resolution);

  bool getHumidity(float& value);
  bool getTemperature(float& value);
  bool getMeasurements(float& temperature, float& humidity);

private:
  bool readSensor(uint8_t command, uint16_t& result);
};

#endif
