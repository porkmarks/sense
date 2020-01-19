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
        {
            DB db;
            createDB(db);

            DB::GeneralSettings settings;
            settings.dateTimeFormat = format;
            db.setGeneralSettings(settings);
            CHECK_TRUE(db.getGeneralSettings().dateTimeFormat == format);
            closeDB(db);
        }
        {
            DB db;
            loadDB(db);
            CHECK_TRUE(db.getGeneralSettings().dateTimeFormat == format);
            closeDB(db);
        }
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
            {
                DB db;
                createDB(db);

                DB::CsvSettings settings;
                settings.dateTimeFormatOverride = format;
                db.setCsvSettings(settings);

                CHECK_TRUE(db.getCsvSettings().dateTimeFormatOverride == format);
                closeDB(db);
            }
            {
                DB db;
                loadDB(db);
                CHECK_TRUE(db.getCsvSettings().dateTimeFormatOverride == format);
                closeDB(db);
            }
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
            {
                DB db;
                createDB(db);

                DB::CsvSettings settings;
                settings.unitsFormat = format;
                db.setCsvSettings(settings);

                CHECK_TRUE(db.getCsvSettings().unitsFormat == format);
                closeDB(db);
            }
            {
                DB db;
                loadDB(db);
                CHECK_TRUE(db.getCsvSettings().unitsFormat == format);
                closeDB(db);
            }
        }
    }

    std::cout << "\tTesting export flags\n";
#define CHECK_BOOL(name)                                        \
    for (auto value: { true, false })                           \
    {                                                           \
        {                                                       \
            DB db;                                              \
            createDB(db);                                       \
            DB::CsvSettings settings;                           \
            settings.name = value;                              \
            db.setCsvSettings(settings);                        \
            CHECK_TRUE(db.getCsvSettings().name == value);      \
            closeDB(db);                                        \
        }                                                       \
        {                                                       \
            DB db;                                              \
            loadDB(db);                                         \
            CHECK_TRUE(db.getCsvSettings().name == value);      \
            closeDB(db);                                        \
        }                                                       \
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
        {
            DB db;
            createDB(db);
            DB::CsvSettings settings;
            settings.decimalPlaces = value;
            db.setCsvSettings(settings);
            CHECK_TRUE(db.getCsvSettings().decimalPlaces == value);
            closeDB(db);
        }
        {
            DB db;
            loadDB(db);
            CHECK_TRUE(db.getCsvSettings().decimalPlaces == value);
            closeDB(db);
        }
    }

    std::cout << "\tTesting separators\n";
    for (auto value: {"/", ",", " ", "|", ""})
    {
        {
            DB db;
            createDB(db);
            DB::CsvSettings settings;
            settings.separator = value;
            db.setCsvSettings(settings);
            CHECK_TRUE(db.getCsvSettings().separator == value);
            closeDB(db);
        }
        {
            DB db;
            loadDB(db);
            CHECK_TRUE(db.getCsvSettings().separator == value);
            closeDB(db);
        }
    }
}


int main(int, const char*[])
{
    testGeneralSettings();
    testCsvSettings();

    return 0;
}
