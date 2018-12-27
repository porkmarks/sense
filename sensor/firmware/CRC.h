#pragma once

#include <stdint.h>
#include <stdlib.h>

uint32_t crc32(const void* data, uint8_t size);
uint16_t crc16(const void* data, uint8_t size);
