#pragma once

#include <chrono>
#include <mutex>
#include <thread>
#include <vector>
#include <condition_variable>
#include <QString>
#include <QObject>
#include <QTimer>
#include "Result.h"

struct sqlite3;
struct sqlite3_stmt;

class Logger : public QObject
{
    Q_OBJECT
public:
    Logger();
    ~Logger();

    typedef std::chrono::system_clock Clock;

	static Result<void> create(sqlite3& db);
    bool load(sqlite3& db);
    void close();

    void clearAllLogs();

    enum class Type : uint8_t
    {
        VERBOSE = 0,
        INFO,
        WARNING,
        CRITICAL
    };

	void setStdOutput(Type minType);

    void logVerbose(QString const& message);
    void logVerbose(std::string const& message);
    void logVerbose(char const* message);

    void logInfo(QString const& message);
    void logInfo(std::string const& message);
    void logInfo(char const* message);

    void logWarning(QString const& message);
    void logWarning(std::string const& message);
    void logWarning(char const* message);

    void logCritical(QString const& message);
    void logCritical(std::string const& message);
    void logCritical(char const* message);

    struct LogLine
    {
        Clock::time_point timePoint;
        uint64_t index;
        Type type = Type::INFO;
        std::string message;
    };

    struct Filter
    {
        Clock::time_point minTimePoint = Clock::time_point(Clock::duration::zero());
        Clock::time_point maxTimePoint = Clock::time_point(Clock::duration(std::numeric_limits<Clock::duration::rep>::max()));
        bool allowVerbose = true;
        bool allowInfo = true;
        bool allowWarning = true;
        bool allowCritical = true;
    };

    std::vector<LogLine> getFilteredLogLines(Filter const& filter) const;

signals:
    void logLinesAdded(std::vector<LogLine> const& logLines);
    void logsChanged();

private:
    void addToDB(std::string const& message, Type type);

    Type m_stdOutputMinType = Type::VERBOSE;

	mutable std::recursive_mutex m_mutex;
    sqlite3* m_sqlite = nullptr;
	std::shared_ptr<sqlite3_stmt> m_insertStmt;
    std::vector<LogLine> m_logLinesForSignaling;
};

