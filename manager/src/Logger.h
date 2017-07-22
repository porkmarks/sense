#pragma once

#include "Settings.h"
#include "DB.h"

class Logger
{
public:
    Logger();
    ~Logger();

    typedef std::chrono::high_resolution_clock Clock;

    bool create(std::string const& name);
    bool load(std::string const& name);

    enum class Type
    {
        VERBOSE,
        INFO,
        WARNING,
        ERROR
    };

    void logVerbose(std::string const& message);
    void logInfo(std::string const& message);
    void logWarning(std::string const& message);
    void logError(std::string const& message);

private:

    struct LogLine
    {
        Clock::time_point timePoint;
        Type type;
        std::string message;
    };

    struct Data
    {
        std::vector<LogLine> logLines;
    };

    Data m_mainData;

    std::atomic_bool m_threadsExit = { false };

    void triggerSave();
    void storeThreadProc();
    void save(Data const& data) const;
    std::atomic_bool m_storeThreadTriggered = { false };
    std::thread m_storeThread;
    std::condition_variable m_storeCV;
    std::mutex m_storeMutex;
    Data m_storeData;

    std::string m_dataName;

    mutable Clock::time_point m_lastDailyBackupTP = Clock::time_point(Clock::duration::zero());
    mutable Clock::time_point m_lastWeeklyBackupTP = Clock::time_point(Clock::duration::zero());
};

