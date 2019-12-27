#include "Logger.h"
#include <algorithm>
#include <functional>
#include <cassert>
#include <fstream>
#include <sstream>
#include <iostream>
#include <QDateTime>
#include <QTimer>

#include "cereal/archives/json.hpp"

#include "CRC.h"
#include "Utils.h"
#include "Crypt.h"


extern std::string s_dataFolder;

struct Header
{
    constexpr static uint64_t k_magic = 2112412454;
    uint32_t magic = 0;

    constexpr static uint32_t k_version = 2;
    uint32_t version = 0;
};

constexpr uint64_t k_fileEncryptionKey = 3452354435332256ULL;


template <typename Stream, typename T>
void write(Stream& s, T const& t)
{
    s.write(reinterpret_cast<const char*>(&t), sizeof(T));
}

template <typename Stream, typename T>
bool read(Stream& s, T& t)
{
    return s.read(reinterpret_cast<char*>(&t), sizeof(T)).good();
}

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
    m_storeThread = std::thread(std::bind(&Logger::storeThreadProc, this));
}

//////////////////////////////////////////////////////////////////////////

Logger::~Logger()
{
    shutdown();
}

//////////////////////////////////////////////////////////////////////////

void Logger::shutdown()
{
    if (m_threadsExit)
    {
        return;
    }

    if (m_delayedTriggerSave)
    {
        std::cout << "A logger save was scheduled. Performing it now...\n";
        triggerSave();
        while (m_storeThreadTriggered) //wait for the thread to finish saving
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        std::cout << "Finished scheduled save\n";
    }

    m_threadsExit = true;

    m_storeCV.notify_all();
    if (m_storeThread.joinable())
    {
        m_storeThread.join();
    }
}

//////////////////////////////////////////////////////////////////////////

void Logger::process()
{
    bool trigger = false;
    {
        std::lock_guard<std::mutex> lg(m_delayedTriggerSaveMutex);
        if (m_delayedTriggerSave)
        {
            trigger = Clock::now() >= m_delayedTriggerSaveTP;
        }
    }

    if (trigger)
    {
        triggerSave();
    }
}

//////////////////////////////////////////////////////////////////////////

bool Logger::create(std::string const& name)
{
    m_dataName = "sense-" + name + ".log";

    m_dataFolder = s_dataFolder;
    std::string dataFilename = m_dataFolder + "/" + m_dataName;

    moveToBackup(m_dataName, dataFilename, m_dataFolder + "/backups/deleted", 50);
    remove((dataFilename).c_str());

    m_mainData = Data();
    save(Data());

    return true;
}

//////////////////////////////////////////////////////////////////////////

bool Logger::load(std::string const& name)
{
    Clock::time_point start = Clock::now();

    m_dataName = "sense-" + name + ".log";

    m_dataFolder = s_dataFolder;
    std::string filename = (m_dataFolder + "/" + m_dataName);

    Data data;
    bool res = load(data, filename);
    if (!res)
    {
        return false;
    }

    //initialize backups
    std::pair<std::string, time_t> bkf = getMostRecentBackup(filename, m_dataFolder + "/backups/daily");
    if (bkf.first.empty())
    {
        m_lastDailyBackupTP = Clock::now();
    }
    else
    {
        m_lastDailyBackupTP = Clock::from_time_t(bkf.second);
    }
    bkf = getMostRecentBackup(filename, m_dataFolder + "/backups/weekly");
    if (bkf.first.empty())
    {
        m_lastWeeklyBackupTP = Clock::now();
    }
    else
    {
        m_lastWeeklyBackupTP = Clock::from_time_t(bkf.second);
    }

    //done!!!
    //refresh the last log line index
    uint32_t lastLineIndex = 0;
    for (StoredLogLine& storedLine: data.storedVerboseLogLines)
    {
        lastLineIndex = std::max(lastLineIndex, storedLine.index);
    }
    for (StoredLogLine& storedLine: data.storedInfoLogLines)
    {
        lastLineIndex = std::max(lastLineIndex, storedLine.index);
    }
    for (StoredLogLine& storedLine: data.storedWarningLogLines)
    {
        lastLineIndex = std::max(lastLineIndex, storedLine.index);
    }
    for (StoredLogLine& storedLine: data.storedCriticalLogLines)
    {
        lastLineIndex = std::max(lastLineIndex, storedLine.index);
    }

    {
        std::lock_guard<std::mutex> lg(m_mainDataMutex);
        m_mainDataLastLineIndex = lastLineIndex;
        m_mainData = std::move(data);
    }

    std::cout << "Time to load logs:" << std::chrono::duration<float>(Clock::now() - start).count() << "s\n";
    std::cout.flush();

    emit logLinesAdded();

    return true;
}

