#include "testUtils.h"
#include "Logger.h"
#include "sqlite3.h"
#include <iostream>

Logger s_logger;

void closeDB(DB& db)
{
	db.process(); //processing to trigger delayed saves
	sqlite3* sqlite = db.getSqliteDB();
	db.close();
	s_logger.close();
	CHECK_EQUALS(sqlite3_close_v2(sqlite), SQLITE_OK);
}
void createDB(DB& db)
{
	std::string filename = "test.db";
	remove(filename.c_str());
	sqlite3* sqlite;
	CHECK_EQUALS(sqlite3_open_v2(filename.c_str(), &sqlite, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr), SQLITE_OK);
	s_logger.setStdOutput(Logger::Type::WARNING);
	CHECK_SUCCESS(s_logger.create(*sqlite));
	CHECK_TRUE(s_logger.load(*sqlite));
	CHECK_SUCCESS(db.create(*sqlite));
	CHECK_SUCCESS(db.load(*sqlite));
}
void loadDB(DB& db)
{
	std::string filename = "test.db";
	sqlite3* sqlite;
	CHECK_EQUALS(sqlite3_open_v2(filename.c_str(), &sqlite, SQLITE_OPEN_READWRITE, nullptr), SQLITE_OK);
	s_logger.setStdOutput(Logger::Type::WARNING);
	CHECK_TRUE(s_logger.load(*sqlite));
	CHECK_SUCCESS(db.load(*sqlite));
}
