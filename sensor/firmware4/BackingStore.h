#pragma once

#include <stdint.h>
#include <string.h>

#ifdef __AVR__
#else
# include <vector>
#endif


class BackingStore
{
public:
	enum { PAGE_COUNT = 512 };
	enum { MAX_PAGE_SIZE = 124 }; //leave space for the 4 byte CRC

	static bool store(uint16_t idx, const void* page, uint16_t size);
	static bool load(uint16_t idx, void* page, uint16_t size);
	static void clear();
	static size_t get_used_page_count();

private:
};
