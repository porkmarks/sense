#pragma once

//#include <SPI.h>
#include <stdint.h>
#include "TypeDef.h"

#ifdef RASPBERRY_PI
#   include "spi_rpi.h"
#endif

#define SPI_READ  0b00000000
#define SPI_WRITE 0b10000000

#ifdef ESP32
  // ESP32 boards (pin 10 conflicts with ESP32 flash connections)
  #define LORALIB_DEFAULT_SPI_CS                      4
#else
  // all other architectures
  #define LORALIB_DEFAULT_SPI_CS                      10
#endif

class Module {
  public:
    Module(int cs = LORALIB_DEFAULT_SPI_CS, int int0 = 2, int int1 = 3);
    
    void init(uint8_t interf, uint8_t gpio);
    
    uint8_t SPIgetRegValue(uint8_t reg, uint8_t msb = 7, uint8_t lsb = 0);

    int8_t SPIsetRegValue(uint8_t reg, uint8_t value, uint8_t msb = 7, uint8_t lsb = 0);
    int8_t SPIsetRegValueChecked(uint8_t reg, uint8_t value, uint8_t msb = 7, uint8_t lsb = 0, uint8_t checkInterval = 2);
    
    void SPIreadRegisterBurst(uint8_t reg, uint8_t numBytes, uint8_t* inBytes);
    uint8_t SPIreadRegister(uint8_t reg);
    
    void SPIwriteRegisterBurst(uint8_t reg, uint8_t* data, uint8_t numBytes);
    void SPIwriteRegister(uint8_t reg, uint8_t data);

    int int0;
    int int1;

  private:
    int _cs;
#ifdef RASPBERRY_PI
    SPI _spi;
#endif

};