//////////////////////////////////////////////////////////////////////////

bool Logger::load(Data& data, std::string const& filename) const
{
    Clock::time_point start = Clock::now();

    std::ifstream file(filename, std::ios_base::binary);
    if (!file.is_open())
    {
        std::cerr << "Failed to open " << filename << " file: " << std::strerror(errno) << "\n";
        return false;
    }

    Header header;
    if (file.read(reinterpret_cast<char*>(&header), sizeof(header)).bad())
    {
        std::cerr << "Failed to open " << filename << " file: corrupted file (cannot read magic).\n";
        return false;
    }
    if (header.magic != Header::k_magic)
    {
        std::cerr << "Failed to open " << filename << " file: corrupted file (bad magic).\n";
        return false;
    }
    if (header.version != Header::k_version)
    {
        std::cerr << "Failed to open " << filename << " file: corrupted file (bad version).\n";
        return false;
    }

    bool res = loadV2(data, filename, file);
    if (!res)
    {
        return false;
    }

    std::cout << "Time to load logs:" << std::chrono::duration<float>(Clock::now() - start).count() << "s\n";
    std::cout.flush();

    return true;
}

//////////////////////////////////////////////////////////////////////////

bool Logger::loadV2(Data& data, std::string const& filename, std::ifstream& file) const
{
    std::string streamData = std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    Crypt crypt;
    crypt.setKey(k_fileEncryptionKey);
    QByteArray decryptedData = crypt.decryptToByteArray(QByteArray(streamData.data(), (int)streamData.size()));

    std::stringstream stream(std::string(decryptedData.data(), decryptedData.size()));

    uint64_t logsSize = 0;
    if (!read(stream, logsSize))
    {
        stream = std::stringstream(std::string(streamData.data(), streamData.size())); //try unencrypted
        if (!read(stream, logsSize))
        {
            std::cerr << "Failed to open " << filename << " file: corrupted file (canot read log size).\n";
            return false;
        }
    }
    if (logsSize > 1024 * 1024 * 1024)
    {
        std::cerr << "Failed to open " << filename << " file: corrupted file (logs too big: " << logsSize << " bytes).\n";
        return false;
    }

    data.logs.resize(logsSize);
    if (stream.read(reinterpret_cast<char*>(&data.logs[0]), logsSize).bad())
    {
        std::cerr << "Failed to open " << filename << " file. Failed to read logs: " << std::strerror(errno) << ".\n";
        return false;
    }

    size_t lineSize = sizeof(StoredLogLine);

    //VERBOSE
    uint64_t lineCount = 0;
    if (!read(stream, lineCount))
    {
        std::cerr << "Failed to open " << filename << " file: corrupted file (canot read line count).\n";
        return false;
    }
    if (lineCount > 1024 * 1024 * 1024)
    {
        std::cerr << "Failed to open " << filename << " file: corrupted file (too many lines: " << lineCount << ").\n";
        return false;
    }
    data.storedVerboseLogLines.resize(lineCount);
    if (stream.read(reinterpret_cast<char*>(data.storedVerboseLogLines.data()), data.storedVerboseLogLines.size() * lineSize).bad())
    {
        std::cerr << "Failed to open " << filename << " file. Failed to read log lines: " << std::strerror(errno) << ".\n";
        return false;
    }

    //INFO
    if (!read(stream, lineCount))
    {
        std::cerr << "Failed to open " << filename << " file: corrupted file (canot read line count).\n";
        return false;
    }
    if (lineCount > 1024 * 1024 * 1024)
    {
        std::cerr << "Failed to open " << filename << " file: corrupted file (too many lines: " << lineCount << ").\n";
        return false;
    }
    data.storedInfoLogLines.resize(lineCount);
    if (stream.read(reinterpret_cast<char*>(data.storedInfoLogLines.data()), data.storedInfoLogLines.size() * lineSize).bad())
    {
        std::cerr << "Failed to open " << filename << " file. Failed to read log lines: " << std::strerror(errno) << ".\n";
        return false;
    }

    //WARNING
    if (!read(stream, lineCount))
    {
        std::cerr << "Failed to open " << filename << " file: corrupted file (canot read line count).\n";
        return false;
    }
    if (lineCount > 1024 * 1024 * 1024)
    {
        std::cerr << "Failed to open " << filename << " file: corrupted file (too many lines: " << lineCount << ").\n";
        return false;
    }
    data.storedWarningLogLines.resize(lineCount);
    if (stream.read(reinterpret_cast<char*>(data.storedWarningLogLines.data()), data.storedWarningLogLines.size() * lineSize).bad())
    {
        std::cerr << "Failed to open " << filename << " file. Failed to read log lines: " << std::strerror(errno) << ".\n";
        return false;
    }

    //CRITICAL
    if (!read(stream, lineCount))
    {
        std::cerr << "Failed to open " << filename << " file: corrupted file (canot read line count).\n";
        return false;
    }
    if (lineCount > 1024 * 1024 * 1024)
    {
        std::cerr << "Failed to open " << filename << " file: corrupted file (too many lines: " << lineCount << ").\n";
        return false;
    }
    data.storedCriticalLogLines.resize(lineCount);
    if (stream.read(reinterpret_cast<char*>(data.storedCriticalLogLines.data()), data.storedCriticalLogLines.size() * lineSize).bad())
    {
        std::cerr << "Failed to open " << filename << " file. Failed to read log lines: " << std::strerror(errno) << ".\n";
        return false;
    }

    return true;
}
//////////////////////////////////////////////////////////////////////////

