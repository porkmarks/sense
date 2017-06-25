#include "Storage.h"
#include "Sensors.h"
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

////////////////////////////////////////////////////////////////////////////

static bool equal(Sensors::Clock::duration d1, Sensors::Clock::duration d2, Sensors::Clock::duration epsilon)
{
    return std::abs(d2 - d1) <= epsilon;
}

void test_sensor_operations()
{
//    //---------------------

//    {
//        Settings settings;
//        if (!settings.init("test_settings.json"))
//        {
//            std::cerr << "Cannot load to settings\n";
//            return;
//        }

//        Sensors sensors(settings);
//        if (!sensors.init())
//        {
//            std::cerr << "DB init failed\n";
//            return;
//        }

//        sensors.set_measurement_period(std::chrono::minutes(1));
//        CHECK(sensors.get_measurement_period() == std::chrono::minutes(1));

//        sensors.set_measurement_period(std::chrono::minutes(10));
//        CHECK(sensors.get_measurement_period() == std::chrono::minutes(10));
//    }

//    //---------------------

//    {
//        Settings settings;
//        if (!settings.init("test_settings.json"))
//        {
//            std::cerr << "Cannot load to system DB\n";
//            return;
//        }

//        Sensors sensors(settings);
//        if (!sensors.init())
//        {
//            std::cerr << "DB init failed\n";
//            return;
//        }

//        sensors.set_measurement_period(std::chrono::minutes(10));
//        sensors.set_comms_period(std::chrono::minutes(10));
//        CHECK(equal(sensors.compute_comms_period(), std::chrono::minutes(10), Sensors::MEASUREMENT_JITTER));

//        sensors.set_comms_period(std::chrono::minutes(1));
//        CHECK(equal(sensors.compute_comms_period(), std::chrono::minutes(10), Sensors::MEASUREMENT_JITTER));

//        sensors.set_measurement_period(std::chrono::minutes(20));
//        sensors.set_comms_period(std::chrono::minutes(10));
//        CHECK(equal(sensors.compute_comms_period(), std::chrono::minutes(20), Sensors::MEASUREMENT_JITTER));

//        sensors.set_comms_period(std::chrono::minutes(1));
//        CHECK(equal(sensors.compute_comms_period(), std::chrono::minutes(20), Sensors::MEASUREMENT_JITTER));

//        sensors.set_measurement_period(std::chrono::minutes(1));
//        sensors.set_comms_period(std::chrono::minutes(10));
//        CHECK(equal(sensors.compute_comms_period(), std::chrono::minutes(10), Sensors::MEASUREMENT_JITTER));

//        sensors.set_comms_period(std::chrono::minutes(1));
//        CHECK(equal(sensors.compute_comms_period(), std::chrono::minutes(1), Sensors::MEASUREMENT_JITTER));
//    }

//    //---------------------

//    for (size_t sensor_count = 0; sensor_count < 100; sensor_count++)
//    {
//        std::cout << "Sensor count: " << sensor_count << "\n";

//        Settings settings;
//        if (!settings.init("test_settings.json"))
//        {
//            std::cerr << "Cannot load to settings\n";
//            return;
//        }

//        Sensors sensors(settings);
//        if (!sensors.init())
//        {
//            std::cerr << "DB init failed\n";
//            return;
//        }

//        for (size_t i = 0; i < sensor_count; i++)
//        {
//            sensors.add_sensor("s", i);
//        }

//        Sensors::Clock::duration total_comms_duration = Sensors::COMMS_DURATION * sensor_count;

//        sensors.set_measurement_period(std::chrono::minutes(10));
//        sensors.set_comms_period(std::chrono::minutes(10));
//        CHECK(equal(sensors.compute_comms_period(), std::max(total_comms_duration, sensors.get_measurement_period()), Sensors::MEASUREMENT_JITTER));

//        sensors.set_comms_period(std::chrono::minutes(1));
//        CHECK(equal(sensors.compute_comms_period(), std::max(total_comms_duration, sensors.get_measurement_period()), Sensors::MEASUREMENT_JITTER));

//        sensors.set_measurement_period(std::chrono::minutes(20));
//        sensors.set_comms_period(std::chrono::minutes(10));
//        CHECK(equal(sensors.compute_comms_period(), std::max(total_comms_duration, sensors.get_measurement_period()), Sensors::MEASUREMENT_JITTER));

//        sensors.set_comms_period(std::chrono::minutes(1));
//        CHECK(equal(sensors.compute_comms_period(), std::max(total_comms_duration, sensors.get_measurement_period()), Sensors::MEASUREMENT_JITTER));

//        sensors.set_measurement_period(std::chrono::minutes(1));
//        sensors.set_comms_period(std::chrono::minutes(10));
//        CHECK(equal(sensors.compute_comms_period(), std::max(Sensors::Clock::duration(std::chrono::minutes(10)), std::max(total_comms_duration, sensors.get_measurement_period())), Sensors::MEASUREMENT_JITTER));

//        sensors.set_comms_period(std::chrono::minutes(1));
//        CHECK(equal(sensors.compute_comms_period(), std::max(total_comms_duration, sensors.get_measurement_period()), Sensors::MEASUREMENT_JITTER));
//    }

//    //---------------------

//    {
//        Settings settings;
//        if (!settings.init("test_settings.json"))
//        {
//            std::cerr << "Cannot load to settings\n";
//            return;
//        }

//        Sensors sensors(settings);
//        if (!sensors.init())
//        {
//            std::cerr << "DB init failed\n";
//            return;
//        }

//        const Sensors::Sensor* sensor = sensors.find_sensor_by_id(Sensors::Sensor_Id(7));
//        CHECK(sensor == nullptr);
//        Sensors::Sensor_Id id = sensor->id;

//        sensor = sensors.add_sensor("s1", 0);
//        CHECK(sensor);
//        sensor = sensors.find_sensor_by_id(id);
//        CHECK(sensor != nullptr);
//        CHECK(sensor->name == "s1");
//        CHECK(sensor->id == id);
//        sensors.remove_sensor(id);
//        sensor = sensors.find_sensor_by_id(id);
//        CHECK(sensor == nullptr);
//    }

//    //---------------------

//    {
//        Settings settings;
//        if (!settings.init("test_settings.json"))
//        {
//            std::cerr << "Cannot load to settings\n";
//            return;
//        }

//        Sensors sensors(settings);
//        if (!sensors.init())
//        {
//            std::cerr << "DB init failed\n";
//            return;
//        }

//        Sensors::Sensor const* sensor = sensors.add_sensor("s1", 1);
//        CHECK(sensor);
//        Sensors::Sensor_Id id1 = sensor->id;
//        sensor = sensors.add_sensor("s2", 2);
//        CHECK(sensor);
//        Sensors::Sensor_Id id2 = sensor->id;

//        const Sensors::Sensor* sensor1 = sensors.find_sensor_by_id(id1);
//        CHECK(sensor1 != nullptr);
//        CHECK(sensor1->name == "s1");
//        CHECK(sensor1->id == id1);
//        const Sensors::Sensor* sensor2 = sensors.find_sensor_by_id(id2);
//        CHECK(sensor2 != nullptr);
//        CHECK(sensor2->name == "s2");
//        CHECK(sensor2->id == id2);

//        sensors.remove_sensor(id1);
//        sensor1 = sensors.find_sensor_by_id(id1);
//        CHECK(sensor1 == nullptr);
//        sensor2 = sensors.find_sensor_by_id(id2);
//        CHECK(sensor2 != nullptr);
//        CHECK(sensor2->name == "s2");
//        CHECK(sensor2->id == id2);

//        sensors.remove_sensor(id2);
//        sensor2 = sensors.find_sensor_by_id(id2);
//        CHECK(sensor2 == nullptr);
//    }

//    //---------------------

//    {
//        Settings settings;
//        if (!settings.init("test_settings.json"))
//        {
//            std::cerr << "Cannot load to settings\n";
//            return;
//        }

//        Sensors sensors(settings);
//        if (!sensors.init())
//        {
//            std::cerr << "DB init failed\n";
//            return;
//        }

//        Sensors::Sensor const* sensor = sensors.add_sensor("s1", 1);
//        CHECK(sensor);
//        Sensors::Sensor_Id id1 = sensor->id;
//        sensor = sensors.add_sensor("s2", 2);
//        CHECK(sensor);
//        Sensors::Sensor_Id id2 = sensor->id;

//        const Sensors::Sensor* sensor1 = sensors.find_sensor_by_id(id1);
//        CHECK(sensor1 != nullptr);
//        CHECK(sensor1->name == "s1");
//        CHECK(sensor1->id == id1);
//        const Sensors::Sensor* sensor2 = sensors.find_sensor_by_id(id2);
//        CHECK(sensor2 != nullptr);
//        CHECK(sensor2->name == "s2");
//        CHECK(sensor2->id == id2);

//        sensors.remove_sensor(id2);
//        sensor2 = sensors.find_sensor_by_id(id2);
//        CHECK(sensor2 == nullptr);
//        sensor1 = sensors.find_sensor_by_id(id1);
//        CHECK(sensor1 != nullptr);
//        CHECK(sensor1->name == "s1");
//        CHECK(sensor1->id == id1);

//        sensors.remove_sensor(id1);
//        sensor1 = sensors.find_sensor_by_id(id1);
//        CHECK(sensor1 == nullptr);
//    }

}

void test_sensors()
{
    test_sensor_operations();
}

/////////////////////////////////////////////////////////////////////////////


void run_tests()
{
    //test_storage();
    test_sensors();
}
