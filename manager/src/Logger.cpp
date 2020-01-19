#include "Logger.h"
#include <algorithm>
#include <functional>
#include <cassert>
#include <fstream>
#include <sstream>
#include <iostream>
#include <QDateTime>
#include <QTimer>

#include "Utils.h"
#include "Crypt.h"
#include "sqlite3.h"

extern std::string s_dataFolder;


//////////////////////////////////////////////////////////////////////////

static uint64_t getTimePointAsMilliseconds(Logger::Clock::time_point tp)
{
    time_t tt = Logger::Clock::to_time_t(tp);
    Logger::Clock::time_point tp2 = Logger::Clock::from_time_t(tt);

    Logger::Clock::duration subSecondRemainder = tp - tp2;

    uint64_t res = uint64_t(tt) * 1000;
    res += static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(subSecondRemainder).count());

    return res;
}

static Logger::Clock::time_point getTimePointFromMilliseconds(uint64_t ms)
{
    Logger::Clock::time_point tp = Logger::Clock::from_time_t(ms / 1000);
    Logger::Clock::duration subSecondRemainder = std::chrono::milliseconds(ms % 1000);
    return tp + subSecondRemainder;
}

//////////////////////////////////////////////////////////////////////////

Logger::Logger()
{
}

//////////////////////////////////////////////////////////////////////////

Logger::~Logger()
{
	m_insertStmt = nullptr;
}

//////////////////////////////////////////////////////////////////////////

Result<void> Logger::create(sqlite3& db)
{
	sqlite3_exec(&db, "BEGIN TRANSACTION;", NULL, NULL, NULL);
	utils::epilogue epi([&db] { sqlite3_exec(&db, "END TRANSACTION;", NULL, NULL, NULL); });

	{
		const char* sql = "CREATE TABLE Logs (id INTEGER PRIMARY KEY AUTOINCREMENT, timePoint DATETIME DEFAULT(STRFTIME('%Y-%m-%d %H:%M:%f', 'NOW')), type INTEGER, message STRING);";
		if (sqlite3_exec(&db, sql, NULL, NULL, nullptr))
		{
			Error error(QString("Error executing SQLite3 statement: %1").arg(sqlite3_errmsg(&db)).toUtf8().data());
			return error;
		}
	}

    return success;
}

//////////////////////////////////////////////////////////////////////////

bool Logger::load(sqlite3& db)
{
    m_sqlite = &db;

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_sqlite, "INSERT INTO Logs (type, message) VALUES(?1, ?2);", -1, &stmt, NULL) != SQLITE_OK)
    {
        Q_ASSERT(false);
        return false;
    }

    m_insertStmt.reset(stmt, &sqlite3_finalize);

    return true;
}

//////////////////////////////////////////////////////////////////////////

void Logger::setStdOutput(bool value)
{
    m_stdOutput = value;
}

//////////////////////////////////////////////////////////////////////////

void Logger::logVerbose(std::string const& message)
{
    std::lock_guard<std::recursive_mutex> lg(m_mutex);
    addToDB(message, Type::VERBOSE);

    if (m_stdOutput)
    {
        std::cout << "VERBOSE: " << QDateTime::currentDateTime().toString("dd-MM-yyyy HH:mm:ss.zzz").toUtf8().data() << ": " << message << "\n";
        std::cout.flush();
    }

    emit logLinesAdded();
}

//////////////////////////////////////////////////////////////////////////

void Logger::logVerbose(QString const& message)
{
    logVerbose(message.toUtf8().data());
}

//////////////////////////////////////////////////////////////////////////

void Logger::logVerbose(char const* message)
{
    logVerbose(std::string(message));
}

//////////////////////////////////////////////////////////////////////////

void Logger::logInfo(std::string const& message)
{
    std::lock_guard<std::recursive_mutex> lg(m_mutex);
    addToDB(message, Type::INFO);

    if (m_stdOutput)
    {
        std::cout << "INFO: " << QDateTime::currentDateTime().toString("dd-MM-yyyy HH:mm:ss.zzz").toUtf8().data() << ": " << message << "\n";
        std::cout.flush();
    }
}

//////////////////////////////////////////////////////////////////////////

void Logger::logInfo(QString const& message)
{
    logInfo(message.toUtf8().data());
}

//////////////////////////////////////////////////////////////////////////

void Logger::logInfo(char const* message)
{
    logInfo(std::string(message));
}

//////////////////////////////////////////////////////////////////////////

void Logger::logWarning(std::string const& message)
{
    std::lock_guard<std::recursive_mutex> lg(m_mutex);
    addToDB(message, Type::WARNING);

    if (m_stdOutput)
    {
        std::cout << "WARNING: " << QDateTime::currentDateTime().toString("dd-MM-yyyy HH:mm:ss.zzz").toUtf8().data() << ": " << message << "\n";
        std::cout.flush();
    }

    emit logLinesAdded();
}

