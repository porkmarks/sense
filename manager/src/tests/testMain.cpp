#include "stdio.h"
#include "Logger.h"
#include <iostream>
#include "DB.h"
#include "sqlite3.h"
#include <set>

Logger s_logger;

#define CHECK_SUCCESS(x) \
{\
    auto __result = x;\
    if (__result != success)\
    {\
        std::cout << "Failed @" << __LINE__ << "/" << #x << ":" << __result.error().what() << std::endl;\
    }\
}
#define CHECK_FAILURE(x) \
{\
    auto __result = x;\
    if (__result == success)\
    {\
        std::cout << "Failed @" << __LINE__ << "/" << #x << ": Expected failure, got success" << std::endl;\
    }\
}
#define CHECK_TRUE(x) \
{\
    auto __result = x;\
    if (!__result)\
    {\
        std::cout << "Failed @" << __LINE__ << "/" << #x << ": Expected true, got false" << std::endl;\
    }\
}
#define CHECK_FALSE(x) \
{\
    auto __result = x;\
    if (__result)\
    {\
        std::cout << "Failed @" << __LINE__ << "/" << #x << ": Expected false, got true" << std::endl;\
    }\
}
#define CHECK_EQUALS(x, y) \
{\
    auto __x = x;\
    auto __y = y;\
    if (__x != __y)\
    {\
        std::cout << "Failed @" << __LINE__ << "/" << #x << " == " << #y << ": Expected " << std::to_string(__y) << ", got " << std::to_string(__x) << std::endl;\
    }\
}


void createDB(DB& db)
{
    std::string filename = "sense.db";
    remove(filename.c_str());
    sqlite3* sqlite;
    if (sqlite3_open_v2(filename.c_str(), &sqlite, SQLITE_OPEN_READWRITE, nullptr))
    {
        if (sqlite3_open_v2(filename.c_str(), &sqlite, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr))
        {
            exit(-1);
        }
    }
    s_logger.setStdOutput(false);
    CHECK_SUCCESS(s_logger.create(*sqlite));
    CHECK_TRUE(s_logger.load(*sqlite));
    CHECK_SUCCESS(db.create(*sqlite));
    CHECK_SUCCESS(db.load(*sqlite));
}
void loadDB(DB& db)
{
    std::string filename = "sense.db";
    sqlite3* sqlite;
    if (sqlite3_open_v2(filename.c_str(), &sqlite, SQLITE_OPEN_READWRITE, nullptr))
    {
        exit(-1);
    }
    s_logger.setStdOutput(false);
    CHECK_TRUE(s_logger.load(*sqlite));
    CHECK_SUCCESS(db.load(*sqlite));
}
void saveDB(DB& db)
{
    db.process(); //processing to trigger delayed saves
}


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
            saveDB(db);
        }
        {
            DB db;
            loadDB(db);
            CHECK_TRUE(db.getGeneralSettings().dateTimeFormat == format);
        }
    }
}

void testCsvSettings()
{
    std::cout << "Testing CSV Settings\n";

    {
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
                saveDB(db);
            }
            {
                DB db;
                loadDB(db);
                CHECK_TRUE(db.getCsvSettings().dateTimeFormatOverride == format);
            }
        }
    }
    {
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
                saveDB(db);
            }
            {
                DB db;
                loadDB(db);
                CHECK_TRUE(db.getCsvSettings().unitsFormat == format);
            }
        }
    }

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
            saveDB(db);                                         \
        }                                                       \
        {                                                       \
            DB db;                                              \
            loadDB(db);                                         \
            CHECK_TRUE(db.getCsvSettings().name == value);      \
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


    for (auto value: { 0u, 1u, 2u, 3u })
    {
        {
            DB db;
            createDB(db);
            DB::CsvSettings settings;
            settings.decimalPlaces = value;
            db.setCsvSettings(settings);
            CHECK_TRUE(db.getCsvSettings().decimalPlaces == value);
            saveDB(db);
        }
        {
            DB db;
            loadDB(db);
            CHECK_TRUE(db.getCsvSettings().decimalPlaces == value);
        }
    }
    for (auto value: {"/", ",", " ", "|", ""})
    {
        {
            DB db;
            createDB(db);
            DB::CsvSettings settings;
            settings.separator = value;
            db.setCsvSettings(settings);
            CHECK_TRUE(db.getCsvSettings().separator == value);
            saveDB(db);
        }
        {
            DB db;
            loadDB(db);
            CHECK_TRUE(db.getCsvSettings().separator == value);
        }
    }
}


int main(int, const char*[])
{
    std::cout << "Hello World\n";

    testGeneralSettings();
    testCsvSettings();

    return 0;
}
