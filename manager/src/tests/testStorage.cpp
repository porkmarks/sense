#include "cstdio"
#include "Logger.h"
#include <iostream>
#include "testUtils.h"
#include <set>
#include "ex/Storage.h"
#include "ex/BackingStore.h"

void testStorage()
{
	std::cout << "Testing Storage\n";

	srand(11);

	{
		Storage storage;
		CHECK_TRUE(storage.empty());
		CHECK_EQUALS(storage.get_data_count(), 0);
		CHECK_EQUALS(storage.get_group_count(), 0);
		CHECK_EQUALS(storage.get_first_group_idx(), 0);
		//CHECK_EQUALS(storage.get_last_group_idx(), 0);

		CHECK_TRUE(storage.push_back(Storage::Data{ 0.f, 0.f }));
		CHECK_FALSE(storage.empty());
		//CHECK_EQUALS(storage.get_data_count(), 1);
		//CHECK_EQUALS(storage.get_group_count(), 1);
		//CHECK_EQUALS(storage.get_first_group_idx(), 0);
		//CHECK_EQUALS(storage.get_last_group_idx(), 0);
	}

	for (size_t r = 0; r < 10000; r++)
	{
		std::cout << "\t\tRun " << r << std::endl;

		BackingStore::clear();
		Storage storage;

		std::vector<Storage::Data> datas;
		float scale = 9.9f;
		float temperature = 0.f;
		float humidity = 0.f;
		while (true)
		{
			Storage::Data data{ temperature, humidity };
			bool ok = storage.push_back(data);
			if (!ok)
				break;
			datas.push_back(data);
			temperature += ((float(rand()) / float(RAND_MAX)) - 0.5f) * scale;
			//temperature += ((float(rand()) / float(RAND_MAX)) < 0.5f) ? scale : -scale;
			temperature = std::clamp(temperature, -100.f, 100.f);
			humidity += ((float(rand()) / float(RAND_MAX)) - 0.5f) * scale;
			//humidity += ((float(rand()) / float(RAND_MAX)) < 0.5f) ? scale : -scale;
			humidity = std::clamp(humidity, 0.f, 100.f);
		}
		CHECK_TRUE(BackingStore::get_used_page_count() == BackingStore::PAGE_COUNT);
		CHECK_TRUE(storage.get_group_count() == BackingStore::PAGE_COUNT + 1);

		Storage::iterator it;
		size_t index = 0;
		while (storage.unpack_next(it))
		{
			const Storage::Data& srcData = it.data;
			const Storage::Data& refData = datas[index];

			float dt = std::abs(srcData.temperature - refData.temperature);
			float dh = std::abs(srcData.humidity - refData.humidity);
 			if (dt > 0.0098f * 0.5f)
 				CHECK_FAIL();
 			if (dh >= 0.0196f * 0.5f)
				CHECK_FAIL();
			if (dt > 0.0978f * 0.5f)
				CHECK_FAIL();
			if (dh >= 0.0489f * 0.5f)
				CHECK_FAIL();

			index++;
		}
		CHECK_TRUE(index == storage.get_data_count());
	}

	int a = 0;

}

