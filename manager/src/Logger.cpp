#include "Logger.h"
#include <algorithm>
#include <functional>
#include <cassert>
#include <fstream>
#include <iostream>

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

Logger::Logger()
{
    m_storeThread = std::thread(std::bind(&Logger::storeThreadProc, this));
}

//////////////////////////////////////////////////////////////////////////

Logger::~Logger()
{
    m_threadsExit = true;

    m_storeCV.notify_all();
    if (m_storeThread.joinable())
    {
        m_storeThread.join();
    }
}

//////////////////////////////////////////////////////////////////////////

bool Logger::create(std::string const& name)
{
    m_dataName = "sense-" + name + ".log";

    moveToBackup(m_dataName, s_dataFolder + "/" + m_dataName, s_dataFolder + "/backups/deleted");
    remove((s_dataFolder + "/" + m_dataName).c_str());

    return true;
}

//////////////////////////////////////////////////////////////////////////

bool Logger::load(std::string const& name)
{
    Clock::time_point start = Clock::now();

    m_dataName = "sense-" + name + ".log";

    std::string dataFilename = (s_dataFolder + "/" + m_dataName);

    Data data;
    data.logLines.reserve(8192);

    {
        std::ifstream file(dataFilename, std::ios_base::binary);
        if (!file.is_open())
        {
            std::cerr << "Failed to open " << dataFilename << " file: " << std::strerror(errno) << "\n";
            return false;
        }

        std::string streamData((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();

        Crypt crypt;
        crypt.setKey(k_fileEncryptionKey);
        QByteArray decryptedData = crypt.decryptToByteArray(QByteArray(streamData.data(), streamData.size()));

        rapidjson::Document document;
        document.Parse(decryptedData.data(), decryptedData.size());
        if (document.GetParseError() != rapidjson::ParseErrorCode::kParseErrorNone)
        {
            document.Parse(streamData.data(), streamData.size());
            if (document.GetParseError() != rapidjson::ParseErrorCode::kParseErrorNone)
            {
                std::cerr << "Failed to open " << dataFilename << " file: " << rapidjson::GetParseErrorFunc(document.GetParseError()) << "\n";
                return false;
            }
        }
        if (!document.IsArray())
        {
            std::cerr << "Bad document.\n";
            return false;
        }

        for (size_t i = 0; i < document.Size(); i++)
        {
            LogLine line;
            rapidjson::Value const& linej = document[i];
            auto it = linej.FindMember("timestamp");
            if (it == linej.MemberEnd() || !it->value.IsUint64())
            {
                std::cerr << "Bad or missing log timestamp\n";
                return false;
            }
            line.timePoint = Clock::time_point(std::chrono::seconds(it->value.GetUint64()));

            it = linej.FindMember("type");
            if (it == linej.MemberEnd() || !it->value.IsInt())
            {
                std::cerr << "Bad or missing log type\n";
                return false;
            }
            line.type = static_cast<Type>(it->value.GetInt());

            it = linej.FindMember("message");
            if (it == linej.MemberEnd() || !it->value.IsString())
            {
                std::cerr << "Bad or missing sensor settings name\n";
                return false;
            }
            line.message = it->value.GetString();

            data.logLines.push_back(line);
        }
    }

    //initialize backups
    std::pair<std::string, time_t> bkf = getMostRecentBackup(dataFilename, s_dataFolder + "/backups/daily");
    if (bkf.first.empty())
    {
        m_lastDailyBackupTP = Settings::Clock::now();
    }
    else
    {
        m_lastDailyBackupTP = Settings::Clock::from_time_t(bkf.second);
    }
    bkf = getMostRecentBackup(dataFilename, s_dataFolder + "/backups/weekly");
    if (bkf.first.empty())
    {
        m_lastWeeklyBackupTP = Settings::Clock::now();
    }
    else
    {
        m_lastWeeklyBackupTP = Settings::Clock::from_time_t(bkf.second);
    }

    //done!!!

    m_mainData = data;

    std::cout << "Time to load logs:" << std::chrono::duration<float>(Clock::now() - start).count() << "s\n";
    std::cout.flush();

    return true;
}

//////////////////////////////////////////////////////////////////////////

void Logger::logVerbose(std::string const& message)
{
    m_mainData.logLines.emplace_back(LogLine{ Clock::now(), Type::VERBOSE, message });
    triggerSave();
}

//////////////////////////////////////////////////////////////////////////

void Logger::logInfo(std::string const& message)
{
    m_mainData.logLines.emplace_back(LogLine{ Clock::now(), Type::VERBOSE, message });
    triggerSave();
}

//////////////////////////////////////////////////////////////////////////

void Logger::logWarning(std::string const& message)
{
    m_mainData.logLines.emplace_back(LogLine{ Clock::now(), Type::VERBOSE, message });
    triggerSave();
}

//////////////////////////////////////////////////////////////////////////

void Logger::logError(std::string const& message)
{
    m_mainData.logLines.emplace_back(LogLine{ Clock::now(), Type::VERBOSE, message });
    triggerSave();
}

//////////////////////////////////////////////////////////////////////////

void Logger::triggerSave()
{
    {
        std::unique_lock<std::mutex> lg(m_storeMutex);
        m_storeData = m_mainData;
        m_storeThreadTriggered = true;
    }
    m_storeCV.notify_all();

    //std::cout << "Save triggered\n";
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

    std::string dataFilename = (s_dataFolder + "/" + m_dataName);

    Clock::time_point start = now;

    {
        rapidjson::Document document;
        document.SetArray();
        for (LogLine const& line: data.logLines)
        {
            rapidjson::Value linej;
            linej.SetObject();
            linej.AddMember("timestamp", static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(line.timePoint.time_since_epoch()).count()), document.GetAllocator());
            linej.AddMember("type", static_cast<int>(line.type), document.GetAllocator());
            linej.AddMember("message", rapidjson::Value(line.message.c_str(), document.GetAllocator()), document.GetAllocator());
            document.PushBack(linej, document.GetAllocator());
        }

        rapidjson::StringBuffer buffer;
        rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
        document.Accept(writer);

        Crypt crypt;
        crypt.setKey(k_fileEncryptionKey);
        QByteArray encryptedData = crypt.encryptToByteArray(QByteArray(buffer.GetString(), buffer.GetSize()));
//        QByteArray encryptedData = QByteArray(buffer.GetString(), buffer.GetSize());

        std::string tempFilename = (s_dataFolder + "/" + m_dataName + "_temp");
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

        copyToBackup(m_dataName, dataFilename, s_dataFolder + "/backups/incremental");

        if (!renameFile(tempFilename.c_str(), dataFilename.c_str()))
        {
            std::cerr << "Error renaming files: " << getLastErrorAsString() << "\n";
        }
    }

    std::cout << "Time to save logs:" << std::chrono::duration<float>(Clock::now() - start).count() << "s\n";
    std::cout.flush();

    if (dailyBackup)
    {
        copyToBackup(m_dataName, dataFilename, s_dataFolder + "/backups/daily");
    }
    if (weeklyBackup)
    {
        copyToBackup(m_dataName, dataFilename, s_dataFolder + "/backups/weekly");
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
