#include "BackingStore.h"
#include <assert.h>
#include <execution> 

#ifdef __AVR__
#else
# include <vector>
# include <array>
static std::array<std::vector<uint8_t>, BackingStore::PAGE_COUNT> s_pages;
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
	s_pages[idx] = std::move(p);
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
	for (auto& p: s_pages)
		p.clear();
#endif
}
size_t BackingStore::get_used_page_count()
{
#ifdef __AVR__
#else
	return std::count_if(s_pages.begin(), s_pages.end(), [](const auto& page) { return !page.empty(); });
#endif
}
