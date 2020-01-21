#include "cstdio"
#include "Logger.h"
#include <iostream>
#include "DB.h"
#include "testUtils.h"
#include <set>

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

