#include "stdio.h"
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
    CHECK_BOOL(exportId);
    CHECK_BOOL(exportIndex);
    CHECK_BOOL(exportSensorName);
    CHECK_BOOL(exportSensorSN);
    CHECK_BOOL(exportTimePoint);
    CHECK_BOOL(exportReceivedTimePoint);
    CHECK_BOOL(exportTemperature);
    CHECK_BOOL(exportHumidity);
    CHECK_BOOL(exportBattery);
    CHECK_BOOL(exportSignal);
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
    for (auto value : std::map<int, bool>{ {-10, false}, {-3, true}, {0, true}, {5, true}, {10, true}, {17, true}, {20, true}, {22, false} })
	{
		std::cout << "\t\t" << value.first << "dBm: Should " << (value.second ? "succeed" : "fail") << "\n";
		DB db;
		createDB(db);
		DB::SensorSettings settings;
        settings.radioPower = (int8_t)value.first;
		CHECK_RESULT(db.setSensorSettings(settings), value.second);
		CHECK_EQUALS(db.getSensorSettings().radioPower == (int8_t)value.first, value.second);
		closeDB(db);
		loadDB(db);
		CHECK_EQUALS(db.getSensorSettings().radioPower == (int8_t)value.first, value.second);
		closeDB(db);
	}
	std::cout << "\tTesting retries\n";
	for (auto value : std::map<uint32_t, bool>{ {0u, false}, {1u, true}, {2u, true}, {10u, true}, {11u, false}})
	{
        std::cout << "\t\t" << value.first << ": Should " << (value.second ? "succeed" : "fail") << "\n";
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


int main(int, const char*[])
{
    testGeneralSettings();
    testCsvSettings();
    testSensorSettings();

    return 0;
}
