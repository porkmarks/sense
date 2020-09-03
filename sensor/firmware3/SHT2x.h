#pragma once

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
