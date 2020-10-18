#include "CRC.h"

#ifdef __AVR__
#   include <avr/pgmspace.h>
#   include <util/crc16.h>
#else
#endif

uint32_t crc32(const void* data, uint8_t size)
{
    constexpr uint32_t Polynomial = 0xEDB88320;
    uint32_t crc = ~uint32_t(~0L);
    const uint8_t* p = reinterpret_cast<const uint8_t*>(data);
    while (size--)
    {
        crc ^= *p++;

        uint8_t lowestBit = crc & 1;
        crc >>= 1;
        if (lowestBit) crc ^= Polynomial;

        lowestBit = crc & 1;
        crc >>= 1;
        if (lowestBit) crc ^= Polynomial;

        lowestBit = crc & 1;
        crc >>= 1;
        if (lowestBit) crc ^= Polynomial;

        lowestBit = crc & 1;
        crc >>= 1;
        if (lowestBit) crc ^= Polynomial;

        lowestBit = crc & 1;
        crc >>= 1;
        if (lowestBit) crc ^= Polynomial;

        lowestBit = crc & 1;
        crc >>= 1;
        if (lowestBit) crc ^= Polynomial;

        lowestBit = crc & 1;
        crc >>= 1;
        if (lowestBit) crc ^= Polynomial;

        lowestBit = crc & 1;
        crc >>= 1;
        if (lowestBit) crc ^= Polynomial;
    }
    return ~crc;
}

#ifndef __AVR__
uint16_t _crc16_update(uint16_t crc, uint8_t a)
{
    crc ^= a;
    for (uint8_t i = 0; i < 8; ++i)
    {
        if (crc & 1)
        {
            crc = (crc >> 1) ^ 0xA001;
        }
        else
        {
            crc = (crc >> 1);
        }
    }

    return crc;
}
#endif

uint16_t crc16(const void* data, uint8_t size)
{
    const uint8_t* p = reinterpret_cast<const uint8_t*>(data);
    uint16_t crc = uint16_t(~0L);
    while (size-- > 0)
    {
        crc = _crc16_update(crc, *p++);
    }
    crc = ~crc;
    return crc;
}

