#include "Module.h"
#include "Arduino_Compat.h"
#include "Chrono.h"
#include "Log.h"

#ifdef __AVR__
#   include "SPI2.h"
#else
#   include "spi_rpi.h"
#   include "pigpio.h"
#   define min(a,b) ((a)<(b)?(a):(b))
#   define max(a,b) ((a)>(b)?(a):(b))
#endif

Module::Module(int cs, int int0, int int1)
    : _cs(cs)
    , int0(int0)
    , int1(int1)
{
}


void Module::init(uint8_t interface, uint8_t gpio)
{
    //DEBUG_PRINTLN("");

    switch(interface)
    {
    case USE_SPI:
#ifdef __AVR__
        pinModeFast(_cs, OUTPUT);
        digitalWriteFast(_cs, HIGH);
        SPI2.begin();
#else
#endif
        break;
    case USE_UART:
        break;
    case USE_I2C:
        break;
    }

    switch(gpio)
    {
    case INT_NONE:
        break;
    case INT_0:
        pinModeFast(int0, INPUT);
        break;
    case INT_1:
        pinModeFast(int1, INPUT);
        break;
    case INT_BOTH:
        pinModeFast(int0, INPUT);
        pinModeFast(int1, INPUT);
        break;
    }
}


uint8_t Module::SPIgetRegValue(uint8_t reg, uint8_t msb, uint8_t lsb)
{
    uint8_t rawValue = SPIreadRegister(reg);
    uint8_t maskedValue = rawValue & ((0b11111111 << lsb) & (0b11111111 >> (7 - msb)));
    return(maskedValue);
}

int8_t Module::SPIsetRegValue(uint8_t reg, uint8_t value, uint8_t msb, uint8_t lsb)
{
    return SPIsetRegValueChecked(reg, value, msb, lsb, 0);
}

int8_t Module::SPIsetRegValueChecked(uint8_t reg, uint8_t value, uint8_t msb, uint8_t lsb, uint8_t checkInterval)
{
    uint8_t currentValue;
    uint8_t newValue;
    if (msb == 7 && lsb == 0)
    {
        newValue = value;
    }
    else
    {
        currentValue = SPIreadRegister(reg);
        uint8_t mask = ~((0b11111111 << (msb + 1)) | (0b11111111 >> (8 - lsb)));
        newValue = (currentValue & ~mask) | (value & mask);
    }
    SPIwriteRegister(reg, newValue);

    // check register value each millisecond until check interval is reached
    // some registers need a bit of time to process the change (e.g. SX127X_REG_OP_MODE)
    if (checkInterval > 0)
    {
        chrono::millis timeout = chrono::millis(100);
        auto start = chrono::now();
        while (chrono::now() - start <= timeout)
        {
            currentValue = SPIreadRegister(reg);
            if (currentValue == newValue)
            {
                // check passed, we can stop the loop
                return(ERR_NONE);
            }
        }
    }
    else
    {
        return(ERR_NONE);
    }

#ifdef KITELIB_DEBUG
    // check failed, print debug info
    LOG(PSTR("addr:%x, bits%d-%d, crt:%x, new:%x\n"), int(reg), int(msb), int(lsb), int(currentValue), int(newValue));
#endif

    return(ERR_SPI_WRITE_FAILED);
}


void Module::SPIreadRegisterBurst(uint8_t reg, uint8_t numBytes, uint8_t* inBytes)
{
#ifdef __AVR__
    SPI2.beginTransaction(SPI2Settings(1000000, MSBFIRST, SPI_MODE0));
    digitalWriteFast(_cs, LOW);
    SPI2.transfer(reg | SPI_READ);
    SPI2.read(inBytes, numBytes);
    digitalWriteFast(_cs, HIGH);
    SPI2.endTransaction();
#else
    uint8_t buffer[1024];
    memset(buffer, 0, numBytes + 1);
    buffer[0] = reg | SPI_READ;
    _spi.transfer(buffer, buffer, numBytes + 1);
    memcpy(inBytes, buffer + 1, numBytes);
#endif
}


uint8_t Module::SPIreadRegister(uint8_t reg)
{
#ifdef __AVR__
    uint8_t inByte;
    SPI2.beginTransaction(SPI2Settings(1000000, MSBFIRST, SPI_MODE0));
    digitalWriteFast(_cs, LOW);
    SPI2.transfer(reg | SPI_READ);
    inByte = SPI2.transfer(0x00);
    digitalWriteFast(_cs, HIGH);
    SPI2.endTransaction();
    return(inByte);
#else
    uint8_t buffer[2] = { (uint8_t)(reg | SPI_READ), 0 };
    _spi.transfer(buffer, buffer, 2);
    return buffer[1];
#endif
}


void Module::SPIwriteRegisterBurst(uint8_t reg, uint8_t* data, uint8_t numBytes)
{
#ifdef __AVR__
    SPI2.beginTransaction(SPI2Settings(1000000, MSBFIRST, SPI_MODE0));
    digitalWriteFast(_cs, LOW);
    SPI2.transfer(reg | SPI_WRITE);
    SPI2.write(data, numBytes);
    digitalWriteFast(_cs, HIGH);
    SPI2.endTransaction();
#else
    uint8_t buffer[1024];
    buffer[0] = (uint8_t)(reg | SPI_WRITE);
    memcpy(buffer + 1, data, numBytes);
    _spi.transfer(buffer, nullptr, numBytes + 1);
#endif
}


void Module::SPIwriteRegister(uint8_t reg, uint8_t data)
{
#ifdef __AVR__
    SPI2.beginTransaction(SPI2Settings(1000000, MSBFIRST, SPI_MODE0));
    digitalWriteFast(_cs, LOW);
    SPI2.transfer(reg | SPI_WRITE);
    SPI2.transfer(data);
    digitalWriteFast(_cs, HIGH);
    SPI2.endTransaction();
#else
    uint8_t buffer[2] = { (uint8_t)(reg | SPI_WRITE), data };
    _spi.transfer(buffer, nullptr, 2);
#endif
}
