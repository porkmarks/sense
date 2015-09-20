#pragma once

#include <stdint.h>
#include <string.h>

template<class T>
void pack(uint8_t* buffer, size_t& off, const T& val)
{
  memcpy(buffer + off, &val, sizeof(val));
  off += sizeof(val);
}

