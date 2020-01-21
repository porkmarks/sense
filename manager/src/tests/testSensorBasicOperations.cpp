#include "cstdio"
#include "Logger.h"
#include <iostream>
#include "DB.h"
#include "testUtils.h"
#include <set>

void testSensorBasicOperations()
{
    std::cout << "Testing sensor basic operations\n";
    {
        std::cout << "\tBaseline\n";
        std::shared_ptr<ManualClock> clock = std::make_shared<ManualClock>();
        clock->advance(std::chrono::seconds(5));
        DB db(clock);
        createDB(db);
        CHECK_TRUE(db.getSensorCount() == 0);
        closeDB(db);
        loadDB(db);
        CHECK_TRUE(db.getSensorCount() == 0);
        closeDB(db);
    }
    {
        std::cout << "\tTesting add/set/remove\n";
        std::shared_ptr<ManualClock> clock = std::make_shared<ManualClock>();
        clock->advance(std::chrono::seconds(5));
        DB db(clock);
        createDB(db);
        CHECK_TRUE(db.getSensorCount() == 0);
        for (size_t i = 0; i < 10; i++)
        {
            DB::SensorDescriptor descriptor;
            descriptor.name = "s" + std::to_string(i);
            CHECK_SUCCESS(db.addSensor(descriptor));
            CHECK_TRUE(db.getSensorCount() == i + 1);
            DB::Sensor sensor = db.getSensor(i);
            CHECK_TRUE(sensor.descriptor == descriptor);

            CHECK_FAILURE(db.addSensor(descriptor)); //same name!!!
            {
                DB::SensorDescriptor descriptor2;
                descriptor2.name = "s2";
                CHECK_FAILURE(db.addSensor(descriptor2)); //unbound sensor already added!!!
            }

            CHECK_TRUE(db.findSensorIndexById(sensor.id) == int32_t(i));
            CHECK_TRUE(db.findSensorById(sensor.id) != std::nullopt);
            CHECK_TRUE(db.findSensorById(sensor.id) == sensor);

            CHECK_TRUE(db.findSensorIndexByName(sensor.descriptor.name) == int32_t(i));
            CHECK_TRUE(db.findSensorByName(sensor.descriptor.name) != std::nullopt);
            CHECK_TRUE(db.findSensorByName(sensor.descriptor.name) == sensor);

            DB::SensorDescriptor descriptor2;
            descriptor2.name = "x" + std::to_string(i);
            CHECK_SUCCESS(db.setSensor(sensor.id, descriptor2));
            sensor = db.getSensor(i);
            CHECK_TRUE(sensor.descriptor == descriptor2);

            CHECK_TRUE(db.findSensorIndexByName(descriptor.name) == -1);
            CHECK_TRUE(db.findSensorByName(descriptor.name) == std::nullopt);

            CHECK_TRUE(db.findSensorIndexByName(descriptor2.name) == int32_t(i));
            CHECK_TRUE(db.findSensorByName(descriptor2.name) != std::nullopt);
            CHECK_TRUE(db.findSensorByName(descriptor2.name) == sensor);

            CHECK_TRUE(db.findSensorIndexBySerialNumber(sensor.serialNumber) == int32_t(i));
            CHECK_TRUE(db.findSensorBySerialNumber(sensor.serialNumber) != std::nullopt);
            CHECK_TRUE(db.findSensorBySerialNumber(sensor.serialNumber) == sensor);

            CHECK_TRUE(db.findSensorIndexByAddress(sensor.address) == int32_t(i));
            CHECK_TRUE(db.findSensorByAddress(sensor.address) != std::nullopt);
            CHECK_TRUE(db.findSensorByAddress(sensor.address) == sensor);

            CHECK_TRUE(db.findUnboundSensorIndex() == int32_t(i));
            CHECK_TRUE(db.findUnboundSensor() != std::nullopt);
            CHECK_TRUE(db.findUnboundSensor() == sensor);
            CHECK_SUCCESS(db.bindSensor(DB::SensorAddress(i + 100), uint8_t(i), uint8_t(i + 1), uint8_t(i + 2), {}));
            CHECK_TRUE(db.findUnboundSensorIndex() == -1);
            CHECK_TRUE(db.findUnboundSensor() == std::nullopt);
        }

        size_t count = 10;
        while (db.getSensorCount() > 0)
        {
            DB::Sensor sensor = db.getSensor(0);
            db.removeSensor(0);
            CHECK_EQUALS(db.getSensorCount(), count - 1);
            Result<DB::SensorId> result = db.addSensor(sensor.descriptor);
            CHECK_TRUE(result == success);
            CHECK_EQUALS(db.getSensorCount(), count);
            db.removeSensorById(result.payload());
            CHECK_EQUALS(db.getSensorCount(), count - 1);
            count--;

            CHECK_TRUE(db.findSensorIndexById(sensor.id) == -1);
            CHECK_TRUE(db.findSensorById(sensor.id) == std::nullopt);

            CHECK_TRUE(db.findSensorIndexByName(sensor.descriptor.name) == -1);
            CHECK_TRUE(db.findSensorByName(sensor.descriptor.name) == std::nullopt);

            CHECK_TRUE(db.findSensorIndexBySerialNumber(sensor.serialNumber) == -1);
            CHECK_TRUE(db.findSensorBySerialNumber(sensor.serialNumber) == std::nullopt);

            CHECK_TRUE(db.findSensorIndexByAddress(sensor.address) == -1);
            CHECK_TRUE(db.findSensorByAddress(sensor.address) == std::nullopt);

            CHECK_TRUE(db.findUnboundSensorIndex() == -1);
            CHECK_TRUE(db.findUnboundSensor() == std::nullopt);
        }

        closeDB(db);
        loadDB(db);
        closeDB(db);
    }
    {
        std::cout << "\tTesting binding\n";
        std::shared_ptr<ManualClock> clock = std::make_shared<ManualClock>();
        clock->advance(std::chrono::seconds(5));
        DB db(clock);

        auto testUnboundSensor = [&db, &clock](DB::SensorDescriptor const& descriptor)
        {
            DB::Sensor s = db.getSensor(0);
            CHECK_TRUE(s.descriptor == descriptor);
            CHECK_TRUE(s.serialNumber == 0);
            CHECK_TRUE(s.deviceInfo.sensorType == 0);
            CHECK_TRUE(s.deviceInfo.hardwareVersion == 0);
            CHECK_TRUE(s.deviceInfo.softwareVersion == 0);
            CHECK_TRUE(s.calibration.temperatureBias == 0.f);
            CHECK_TRUE(s.calibration.humidityBias == 0.f);
            CHECK_TRUE(s.state == DB::Sensor::State::Unbound);
            CHECK_TRUE(s.shouldSleep == false);
            CHECK_TRUE(s.sleepStateTimePoint == IClock::time_point(IClock::duration::zero()));
            CHECK_TRUE(s.stats == DB::SensorStats());
            CHECK_TRUE(s.lastCommsTimePoint == IClock::time_point(IClock::duration::zero()));
            CHECK_TRUE(s.blackout == false);
            CHECK_TRUE(s.lastConfirmedMeasurementIndex == 0);
            CHECK_TRUE(s.lastAlarmProcessesMeasurementIndex == 0);
            CHECK_TRUE(s.firstStoredMeasurementIndex == 0);
            CHECK_TRUE(s.estimatedStoredMeasurementCount == 0);
            CHECK_TRUE(s.lastSignalStrengthB2S == 0);
            CHECK_TRUE(s.averageSignalStrength.b2s == 0);
            CHECK_TRUE(s.averageSignalStrength.s2b == 0);
            CHECK_TRUE(s.isRTMeasurementValid == false);
            CHECK_TRUE(s.addedTimePoint == clock->now());
        };
        auto testBoundSensor = [&db, &clock](DB::SensorDescriptor const& descriptor)
        {
            DB::Sensor s = db.getSensor(0);
            CHECK_TRUE(s.descriptor == descriptor);
            CHECK_TRUE(s.serialNumber == 1234u);
            CHECK_TRUE(s.deviceInfo.sensorType == 1u);
            CHECK_TRUE(s.deviceInfo.hardwareVersion == 2u);
            CHECK_TRUE(s.deviceInfo.softwareVersion == 3u);
            CHECK_TRUE(s.calibration.temperatureBias == 0.7f);
            CHECK_TRUE(s.calibration.humidityBias == -0.9f);
            CHECK_TRUE(s.state == DB::Sensor::State::Active);
            CHECK_TRUE(s.shouldSleep == false);
            CHECK_TRUE(s.sleepStateTimePoint == IClock::time_point(IClock::duration::zero()));
            CHECK_TRUE(s.stats == DB::SensorStats());
            CHECK_TRUE(s.lastCommsTimePoint == IClock::time_point(IClock::duration::zero()));
            CHECK_TRUE(s.blackout == false);
            CHECK_TRUE(s.lastConfirmedMeasurementIndex == 0);
            CHECK_TRUE(s.lastAlarmProcessesMeasurementIndex == 0);
            CHECK_TRUE(s.firstStoredMeasurementIndex == 0);
            CHECK_TRUE(s.estimatedStoredMeasurementCount == 0);
            CHECK_TRUE(s.lastSignalStrengthB2S == 0);
            CHECK_TRUE(s.averageSignalStrength.b2s == 0);
            CHECK_TRUE(s.averageSignalStrength.s2b == 0);
            CHECK_TRUE(s.isRTMeasurementValid == false);
            CHECK_TRUE(s.addedTimePoint == clock->now());
        };

        createDB(db);
        CHECK_TRUE(db.getSensorCount() == 0);
        DB::SensorDescriptor descriptor;
        descriptor.name = "s1";
        CHECK_SUCCESS(db.addSensor(descriptor));
        CHECK_TRUE(db.getSensorCount() == 1);
        CHECK_TRUE(db.getSensor(0).descriptor == descriptor);
        CHECK_FAILURE(db.addSensor(descriptor)); //same name!!!
        {
            DB::SensorDescriptor descriptor2;
            descriptor2.name = "s2";
            CHECK_FAILURE(db.addSensor(descriptor2)); //unbound sensor already added!!!
        }
        testUnboundSensor(descriptor);
        CHECK_SUCCESS(db.bindSensor(DB::SensorAddress(1234u), 1u, 2u, 3u, {0.7f, -0.9f}));
        testBoundSensor(descriptor);
        closeDB(db);
        loadDB(db);
        testBoundSensor(descriptor);
        closeDB(db);
    }
}

