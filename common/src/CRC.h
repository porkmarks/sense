#pragma once

#include <stdint.h>
#include <stdlib.h>

uint32_t crc_update(uint32_t crc, uint8_t data);
uint32_t crc32(const uint8_t* data, size_t size);
