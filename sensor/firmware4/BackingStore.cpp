#include "BackingStore.h"
#include <assert.h>

#ifdef __AVR__
#else
static std::vector<std::vector<uint8_t>> s_pages;
#endif

bool BackingStore::store(uint16_t idx, const void* page, uint16_t size)
{
	if (idx >= PAGE_COUNT)
		return false;

#ifdef __AVR__
#else
	std::vector<uint8_t> p;
	p.resize(size);
	memcpy(p.data(), page, size);
	s_pages.push_back(std::move(p));
#endif
	return true;
}

bool BackingStore::load(uint16_t idx, void* page, uint16_t size)
{
#ifdef __AVR__
#else
	const std::vector<uint8_t>& p = s_pages[idx];
	assert(size == p.size());
	memcpy(page, p.data(), size);
#endif
	return true;
}

void BackingStore::clear()
{
#ifdef __AVR__
#else
	s_pages.clear();
#endif
}
size_t BackingStore::get_used_page_count()
{
#ifdef __AVR__
#else
	return s_pages.size();
#endif
}
