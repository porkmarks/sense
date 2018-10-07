#include "Logger.h"
#include <algorithm>
#include <functional>
#include <cassert>
#include <fstream>
#include <sstream>
#include <iostream>
#include <QDateTime>
#include <QTimer>

#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/writer.h"
#include "rapidjson/reader.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/ostreamwrapper.h"
#include "rapidjson/istreamwrapper.h"

#include "CRC.h"
#include "Utils.h"
#include "Crypt.h"


extern std::string s_dataFolder;

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
    save(m_mainData);

    return true;
}

//////////////////////////////////////////////////////////////////////////

bool Logger::load(std::string const& name)
{
    Clock::time_point start = Clock::now();

    m_dataName = "sense-" + name + ".log";

    m_dataFolder = s_dataFolder;
    std::string dataFilename = (m_dataFolder + "/" + m_dataName);

    Data data;

    {
        std::string streamData;
        {
            std::ifstream file(dataFilename, std::ios_base::binary);
            if (!file.is_open())
            {
                std::cerr << "Failed to open " << dataFilename << " file: " << std::strerror(errno) << "\n";
                return false;
            }

            streamData = std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            file.close();
        }

        Crypt crypt;
        crypt.setKey(k_fileEncryptionKey);
        QByteArray decryptedData = crypt.decryptToByteArray(QByteArray(streamData.data(), streamData.size()));

        std::stringstream stream(std::string(decryptedData.data(), decryptedData.size()));

        uint64_t logsSize = 0;
        if (!read(stream, logsSize))
        {
            stream = std::stringstream(std::string(streamData.data(), streamData.size())); //try unencrypted
            if (!read(stream, logsSize))
            {
                std::cerr << "Failed to open " << dataFilename << " file: corrupted file (canot read log size).\n";
                return false;
            }
        }
        if (logsSize > 1024 * 1024 * 1024)
        {
            std::cerr << "Failed to open " << dataFilename << " file: corrupted file (logs too big: " << logsSize << " bytes).\n";
            return false;
        }

        data.logs.resize(logsSize);
        if (stream.read(reinterpret_cast<char*>(&data.logs[0]), logsSize).bad())
        {
            std::cerr << "Failed to open " << dataFilename << " file. Failed to read logs: " << std::strerror(errno) << ".\n";
            return false;
        }

        uint64_t lineCount = 0;
        if (!read(stream, lineCount))
        {
            std::cerr << "Failed to open " << dataFilename << " file: corrupted file (canot read line count).\n";
            return false;
        }

        if (lineCount > 1024 * 1024 * 1024)
        {
            std::cerr << "Failed to open " << dataFilename << " file: corrupted file (too many lines: " << lineCount << ").\n";
            return false;
        }

        size_t lineSize = sizeof(StoredLogLine);
        //std::vector<StoredLogLineOld> storedLogLinesOld;
        //storedLogLinesOld.resize(lineCount);

        data.storedLogLines.resize(lineCount);
        if (stream.read(reinterpret_cast<char*>(data.storedLogLines.data()), data.storedLogLines.size() * lineSize).bad())
        {
            std::cerr << "Failed to open " << dataFilename << " file. Failed to read log lines: " << std::strerror(errno) << ".\n";
            return false;
        }

//        for (size_t i = 0; i < lineCount; i++)
//        {
//            StoredLogLine& sll = data.storedLogLines[i];
//            StoredLogLineOld& sllo = storedLogLinesOld[i];
//            sll.timePoint = sllo.timePoint;
//            sll.index = sllo.index;
//            sll.type = sllo.type;
//            sll.messageOffset = sllo.messageOffset;
//            sll.messageSize = sllo.messageSize;
//        }

        //refresh the last log line index
        std::string msg = "Added measurement index ";
        for (StoredLogLine& storedLine: data.storedLogLines)
        {
            if (storedLine.messageSize >= msg.size() && data.logs.compare(storedLine.messageOffset, msg.size(), msg) == 0)
            {
                storedLine.type = Type::VERBOSE;
            }
            data.lastLineIndex = std::max(data.lastLineIndex, storedLine.index);
        }
    }

    //initialize backups
    std::pair<std::string, time_t> bkf = getMostRecentBackup(dataFilename, m_dataFolder + "/backups/daily");
    if (bkf.first.empty())
    {
        m_lastDailyBackupTP = Clock::now();
    }
    else
    {
        m_lastDailyBackupTP = Clock::from_time_t(bkf.second);
    }
    bkf = getMostRecentBackup(dataFilename, m_dataFolder + "/backups/weekly");
    if (bkf.first.empty())
    {
        m_lastWeeklyBackupTP = Clock::now();
    }
    else
    {
        m_lastWeeklyBackupTP = Clock::from_time_t(bkf.second);
    }

    //done!!!

    {
        std::lock_guard<std::mutex> lg(m_mainDataMutex);
        m_mainData = std::move(data);
    }

    std::cout << "Time to load logs:" << std::chrono::duration<float>(Clock::now() - start).count() << "s\n";
    std::cout.flush();

    emit logLinesAdded();

    return true;
}

//////////////////////////////////////////////////////////////////////////

