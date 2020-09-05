#include "cstdio"
#include "Logger.h"
#include <iostream>
#include "testUtils.h"
#include <set>
#include "ex/Storage.h"


class BackingStore
{
public:
	enum { GROUP_COUNT = 1024 };

	static std::vector<std::vector<uint8_t>> s_pages;

	static bool store(uint16_t idx, const void* page, uint16_t size)
	{
		if (idx >= GROUP_COUNT)
			return false;

		std::vector<uint8_t> p;
		p.reserve(size);
		std::copy((const uint8_t*)page, (const uint8_t*)page + size, std::back_inserter(p));
		s_pages.push_back(std::move(p));
		return true;
	}
	static bool load(uint16_t idx, void* page, uint16_t size)
	{
		const std::vector<uint8_t>& p = s_pages[idx];
		assert(size == p.size());
		std::copy(p.begin(), p.end(), (uint8_t*)page);
		return true;
	}
};

std::vector<std::vector<uint8_t>> BackingStore::s_pages;


void testStorage()
{
	std::cout << "Testing Storage\n";

	srand(11);

	using S = Storage<BackingStore>;
	S storage;

	CHECK_TRUE(storage.empty());
	CHECK_EQUALS(storage.get_data_count(), 0);
	CHECK_EQUALS(storage.get_group_count(), 0);
	CHECK_EQUALS(storage.get_first_group_idx(), 0);
	//CHECK_EQUALS(storage.get_last_group_idx(), 0);

	CHECK_TRUE(storage.push_back(S::Data{0.f, 0.f}));
	CHECK_FALSE(storage.empty());
	//CHECK_EQUALS(storage.get_data_count(), 1);
	//CHECK_EQUALS(storage.get_group_count(), 1);
	//CHECK_EQUALS(storage.get_first_group_idx(), 0);
	//CHECK_EQUALS(storage.get_last_group_idx(), 0);

	std::vector<S::Data> datas;
	float scale = 0.5f;
	float temperature = 0.f;
	float humidity = 0.f;
	while (true)
	{
		S::Data data{ temperature, humidity };
		bool ok = storage.push_back(data);
		if (!ok)
			break;
		datas.push_back(data);
		temperature += ((float(rand()) / float(RAND_MAX)) - 0.5f) * scale;
		humidity += ((float(rand()) / float(RAND_MAX)) - 0.5f) * scale;
	}
	CHECK_TRUE(BackingStore::s_pages.size() == BackingStore::GROUP_COUNT);
	CHECK_TRUE(storage.get_group_count() == BackingStore::GROUP_COUNT + 1);

	S::iterator it;
	size_t index = 0;
	while (storage.unpack_next(it))
	{
		S::Data srcData = it.data;
		S::Data refData = datas[index];

		index++;
	}
	CHECK_TRUE(index == storage.get_data_count());

	int a = 0;

}

