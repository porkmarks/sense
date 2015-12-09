#include "Storage.h"
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <limits>
#include <iostream>
#include <algorithm>

#define CHECK(x) \
do \
{\
    if (!(x))\
    {\
        std::cout << "Check '" << #x << "' failed at: " << __FILE__ << ": " << __LINE__<< ".\n";\
        exit(1);\
    }\
} while(false)


static bool check_equal(const Storage& storage, const std::vector<Storage::Data>& ref_datas)
{
    size_t data_idx = 0;
    Storage::iterator it;

    constexpr float TEMPERATURE_ACCURACY = 0.25f;
    constexpr float HUMIDITY_ACCURACY = 0.5f;
    constexpr float VCC_ACCURACY = 0.1f;

    while (storage.unpack_next(it))
    {
        const Storage::Data& data = it.data;
        const Storage::Data& ref_data = ref_datas[data_idx];

        float temperature_delta = std::abs(data.temperature - ref_data.temperature);
        if (temperature_delta > TEMPERATURE_ACCURACY)
        {
            //std::cout << data_idx << ": Temperature delta too big: " << temperature_delta << ".\n";
            return false;
        }
        float humidity_delta = std::abs(data.humidity - ref_data.humidity);
        if (humidity_delta > HUMIDITY_ACCURACY)
        {
            //std::cout << data_idx << ": Humidity delta too big: " << humidity_delta << ".\n";
            return false;
        }
        float vcc_delta = std::abs(data.vcc - ref_data.vcc);
        if (it.offset == 0 && vcc_delta > VCC_ACCURACY)
        {
            //std::cout << data_idx << ": VCC delta too big: " << vcc_delta << ".\n";
            return false;
        }

        data_idx++;
    }
    if (data_idx != ref_datas.size())
    {
        //std::cout << "Ref data size mismatch\n";
        return false;
    }

    return true;
}

static bool is_pristine(const Storage& storage)
{
    return storage.get_group_count() == 0 &&
            storage.get_first_group_idx() == 0;
}

static void pop_all(Storage& storage, std::vector<Storage::Data>& ref_datas)
{
    size_t count = storage.get_data_count();

    //pop_front
    while (count > 0)
    {
        storage.pop_front();
        ref_datas.erase(ref_datas.begin(), ref_datas.begin() + 1);
        CHECK(check_equal(storage, ref_datas));

        size_t new_count = storage.get_data_count();
        CHECK(new_count + 1 == count);
        count = new_count;
    }
}


static void test_storage_operations()
{
    Storage storage;
    CHECK(is_pristine(storage));
    //CHECK(storage.pop_first_group() == false);

    storage.clear();
    CHECK(is_pristine(storage));
    //CHECK(storage.pop_first_group() == false);


    std::vector<Storage::Data> ref_datas;
    CHECK(check_equal(storage, ref_datas));

    Storage::Data data;
    data.temperature = 50.f;
    data.humidity = 50.f;
    data.vcc = 4.2f;

    //add one group
    storage.push_back(data);
    ref_datas.push_back(data);
    CHECK(check_equal(storage, ref_datas));
    CHECK(storage.get_group_count() == 1);
    CHECK(!is_pristine(storage));
    CHECK(storage.get_last_group_idx() == 0);
    CHECK(storage.get_first_group_idx() == 0);

    //add another group
    while (storage.get_group_count() < 2)
    {
        storage.push_back(data);
        ref_datas.push_back(data);
        CHECK(check_equal(storage, ref_datas));
    }
    CHECK(storage.get_group_count() == 2);
    CHECK(!is_pristine(storage));
    CHECK(storage.get_last_group_idx() == 1);
    CHECK(storage.get_first_group_idx() == 0);

    size_t count = storage.get_data_count();
    CHECK(count > 10);

    pop_all(storage, ref_datas);

    //fill the storage
    while (storage.push_back(data))
    {
        ref_datas.push_back(data);
        CHECK(check_equal(storage, ref_datas));
    }

    pop_all(storage, ref_datas);
}


static void test_storage_accuracy()
{
    Storage storage;

    typedef Storage::Data Data;

    std::vector<Data> ref_datas;

    auto randposf = []() -> float { return (float(rand() % 10000) / 10000.f); };
    auto randf = [&randposf]() -> float { return randposf() - 0.5f; };

    Data last_data;
    last_data.temperature = randposf() * 80.f;
    last_data.humidity = randposf() * 100.f;
    last_data.vcc = randposf() * 4.2f;

    size_t min_items = std::numeric_limits<size_t>::max();
    size_t max_items = std::numeric_limits<size_t>::lowest();
    float avg_items = 0;

    constexpr float MAX_TEMPERATURE_VARIATION = 1.f;
    constexpr float MAX_HUMIDITY_VARIATION = 10.f;
    constexpr float MAX_VCC_VARIATION = 0.1f;

    for (size_t i = 0; i < 100000; i++)
    {
        //while (true)
        {
            last_data.temperature += randf() * MAX_TEMPERATURE_VARIATION;
            last_data.temperature = std::min(std::max(last_data.temperature, -60.f), 80.f);

            last_data.humidity += randf() * MAX_HUMIDITY_VARIATION;
            last_data.humidity = std::min(std::max(last_data.humidity, 0.f), 100.f);

            last_data.vcc += randf() * MAX_VCC_VARIATION;
            last_data.vcc = std::min(std::max(last_data.vcc, 2.f), 4.5f);

            bool ok = storage.push_back(last_data);
            if (ok)
            {
                ref_datas.push_back(last_data);
            }
            else
            {
                ref_datas.erase(ref_datas.begin(), ref_datas.begin() + 1);
                storage.pop_front();
            }
        }
        min_items = std::min(min_items, ref_datas.size());
        max_items = std::max(max_items, ref_datas.size());
        avg_items = (avg_items + float(ref_datas.size()) / 1000);
        std::cout << "Stored " << ref_datas.size() << " items. " << min_items << " - " << max_items << " - " << avg_items << "\n";


        if (!check_equal(storage, ref_datas))
        {
            std::cout << "Data mismatch\n";
        }
    }
}

void test_storage()
{
    test_storage_operations();
    test_storage_accuracy();
}
