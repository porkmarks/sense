#pragma once

#include <chrono>
#include <mutex>
#include <thread>
#include <vector>
#include <condition_variable>
#include <QString>
#include <QObject>

class Logger : public QObject
{
    Q_OBJECT
public:
    Logger();
    ~Logger();

    typedef std::chrono::system_clock Clock;

    void shutdown();

    bool create(std::string const& name);
    bool load(std::string const& name);

    enum class Type : uint8_t
    {
        VERBOSE,
        INFO,
        WARNING,
        CRITICAL
    };

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
        Type type;
        std::string message;
    };

    struct Filter
    {
        Clock::time_point minTimePoint;
        Clock::time_point maxTimePoint;
        bool allowVerbose = true;
        bool allowInfo = true;
        bool allowWarning = true;
        bool allowError = true;
    };

    std::vector<LogLine> getFilteredLogLines(Filter const& filter) const;
    size_t getAllLogLineCount() const;

signals:
    void logLinesAdded();

private:


#pragma pack(push, 1) // exact fit - no padding

    struct StoredLogLine
    {
        uint64_t timePoint; //milliseconds
        uint32_t index;
        Type type;
        uint32_t messageOffset;
        uint32_t messageSize;
    };

    struct StoredLogLineOld
    {
        uint64_t timePoint; //milliseconds
        uint64_t index;
        Type type;
        size_t messageOffset;
        size_t messageSize;
    };

#pragma pack(pop) // exact fit - no padding

    struct Data
    {
        std::string logs;
        std::vector<StoredLogLine> storedLogLines;
        uint32_t lastLineIndex = 0;
    };

    mutable std::mutex m_mainDataMutex;
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

    std::string m_dataFolder;
    std::string m_dataName;

    mutable Clock::time_point m_lastDailyBackupTP = Clock::time_point(Clock::duration::zero());
    mutable Clock::time_point m_lastWeeklyBackupTP = Clock::time_point(Clock::duration::zero());
};