//bool Logger::loadAndMerge(Data& data, Data const& delta) const
//{
//    std::string filename = (m_dataFolder + "/" + m_dataName);
//    if (!load(data, filename))
//    {
//        return false;
//    }

//    uint32_t offset = static_cast<uint32_t>(data.logs.size());
//    data.logs.append(delta.logs);
//    for (StoredLogLine l: delta.storedLogLines)
//    {
//        l.messageOffset += offset;
//        data.storedLogLines.push_back(l);
//    }
//    return true;
//}

//////////////////////////////////////////////////////////////////////////

void Logger::logVerbose(std::string const& message)
{
    {
        std::lock_guard<std::mutex> lg(m_mainDataMutex);
        StoredLogLine storedLine {getTimePointAsMilliseconds(Clock::now()), ++m_mainDataLastLineIndex, 0, 0};
        storedLine.messageOffset = static_cast<uint32_t>(m_mainData.logs.size());
        storedLine.messageSize = static_cast<uint32_t>(message.size());
        m_mainData.logs.append(message);
        m_mainData.storedVerboseLogLines.emplace_back(storedLine);
    }

    std::cout << "VERBOSE: " << QDateTime::currentDateTime().toString("dd-MM-yyyy HH:mm:ss.zzz").toUtf8().data() << ": " << message << "\n";
    std::cout.flush();

    emit logLinesAdded();

#ifdef NDEBUG
    triggerDelayedSave(std::chrono::minutes(10));
#else
    triggerDelayedSave(std::chrono::seconds(10));
#endif
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
    {
        std::lock_guard<std::mutex> lg(m_mainDataMutex);
        StoredLogLine storedLine {getTimePointAsMilliseconds(Clock::now()), ++m_mainDataLastLineIndex, 0, 0};
        storedLine.messageOffset = static_cast<uint32_t>(m_mainData.logs.size());
        storedLine.messageSize = static_cast<uint32_t>(message.size());
        m_mainData.logs.append(message);
        m_mainData.storedInfoLogLines.emplace_back(storedLine);
    }

    std::cout << "INFO: " << QDateTime::currentDateTime().toString("dd-MM-yyyy HH:mm:ss.zzz").toUtf8().data() << ": " << message << "\n";
    std::cout.flush();

    emit logLinesAdded();

#ifdef NDEBUG
    triggerDelayedSave(std::chrono::minutes(1));
#else
    triggerDelayedSave(std::chrono::seconds(10));
#endif
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
    {
        std::lock_guard<std::mutex> lg(m_mainDataMutex);
        StoredLogLine storedLine {getTimePointAsMilliseconds(Clock::now()), ++m_mainDataLastLineIndex, 0, 0};
        storedLine.messageOffset = static_cast<uint32_t>(m_mainData.logs.size());
        storedLine.messageSize = static_cast<uint32_t>(message.size());
        m_mainData.logs.append(message);
        m_mainData.storedWarningLogLines.emplace_back(storedLine);
    }

    std::cout << "WARNING: " << QDateTime::currentDateTime().toString("dd-MM-yyyy HH:mm:ss.zzz").toUtf8().data() << ": " << message << "\n";
    std::cout.flush();

    emit logLinesAdded();
    triggerSave();
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
    {
        std::lock_guard<std::mutex> lg(m_mainDataMutex);
        StoredLogLine storedLine {getTimePointAsMilliseconds(Clock::now()), ++m_mainDataLastLineIndex, 0, 0};
        storedLine.messageOffset = static_cast<uint32_t>(m_mainData.logs.size());
        storedLine.messageSize = static_cast<uint32_t>(message.size());
        m_mainData.logs.append(message);
        m_mainData.storedCriticalLogLines.emplace_back(storedLine);
    }

    std::cout << "ERROR: " << QDateTime::currentDateTime().toString("dd-MM-yyyy HH:mm:ss.zzz").toUtf8().data() << ": " << message << "\n";
    std::cout.flush();

    emit logLinesAdded();
    triggerSave();
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

std::vector<Logger::LogLine> Logger::getFilteredLogLines(Filter const& filter) const
{
    std::lock_guard<std::mutex> lg(m_mainDataMutex);
    Data const& data = m_mainData;

    std::vector<LogLine> result;
    result.reserve(16384);

    uint64_t minTimePoint = getTimePointAsMilliseconds(filter.minTimePoint);
    uint64_t maxTimePoint = getTimePointAsMilliseconds(filter.maxTimePoint);

    if (filter.allowVerbose)
    {
        for (StoredLogLine const& storedLine: data.storedVerboseLogLines)
        {
            if (storedLine.timePoint < minTimePoint || storedLine.timePoint > maxTimePoint)
            {
                continue;
            }
            LogLine line;
            line.index = storedLine.index;
            line.timePoint = getTimePointFromMilliseconds(storedLine.timePoint);
            line.type = Type::VERBOSE;
            assert(storedLine.messageOffset + storedLine.messageSize <= data.logs.size());
            line.message = data.logs.substr(storedLine.messageOffset, storedLine.messageSize);
            result.push_back(std::move(line));
        }
    }
    if (filter.allowInfo)
    {
        for (StoredLogLine const& storedLine: data.storedInfoLogLines)
        {
            if (storedLine.timePoint < minTimePoint || storedLine.timePoint > maxTimePoint)
            {
                continue;
            }
            LogLine line;
            line.index = storedLine.index;
            line.timePoint = getTimePointFromMilliseconds(storedLine.timePoint);
            line.type = Type::INFO;
            assert(storedLine.messageOffset + storedLine.messageSize <= data.logs.size());
            line.message = data.logs.substr(storedLine.messageOffset, storedLine.messageSize);
            result.push_back(std::move(line));
        }
    }
    if (filter.allowWarning)
    {
        for (StoredLogLine const& storedLine: data.storedWarningLogLines)
        {
            if (storedLine.timePoint < minTimePoint || storedLine.timePoint > maxTimePoint)
            {
                continue;
            }
            LogLine line;
            line.index = storedLine.index;
            line.timePoint = getTimePointFromMilliseconds(storedLine.timePoint);
            line.type = Type::WARNING;
            assert(storedLine.messageOffset + storedLine.messageSize <= data.logs.size());
            line.message = data.logs.substr(storedLine.messageOffset, storedLine.messageSize);
            result.push_back(std::move(line));
        }
    }
    if (filter.allowCritical)
    {
        for (StoredLogLine const& storedLine: data.storedCriticalLogLines)
        {
            if (storedLine.timePoint < minTimePoint || storedLine.timePoint > maxTimePoint)
            {
                continue;
            }
            LogLine line;
            line.index = storedLine.index;
            line.timePoint = getTimePointFromMilliseconds(storedLine.timePoint);
            line.type = Type::CRITICAL;
            assert(storedLine.messageOffset + storedLine.messageSize <= data.logs.size());
            line.message = data.logs.substr(storedLine.messageOffset, storedLine.messageSize);
            result.push_back(std::move(line));
        }
    }

    return result;
}

//////////////////////////////////////////////////////////////////////////

void Logger::triggerSave()
{
    {
        std::lock_guard<std::mutex> lg(m_delayedTriggerSaveMutex);
        m_delayedTriggerSave = false;
    }

    Data delta;
    {
        std::lock_guard<std::mutex> lg(m_mainDataMutex);
        delta = m_mainData;
    }


    {
        std::unique_lock<std::mutex> lg(m_storeMutex);
        m_storeData = std::move(delta);
        m_storeThreadTriggered = true;
    }
    m_storeCV.notify_all();
}

//////////////////////////////////////////////////////////////////////////

void Logger::triggerDelayedSave(Clock::duration i_dt)
{
    if (i_dt < std::chrono::milliseconds(100))
    {
        triggerSave();
        return;
    }


    std::lock_guard<std::mutex> lg(m_delayedTriggerSaveMutex);
    if (m_delayedTriggerSave && m_delayedTriggerSaveTP < Clock::now() + i_dt)
    {
        //if the previous schedule is sooner than the new one, leave the previous intact
        return;
    }

    m_delayedTriggerSaveTP = Clock::now() + i_dt;
    m_delayedTriggerSave = true;
}

//////////////////////////////////////////////////////////////////////////

void Logger::save(Data const& data) const
{
    bool dailyBackup = false;
    bool weeklyBackup = false;
    Clock::time_point now = Clock::now();
    if (now - m_lastDailyBackupTP >= std::chrono::hours(24))
    {
        m_lastDailyBackupTP = now;
        dailyBackup = true;
    }
    if (now - m_lastWeeklyBackupTP >= std::chrono::hours(24 * 7))
    {
        m_lastWeeklyBackupTP = now;
        weeklyBackup = true;
    }

    std::string dataFilename = (m_dataFolder + "/" + m_dataName);

    Clock::time_point start = now;

    {
        std::string buffer;
        {
            std::stringstream stream;
            //write the log text
            write(stream, static_cast<uint64_t>(data.logs.size()));
            stream.write(reinterpret_cast<const char*>(data.logs.data()), data.logs.size());

            //write the VERBOSE log lines
            write(stream, static_cast<uint64_t>(data.storedVerboseLogLines.size()));
            stream.write(reinterpret_cast<const char*>(data.storedVerboseLogLines.data()), data.storedVerboseLogLines.size() * sizeof(StoredLogLine));

            //write the INFO log lines
            write(stream, static_cast<uint64_t>(data.storedInfoLogLines.size()));
            stream.write(reinterpret_cast<const char*>(data.storedInfoLogLines.data()), data.storedInfoLogLines.size() * sizeof(StoredLogLine));

            //write the WARNING log lines
            write(stream, static_cast<uint64_t>(data.storedWarningLogLines.size()));
            stream.write(reinterpret_cast<const char*>(data.storedWarningLogLines.data()), data.storedWarningLogLines.size() * sizeof(StoredLogLine));

            //write the CRITICAL log lines
            write(stream, static_cast<uint64_t>(data.storedCriticalLogLines.size()));
            stream.write(reinterpret_cast<const char*>(data.storedCriticalLogLines.data()), data.storedCriticalLogLines.size() * sizeof(StoredLogLine));

            buffer = stream.str();
        }

        Crypt crypt;
        crypt.setKey(k_fileEncryptionKey);
        crypt.setCompressionLevel(1);
        QByteArray encryptedData = crypt.encryptToByteArray(QByteArray(buffer.data(), (int)buffer.size()));
//        QByteArray encryptedData = QByteArray(buffer.data(), buffer.size());

        std::string tempFilename = (m_dataFolder + "/" + m_dataName + "_temp");
        {
            std::ofstream file(tempFilename, std::ios_base::binary);
            if (!file.is_open())
            {
                std::cerr << "Failed to open " << tempFilename << " file: " << std::strerror(errno) << "\n";
            }
            else
            {
                Header header;
                header.magic = Header::k_magic;
                header.version = Header::k_version;
                file.write(reinterpret_cast<char const*>(&header), sizeof(header));
                file.write(encryptedData.data(), encryptedData.size());
            }
            file.flush();
            file.close();
        }

        if (!renameFile(tempFilename.c_str(), dataFilename.c_str()))
        {
            std::cerr << "Error renaming files: " << getLastErrorAsString() << "\n";
        }
    }

    std::cout << "Time to save logs:" << std::chrono::duration<float>(Clock::now() - start).count() << "s\n";
    std::cout.flush();

    if (dailyBackup)
    {
        copyToBackup(m_dataName, dataFilename, m_dataFolder + "/backups/daily", 10);
    }
    if (weeklyBackup)
    {
        copyToBackup(m_dataName, dataFilename, m_dataFolder + "/backups/weekly", 10);
    }
}

//////////////////////////////////////////////////////////////////////////

void Logger::storeThreadProc()
{
    while (!m_threadsExit)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        Data data;

        while (true)
        {
            data = Data(); //clear

            //wait for data
            std::unique_lock<std::mutex> lg(m_storeMutex);
            if (!m_storeThreadTriggered)
            {
                m_storeCV.wait(lg, [this]{ return m_storeThreadTriggered || m_threadsExit; });
            }
            if (m_threadsExit)
            {
                break;
            }

            data = std::move(m_storeData);
            m_storeData = Data();
            m_storeThreadTriggered = false;
            break;
        }
        if (m_threadsExit)
        {
            break;
        }
        save(data);
    }
}