//////////////////////////////////////////////////////////////////////////

void Logger::logWarning(QString const& message)
{
    logWarning(message.toUtf8().data());
}

//////////////////////////////////////////////////////////////////////////

void Logger::logWarning(char const* message)
{
    logWarning(std::string(message));
}

//////////////////////////////////////////////////////////////////////////

void Logger::logCritical(std::string const& message)
{
    std::lock_guard<std::recursive_mutex> lg(m_mutex);
    addToDB(message, Type::CRITICAL);

    if (m_stdOutput)
    {
        std::cout << "ERROR: " << QDateTime::currentDateTime().toString("dd-MM-yyyy HH:mm:ss.zzz").toUtf8().data() << ": " << message << "\n";
        std::cout.flush();
    }

    emit logLinesAdded();
}

//////////////////////////////////////////////////////////////////////////

void Logger::logCritical(QString const& message)
{
    logCritical(message.toUtf8().data());
}

//////////////////////////////////////////////////////////////////////////

void Logger::logCritical(char const* message)
{
    logCritical(std::string(message));
}

//////////////////////////////////////////////////////////////////////////

void Logger::addToDB(std::string const& message, Type type)
{
	sqlite3_bind_int(m_insertStmt.get(), 1, (int)type);
	sqlite3_bind_text(m_insertStmt.get(), 2, message.c_str(), -1, SQLITE_STATIC);
	if (sqlite3_step(m_insertStmt.get()) != SQLITE_DONE)
	{
        std::cerr << "Failed to add to db: " << sqlite3_errmsg(m_sqlite) << std::endl;
        Q_ASSERT(false);
	}
	sqlite3_reset(m_insertStmt.get());
}

//////////////////////////////////////////////////////////////////////////

std::vector<Logger::LogLine> Logger::getFilteredLogLines(Filter const& filter) const
{
    std::lock_guard<std::recursive_mutex> lg(m_mutex);

    std::vector<LogLine> result;
    result.reserve(16384);

	{
		//Logs (id INTEGER PRIMARY KEY AUTOINCREMENT, timePoint DATETIME DEFAULT(STRFTIME('%Y-%m-%d %H:%M:%f', 'NOW')), type INTEGER, message STRING)
		std::string sql = "SELECT id, datetime(timePoint, 'localtime'), type, message FROM Logs WHERE timePoint >= ?1 AND timePoint <= ?2";
        std::string typeFilter;
        if (filter.allowVerbose)
        {
            typeFilter += "type = 0";
        }
		if (filter.allowInfo)
		{
            if (!typeFilter.empty()) typeFilter += " OR";
            typeFilter += " type = 1";
		}
		if (filter.allowWarning)
		{
			if (!typeFilter.empty()) typeFilter += " OR";
			typeFilter += " type = 2";
		}
		if (filter.allowCritical)
		{
			if (!typeFilter.empty()) typeFilter += " OR";
			typeFilter += " type = 3";
		}
        if (!typeFilter.empty())
        {
            sql += " AND (";
            sql += typeFilter;
            sql += ")";
        }
        sql += ";";

		sqlite3_stmt* stmt;
		if (sqlite3_prepare_v2(m_sqlite, sql.c_str(), -1, &stmt, 0) != SQLITE_OK)
		{
            Q_ASSERT(false);
            return {};
		}
		utils::epilogue epi([stmt] { sqlite3_finalize(stmt); });

		char buffer[128];
        time_t timet = Clock::to_time_t(filter.minTimePoint);
		struct tm* timetm = gmtime(&timet);
		strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timetm);
		sqlite3_bind_text(stmt, 1, buffer, -1, SQLITE_TRANSIENT);
        timet = Clock::to_time_t(filter.maxTimePoint);
		timetm = gmtime(&timet);
		strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timetm);
		sqlite3_bind_text(stmt, 2, buffer, -1, SQLITE_TRANSIENT);

        const char* xx = sqlite3_expanded_sql(stmt);

		while (sqlite3_step(stmt) == SQLITE_ROW)
		{
			LogLine line;
			line.index = sqlite3_column_int64(stmt, 0);
			line.timePoint = Clock::from_time_t(QDateTime::fromString((const char*)sqlite3_column_text(stmt, 1), QString("yyyy-MM-dd HH:mm:ss")).toTime_t());
			line.type = (Type)sqlite3_column_int(stmt, 2);
			line.message = (char const*)sqlite3_column_text(stmt, 3);
            result.push_back(std::move(line));
		}
	}

    return std::move(result);
}

