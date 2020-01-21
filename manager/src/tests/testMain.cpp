#include "cstdio"
#include "Logger.h"
#include <iostream>
#include "DB.h"
#include "testUtils.h"
#include <set>

void testGeneralSettings()
{
    std::cout << "Testing General Settings\n";

    std::set<DB::DateTimeFormat> formats =
    {
        DB::DateTimeFormat::DD_MM_YYYY_Dash,
        DB::DateTimeFormat::YYYY_MM_DD_Dash,
        DB::DateTimeFormat::MM_DD_YYYY_Slash,
        DB::DateTimeFormat::YYYY_MM_DD_Slash
    };

    for (auto format: formats)
    {
        DB db;
        createDB(db);
        DB::GeneralSettings settings;
        settings.dateTimeFormat = format;
        db.setGeneralSettings(settings);
        CHECK_TRUE(db.getGeneralSettings().dateTimeFormat == format);
        closeDB(db);
        loadDB(db);
        CHECK_TRUE(db.getGeneralSettings().dateTimeFormat == format);
        closeDB(db);
    }
}

void testCsvSettings()
{
    std::cout << "Testing CSV Settings\n";

    {
        std::cout << "\tTesting date time formats\n";
        std::set<std::optional<DB::DateTimeFormat>> formats =
        {
            DB::DateTimeFormat::DD_MM_YYYY_Dash,
            DB::DateTimeFormat::YYYY_MM_DD_Dash,
            DB::DateTimeFormat::MM_DD_YYYY_Slash,
            DB::DateTimeFormat::YYYY_MM_DD_Slash,
            std::nullopt
        };

        for (auto format: formats)
        {
            DB db;
            createDB(db);
            DB::CsvSettings settings;
            settings.dateTimeFormatOverride = format;
            db.setCsvSettings(settings);
            CHECK_TRUE(db.getCsvSettings().dateTimeFormatOverride == format);
            closeDB(db);
            loadDB(db);
            CHECK_TRUE(db.getCsvSettings().dateTimeFormatOverride == format);
            closeDB(db);
        }
    }
    {
        std::cout << "\tTesting units formats\n";
        std::set<DB::CsvSettings::UnitsFormat> formats =
        {
            DB::CsvSettings::UnitsFormat::None,
            DB::CsvSettings::UnitsFormat::Embedded,
            DB::CsvSettings::UnitsFormat::SeparateColumn
        };

        for (auto format: formats)
        {
            DB db;
            createDB(db);
            DB::CsvSettings settings;
            settings.unitsFormat = format;
            db.setCsvSettings(settings);
            CHECK_TRUE(db.getCsvSettings().unitsFormat == format);
            closeDB(db);
            loadDB(db);
            CHECK_TRUE(db.getCsvSettings().unitsFormat == format);
            closeDB(db);
        }
    }

    std::cout << "\tTesting export flags\n";
#define CHECK_BOOL(name)                                    \
    for (auto value: { true, false })                       \
    {                                                       \
        DB db;                                              \
        createDB(db);                                       \
        DB::CsvSettings settings;                           \
        settings.name = value;                              \
        db.setCsvSettings(settings);                        \
        CHECK_TRUE(db.getCsvSettings().name == value);      \
        closeDB(db);                                        \
        loadDB(db);                                         \
        CHECK_TRUE(db.getCsvSettings().name == value);      \
        closeDB(db);                                        \
    }
    CHECK_BOOL(exportId)
    CHECK_BOOL(exportIndex)
    CHECK_BOOL(exportSensorName)
    CHECK_BOOL(exportSensorSN)
    CHECK_BOOL(exportTimePoint)
    CHECK_BOOL(exportReceivedTimePoint)
    CHECK_BOOL(exportTemperature)
    CHECK_BOOL(exportHumidity)
    CHECK_BOOL(exportBattery)
    CHECK_BOOL(exportSignal)
#undef CHECK_BOOL


    std::cout << "\tTesting decimal places\n";
    for (auto value: { 0u, 1u, 2u, 3u })
    {
        DB db;
        createDB(db);
        DB::CsvSettings settings;
        settings.decimalPlaces = value;
        db.setCsvSettings(settings);
        CHECK_TRUE(db.getCsvSettings().decimalPlaces == value);
        closeDB(db);
        loadDB(db);
        CHECK_TRUE(db.getCsvSettings().decimalPlaces == value);
        closeDB(db);
    }

    std::cout << "\tTesting separators\n";
    for (auto value: {"/", ",", " ", "|", ""})
    {
        DB db;
        createDB(db);
        DB::CsvSettings settings;
        settings.separator = value;
        db.setCsvSettings(settings);
        CHECK_TRUE(db.getCsvSettings().separator == value);
        closeDB(db);
        loadDB(db);
        CHECK_TRUE(db.getCsvSettings().separator == value);
        closeDB(db);
    }
}


