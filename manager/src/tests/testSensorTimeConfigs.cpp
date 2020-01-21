#include "cstdio"
#include "Logger.h"
#include <iostream>
#include "DB.h"
#include "testUtils.h"
#include <set>

void testSensorTimeConfig()
{
	std::cout << "Testing sensor time configs\n";
    {
        std::cout << "\tTesting default sensor time configs\n";
		std::shared_ptr<ManualClock> clock = std::make_shared<ManualClock>();
		DB db(clock);

        auto test = [&db, &clock]()
        {
            CHECK_TRUE(db.getSensorTimeConfigCount() == 1);
            DB::SensorTimeConfig config = db.getSensorTimeConfig(0);
            CHECK_EQUALS(config.baselineMeasurementIndex, 0);
            CHECK_TRUE(config.baselineMeasurementTimePoint == clock->now());
            CHECK_TRUE(config.descriptor.commsPeriod > std::chrono::minutes(1));
            CHECK_TRUE(config.descriptor.measurementPeriod > std::chrono::minutes(1));
            CHECK_TRUE(config.descriptor.commsPeriod > config.descriptor.measurementPeriod);
            CHECK_TRUE(db.computeActualCommsPeriod(config.descriptor) >= config.descriptor.commsPeriod);
            CHECK_TRUE(config == db.getLastSensorTimeConfig());
            CHECK_TRUE(config == db.findSensorTimeConfigForMeasurementIndex(0));
            CHECK_TRUE(config == db.findSensorTimeConfigForMeasurementIndex(1));
            CHECK_TRUE(config == db.findSensorTimeConfigForMeasurementIndex(1000));
        };

        createDB(db);
        test();
        closeDB(db);
        loadDB(db);
        test();
        closeDB(db);
    }
	{
		std::cout << "\tTesting extra sensor time config\n";
		std::shared_ptr<ManualClock> clock = std::make_shared<ManualClock>();
		DB db(clock);

		auto test = [&db, &clock](size_t index, size_t count, IClock::duration cd, IClock::duration md, IClock::time_point baselineTimePoint, uint32_t fromIndex)
		{
			CHECK_TRUE(db.getSensorTimeConfigCount() == count);
			DB::SensorTimeConfig config = db.getSensorTimeConfig(index);
			CHECK_EQUALS(config.baselineMeasurementIndex, fromIndex);
			CHECK_TRUE(config.baselineMeasurementTimePoint == baselineTimePoint);
			CHECK_TRUE(config.descriptor.commsPeriod == cd);
			CHECK_TRUE(config.descriptor.measurementPeriod == md);
			CHECK_TRUE(db.computeActualCommsPeriod(config.descriptor) >= config.descriptor.commsPeriod);
            if (index + 1 == count)
			{
				CHECK_TRUE(config == db.getLastSensorTimeConfig());
			}
            for (uint32_t index = 0; index < fromIndex; index++)
			{
				CHECK_TRUE(config != db.findSensorTimeConfigForMeasurementIndex(index));
			}
			for (uint32_t index = fromIndex; index < fromIndex * 2; index++)
			{
				CHECK_TRUE(config == db.findSensorTimeConfigForMeasurementIndex(index));
			}
		};

		createDB(db);
		clock->advance(db.getLastSensorTimeConfig().descriptor.measurementPeriod * 3); //3 cycles
        IClock::time_point configBaselineTP1 = clock->now();
		{
			DB::SensorTimeConfigDescriptor descriptor;
			descriptor.commsPeriod = std::chrono::minutes(21);
			descriptor.measurementPeriod = std::chrono::minutes(18);
			CHECK_SUCCESS(db.addSensorTimeConfig(descriptor));
			test(1, 2, std::chrono::minutes(21), std::chrono::minutes(18), configBaselineTP1, 3);
		}
		clock->advance(db.getLastSensorTimeConfig().descriptor.measurementPeriod * 5); //5 cycles
		IClock::time_point configBaselineTP2 = clock->now();
		{
			DB::SensorTimeConfigDescriptor descriptor;
			descriptor.commsPeriod = std::chrono::minutes(41);
			descriptor.measurementPeriod = std::chrono::minutes(28);
			CHECK_SUCCESS(db.addSensorTimeConfig(descriptor));
			test(1, 3, std::chrono::minutes(21), std::chrono::minutes(18), configBaselineTP1, 3);
			test(2, 3, std::chrono::minutes(41), std::chrono::minutes(28), configBaselineTP2, 8);
		}
        closeDB(db);
		loadDB(db);
		test(1, 3, std::chrono::minutes(21), std::chrono::minutes(18), configBaselineTP1, 3);
		test(2, 3, std::chrono::minutes(41), std::chrono::minutes(28), configBaselineTP2, 8);
		closeDB(db);
	}
    {
        std::cout << "\tTesting invalid sensor time configs\n";
        std::shared_ptr<ManualClock> clock = std::make_shared<ManualClock>();
        DB db(clock);

        auto test = [&db, &clock]()
        {
            CHECK_TRUE(db.getSensorTimeConfigCount() == 1);
            DB::SensorTimeConfig config = db.getSensorTimeConfig(0);
            CHECK_EQUALS(config.baselineMeasurementIndex, 0);
            CHECK_TRUE(config.baselineMeasurementTimePoint == clock->now());
            CHECK_TRUE(config.descriptor.commsPeriod > std::chrono::minutes(1));
            CHECK_TRUE(config.descriptor.measurementPeriod > std::chrono::minutes(1));
            CHECK_TRUE(config.descriptor.commsPeriod > config.descriptor.measurementPeriod);
            CHECK_TRUE(db.computeActualCommsPeriod(config.descriptor) >= config.descriptor.commsPeriod);
            CHECK_TRUE(config == db.getLastSensorTimeConfig());
            CHECK_TRUE(config == db.findSensorTimeConfigForMeasurementIndex(0));
            CHECK_TRUE(config == db.findSensorTimeConfigForMeasurementIndex(1));
            CHECK_TRUE(config == db.findSensorTimeConfigForMeasurementIndex(1000));
        };

        createDB(db);
        {
            DB::SensorTimeConfigDescriptor descriptor;
            descriptor.commsPeriod = std::chrono::seconds(25);
            CHECK_FAILURE(db.addSensorTimeConfig(descriptor));
            test();
        }
        {
            DB::SensorTimeConfigDescriptor descriptor;
            descriptor.measurementPeriod = std::chrono::seconds(9);
            CHECK_FAILURE(db.addSensorTimeConfig(descriptor));
            test();
        }
        {
            DB::SensorTimeConfigDescriptor descriptor;
            descriptor.commsPeriod = std::chrono::seconds(60);
            descriptor.measurementPeriod = std::chrono::seconds(61);
            CHECK_FAILURE(db.addSensorTimeConfig(descriptor));
            test();
        }
        test();
        closeDB(db);
        loadDB(db);
        test();
        closeDB(db);
    }
    {
        std::cout << "\tTesting sensor time configs with many sensors\n";
		std::shared_ptr<ManualClock> clock = std::make_shared<ManualClock>();
		DB db(clock);

        auto test = [&db, &clock](size_t count)
        {
            DB::SensorTimeConfig config = db.getLastSensorTimeConfig();
            IClock::duration d = db.computeActualCommsPeriod(config.descriptor);
            CHECK_TRUE(d >= std::chrono::seconds(10) * count);
            CHECK_TRUE(d < std::chrono::seconds(10) * (count + 1));
        };

		createDB(db);
		{
			DB::SensorTimeConfigDescriptor descriptor;
            descriptor.commsPeriod = std::chrono::minutes(1);
            descriptor.measurementPeriod = std::chrono::seconds(30);
            CHECK_SUCCESS(db.addSensorTimeConfig(descriptor));
            DB::SensorTimeConfig config = db.getLastSensorTimeConfig();
            CHECK_TRUE(db.computeActualCommsPeriod(config.descriptor) >= config.descriptor.commsPeriod);
            for (size_t i = 0; i < 10; i++)
            {
                DB::SensorDescriptor sensorD;
                sensorD.name = "test" + std::to_string(i);
                CHECK_SUCCESS(db.addSensor(sensorD));
                CHECK_SUCCESS(db.bindSensor(DB::SensorAddress(i), 1u, 2u, 3u, {}));
            }
		}
        test(10);
        for (size_t i = 10; i < 20; i++)
        {
            DB::SensorDescriptor sensorD;
            sensorD.name = "test" + std::to_string(i);
            CHECK_SUCCESS(db.addSensor(sensorD));
            CHECK_SUCCESS(db.bindSensor(DB::SensorAddress(i), 1u, 2u, 3u, {}));
        }
        test(20);
        closeDB(db);
		loadDB(db);
        test(20);
		closeDB(db);
	}
}