void Logger::logVerbose(std::string const& message)
{
    {
        std::lock_guard<std::mutex> lg(m_mainDataMutex);
        StoredLogLine storedLine {getTimePointAsMilliseconds(Clock::now()), ++m_mainData.lastLineIndex, Type::VERBOSE, 0, 0};
        storedLine.messageOffset = m_mainData.logs.size();
        storedLine.messageSize = message.size();
        m_mainData.logs.append(message);
        m_mainData.storedLogLines.emplace_back(storedLine);
    }

    std::cout << "VERBOSE: " << QDateTime::currentDateTime().toString("dd-MM-yyyy HH:mm:ss.zzz").toUtf8().data() << ": " << message << "\n";
    std::cout.flush();

    emit logLinesAdded();

    triggerDelayedSave(std::chrono::minutes(10));
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
        StoredLogLine storedLine {getTimePointAsMilliseconds(Clock::now()), ++m_mainData.lastLineIndex, Type::INFO, 0, 0};
        storedLine.messageOffset = m_mainData.logs.size();
        storedLine.messageSize = message.size();
        m_mainData.logs.append(message);
        m_mainData.storedLogLines.emplace_back(storedLine);
    }

    std::cout << "INFO: " << QDateTime::currentDateTime().toString("dd-MM-yyyy HH:mm:ss.zzz").toUtf8().data() << ": " << message << "\n";
    std::cout.flush();

    emit logLinesAdded();

    triggerDelayedSave(std::chrono::minutes(1));
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
        StoredLogLine storedLine {getTimePointAsMilliseconds(Clock::now()), ++m_mainData.lastLineIndex, Type::WARNING, 0, 0};
        storedLine.messageOffset = m_mainData.logs.size();
        storedLine.messageSize = message.size();
        m_mainData.logs.append(message);
        m_mainData.storedLogLines.emplace_back(storedLine);
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
        StoredLogLine storedLine {getTimePointAsMilliseconds(Clock::now()), ++m_mainData.lastLineIndex, Type::CRITICAL, 0, 0};
        storedLine.messageOffset = m_mainData.logs.size();
        storedLine.messageSize = message.size();
        m_mainData.logs.append(message);
        m_mainData.storedLogLines.emplace_back(storedLine);
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
    std::vector<LogLine> result;
    result.reserve(16384);

    std::lock_guard<std::mutex> lg(m_mainDataMutex);

    uint64_t minTimePoint = getTimePointAsMilliseconds(filter.minTimePoint);
    uint64_t maxTimePoint = getTimePointAsMilliseconds(filter.maxTimePoint);

    for (StoredLogLine const& storedLine: m_mainData.storedLogLines)
    {
        if (storedLine.timePoint < minTimePoint || storedLine.timePoint > maxTimePoint)
        {
            continue;
        }
        if ((!filter.allowVerbose && storedLine.type == Type::VERBOSE) ||
            (!filter.allowInfo && storedLine.type == Type::INFO) ||
            (!filter.allowWarning && storedLine.type == Type::WARNING) ||
            (!filter.allowError && storedLine.type == Type::CRITICAL))
        {
            continue;
        }

        LogLine line;
        line.index = storedLine.index;
        line.timePoint = getTimePointFromMilliseconds(storedLine.timePoint);
        line.type = storedLine.type;
        assert(storedLine.messageOffset + storedLine.messageSize <= m_mainData.logs.size());
        line.message = m_mainData.logs.substr(storedLine.messageOffset, storedLine.messageSize);
        result.push_back(line);
    }

    return result;
}

//////////////////////////////////////////////////////////////////////////

size_t Logger::getAllLogLineCount() const
{
    std::lock_guard<std::mutex> lg(m_mainDataMutex);
    return m_mainData.storedLogLines.size();
}

//////////////////////////////////////////////////////////////////////////

void Logger::triggerSave()
{
    {
        std::lock_guard<std::mutex> lg(m_delayedTriggerSaveMutex);
        m_delayedTriggerSave = false;
    }

    Data mainDataCopy;
    {
        std::lock_guard<std::mutex> lg(m_mainDataMutex);
        mainDataCopy = m_mainData;
    }


    {
        std::unique_lock<std::mutex> lg(m_storeMutex);
        m_storeData = std::move(mainDataCopy);
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

            //write the log lines
            write(stream, static_cast<uint64_t>(data.storedLogLines.size()));
            stream.write(reinterpret_cast<const char*>(data.storedLogLines.data()), data.storedLogLines.size() * sizeof(StoredLogLine));

            buffer = stream.str();
        }

        Crypt crypt;
        crypt.setKey(k_fileEncryptionKey);
        crypt.setCompressionLevel(1);
        QByteArray encryptedData = crypt.encryptToByteArray(QByteArray(buffer.data(), buffer.size()));
//        QByteArray encryptedData = QByteArray(buffer.GetString(), buffer.GetSize());

        std::string tempFilename = (m_dataFolder + "/" + m_dataName + "_temp");
        {
            std::ofstream file(tempFilename, std::ios_base::binary);
            if (!file.is_open())
            {
                std::cerr << "Failed to open " << tempFilename << " file: " << std::strerror(errno) << "\n";
            }
            else
            {
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
        {
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
        }

        save(data);
    }
}