void testSensorSettings()
{
	std::cout << "Testing Sensor Settings\n";

	std::cout << "\tTesting radio power\n";
    for (auto value : std::map<int8_t, bool>{ {-10, false}, {-3, true}, {0, true}, {5, true}, {10, true}, {17, true}, {20, true}, {22, false} })
	{
        std::cout << "\t\t" << int32_t(value.first) << "dBm: Should " << (value.second ? "succeed" : "fail") << "\n";
		DB db;
		createDB(db);
		DB::SensorSettings settings;
        settings.radioPower = value.first;
		CHECK_RESULT(db.setSensorSettings(settings), value.second);
        CHECK_EQUALS(db.getSensorSettings().radioPower == value.first, value.second);
		closeDB(db);
		loadDB(db);
        CHECK_EQUALS(db.getSensorSettings().radioPower == value.first, value.second);
		closeDB(db);
	}
	std::cout << "\tTesting retries\n";
    for (auto value : std::map<uint8_t, bool>{ {0u, false}, {1u, true}, {2u, true}, {10u, true}, {11u, false}})
	{
        std::cout << "\t\t" << int32_t(value.first) << ": Should " << (value.second ? "succeed" : "fail") << "\n";
		DB db;
		createDB(db);
		DB::SensorSettings settings;
		settings.retries = value.first;
		CHECK_RESULT(db.setSensorSettings(settings), value.second);
		CHECK_EQUALS(db.getSensorSettings().retries == value.first, value.second);
		closeDB(db);
		loadDB(db);
		CHECK_EQUALS(db.getSensorSettings().retries == value.first, value.second);
		closeDB(db);
	}
	std::cout << "\tTesting alert battery level\n";
	for (auto value : std::map<float, bool>{ {-0.1f, false}, {0.f, true}, {0.5f, true}, {0.99f, true}, {1.001f, false} })
	{
        std::cout << "\t\t" << value.first << ": Should " << (value.second ? "succeed" : "fail") << "\n";
		DB db;
		createDB(db);
		DB::SensorSettings settings;
		settings.alertBatteryLevel = value.first;
		CHECK_RESULT(db.setSensorSettings(settings), value.second);
		CHECK_EQUALS(db.getSensorSettings().alertBatteryLevel == value.first, value.second);
		closeDB(db);
		loadDB(db);
		CHECK_EQUALS(db.getSensorSettings().alertBatteryLevel == value.first, value.second);
		closeDB(db);
	}
	std::cout << "\tTesting alert signal level\n";
	for (auto value : std::map<float, bool>{ {-0.1f, false}, {0.f, true}, {0.5f, true}, {0.99f, true}, {1.001f, false} })
	{
        std::cout << "\t\t" << value.first << ": Should " << (value.second ? "succeed" : "fail") << "\n";
		DB db;
		createDB(db);
		DB::SensorSettings settings;
		settings.alertSignalStrengthLevel = value.first;
		CHECK_RESULT(db.setSensorSettings(settings), value.second);
		CHECK_EQUALS(db.getSensorSettings().alertSignalStrengthLevel == value.first, value.second);
		closeDB(db);
		loadDB(db);
		CHECK_EQUALS(db.getSensorSettings().alertSignalStrengthLevel == value.first, value.second);
		closeDB(db);
	}
}

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

int main(int, const char*[])
{
//    testGeneralSettings();
//    testCsvSettings();
//    testSensorSettings();
//    testSensorTimeConfig();
    testSensorBasicOperations();

    return 0;
}
