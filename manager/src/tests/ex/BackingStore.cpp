#include "BackingStore.h"
#include <cassert>

static std::vector<std::vector<uint8_t>> s_pages;

bool BackingStore::store(uint16_t idx, const void* page, uint16_t size)
{
	if (idx >= PAGE_COUNT)
		return false;

	std::vector<uint8_t> p;
	p.resize(size);
	memcpy(p.data(), page, size);
	s_pages.push_back(std::move(p));
	return true;
}

bool BackingStore::load(uint16_t idx, void* page, uint16_t size)
{
	const std::vector<uint8_t>& p = s_pages[idx];
	assert(size == p.size());
	memcpy(page, p.data(), size);
	return true;
}

void BackingStore::clear()
{
	s_pages.clear();
}
size_t BackingStore::get_used_page_count()
{
	return s_pages.size();
}
