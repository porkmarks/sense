#pragma once

#include <stdint.h>
#include <stdlib.h>

uint32_t crc32(const uint8_t* data, size_t size);
uint16_t crc16(const uint8_t* data, size_t size);
