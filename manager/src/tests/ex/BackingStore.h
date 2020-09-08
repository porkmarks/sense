#pragma once

#include <cstdint>
#include <vector>

class BackingStore
{
public:
	enum { PAGE_COUNT = 512 };

	static bool store(uint16_t idx, const void* page, uint16_t size);
	static bool load(uint16_t idx, void* page, uint16_t size);
	static void clear();
	static size_t get_used_page_count();

private:
};
