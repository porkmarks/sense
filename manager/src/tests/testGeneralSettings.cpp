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

