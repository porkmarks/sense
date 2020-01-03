#include "Utils.h"

#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <QFile>
#include <QDirIterator>
#include <QDateTime>
#include <algorithm>
#include <array>
#include "Logger.h"

#ifdef _WIN32
#   define NOMINMAX
#   include <windows.h>
#endif

extern Logger s_logger;

namespace utils
{
//////////////////////////////////////////////////////////////////////////

typedef std::pair<std::string, time_t> FD;
static std::vector<FD> getBackupFiles(std::string const& filename, std::string const& folder)
{
    std::vector<FD> bkFiles;

    QDirIterator it(folder.c_str(), QDirIterator::Subdirectories);
    while (it.hasNext())
    {
		it.next();
        QFileInfo info = it.fileInfo();
        QString fn = info.fileName();
        if (!info.isFile())
        {
            //std::cout << "Skipping folder '" << fn.toUtf8().data() << "'.\n";
            it.next();
            continue;
        }

        if (!fn.endsWith((filename + "_backup").c_str()))
        {
            //std::cout << "Skipping unrecognized file '" << fn.toUtf8().data() << "'.\n";
            it.next();
            continue;
        }

		QString tsStr = fn.mid(0, fn.size() - static_cast<int>((filename + "_backup").size()));
		if (tsStr.isEmpty())
		{
			//            std::cout << "Skipping unrecognized file '" << fn.toUtf8().data() << "'.\n";
			it.next();
			continue;
		}

		QDateTime dt = QDateTime::fromString(tsStr, "yyyy-MM-dd-HH-mm-ss");
        if (!dt.isValid())
        {
            //std::cout << "Skipping file '" << fn.toUtf8().data() << "' because of malformed timestamp.\n";
            it.next();
            continue;
        }

        bkFiles.emplace_back(std::string(fn.toUtf8().data()), dt.toTime_t());
    }

    return bkFiles;
}

static void clipBackups(std::string const& filename, std::string const& folder, size_t maxBackups)
{
    std::vector<FD> bkFiles = getBackupFiles(filename, folder);
    bool trim = bkFiles.size() > maxBackups;

    //s_logger.logVerbose(QString("File '%1' has %2 out of %3 backups. %4").arg(filename.c_str()).arg(bkFiles.size()).arg(maxBackups).arg(trim ? "Trimming" : "No trimming"));

    if (!trim)
    {
        return;
    }

    std::sort(bkFiles.begin(), bkFiles.end(), [](FD const& a, FD const& b) { return a.second < b.second; });
    while (bkFiles.size() > maxBackups)
    {
        //s_logger.logVerbose(QString("Deleting backup of file '%1'").arg(filename.c_str()));
        remove((folder + "/" + bkFiles.front().first).c_str());
        bkFiles.erase(bkFiles.begin());
    }
}

std::pair<std::string, time_t> getMostRecentBackup(std::string const& filename, std::string const& folder)
{
    std::vector<FD> bkFiles = getBackupFiles(filename, folder);
    if (bkFiles.empty())
    {
        return { std::string(), time_t() };
    }

    std::sort(bkFiles.begin(), bkFiles.end(), [](FD const& a, FD const& b) { return a.second > b.second; });
    return bkFiles.front();
}

void copyToBackup(std::string const& filename, std::string const& srcFilepath, std::string const& folder, size_t maxBackups)
{
    QString nowStr = QDateTime::currentDateTime().toString("yyyy-MM-dd-HH-mm-ss");
    std::string newFilepath = folder + "/" + nowStr.toUtf8().data() + filename + "_backup";

    QDir().mkpath(folder.c_str());

    if (!QFile::exists(srcFilepath.c_str()))
    {
        return;
    }

    if (!QFile::copy(srcFilepath.c_str(), newFilepath.c_str()))
    {
        return;
    }

    clipBackups(filename, folder, maxBackups);
}

void moveToBackup(std::string const& filename, std::string const& srcFilepath, std::string const& folder, size_t maxBackups)
{
	QString nowStr = QDateTime::currentDateTime().toString("yyyy-MM-dd-HH-mm-ss");
	std::string newFilepath = folder + "/" + nowStr.toUtf8().data() + filename + "_backup";

    QDir().mkpath(folder.c_str());

    if (!QFile::exists(srcFilepath.c_str()))
    {
        return;
    }

    if (!renameFile(srcFilepath, newFilepath))
    {
        s_logger.logCritical(QString("Error moving file '%1' to '%2': %3").arg(srcFilepath.c_str()).arg(newFilepath.c_str()).arg(getLastErrorAsString().c_str()));
        return;
    }

    clipBackups(filename, folder, maxBackups);
}

bool renameFile(std::string const& oldFilepath, std::string const& newFilepath)
{
#ifdef _WIN32
    //one on success
    return MoveFileExA(oldFilepath.c_str(), newFilepath.c_str(), MOVEFILE_REPLACE_EXISTING) != 0;
#else
    //zero on success
    return rename(oldFilepath.c_str(), newFilepath.c_str()) == 0;
#endif
}

std::string getLastErrorAsString()
{
#ifdef _WIN32
    //Get the error message, if any.
    DWORD errorMessageID = ::GetLastError();
    if (errorMessageID == 0)
        return std::string(); //No error message has been recorded

    LPSTR messageBuffer = nullptr;
    size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                 NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

    std::string message(messageBuffer, size);

    //Free the buffer.
    LocalFree(messageBuffer);
    return message;
#else
    return std::strerror(errno);
#endif
}

float getBatteryLevel(float vcc)
{
    float level = std::max(std::min(vcc, k_maxBatteryLevel) - k_minBatteryLevel, 0.f) / (k_maxBatteryLevel - k_minBatteryLevel);
    level = std::pow(level, 5.f); //to revert the battery curve
    return level;
}

QIcon getBatteryIcon(float vcc)
{
    static const QIcon k_alertIcon = QIcon(":/icons/ui/battery-0.png");
    const static std::array<QIcon, 5> k_icons =
    {
        QIcon(":/icons/ui/battery-20.png"),
        QIcon(":/icons/ui/battery-40.png"),
        QIcon(":/icons/ui/battery-60.png"),
        QIcon(":/icons/ui/battery-80.png"),
        QIcon(":/icons/ui/battery-100.png")
    };

    float level = getBatteryLevel(vcc);
    if (level <= k_alertBatteryLevel)
    {
        return k_alertIcon;
    }

    //renormalize the remaining range
    level = (level - k_alertBatteryLevel) / (1.f - k_alertBatteryLevel);

    size_t index = std::min(static_cast<size_t>(level * static_cast<float>(k_icons.size())), k_icons.size() - 1);
    return k_icons[index];
}

float getSignalLevel(int8_t dBm)
{
    if (dBm == 0)
    {
        return 0.f;
    }
    float level = std::max(std::min(static_cast<float>(dBm), k_maxSignalLevel) - k_minSignalLevel, 0.f) / (k_maxSignalLevel - k_minSignalLevel);
    return level;
}

QIcon getSignalIcon(int8_t dBm)
{
    static const QIcon k_alertIcon = QIcon(":/icons/ui/signal-0.png");
    const static std::array<QIcon, 5> k_icons =
    {
        QIcon(":/icons/ui/signal-20.png"),
        QIcon(":/icons/ui/signal-40.png"),
        QIcon(":/icons/ui/signal-60.png"),
        QIcon(":/icons/ui/signal-80.png"),
        QIcon(":/icons/ui/signal-100.png")
    };

    float level = getSignalLevel(dBm);
    if (level <= k_alertSignalLevel)
    {
        return k_alertIcon;
    }

    //renormalize the remaining range
    level = (level - k_alertSignalLevel) / (1.f - k_alertSignalLevel);

    size_t index = std::min(static_cast<size_t>(level * static_cast<float>(k_icons.size())), k_icons.size() - 1);
    return k_icons[index];
}

uint32_t crc32(const void* data, size_t size)
{
    constexpr uint32_t Polynomial = 0xEDB88320;
    uint32_t crc = ~uint32_t(~0L);
    const uint8_t* p = reinterpret_cast<const uint8_t*>(data);
    while (size--)
    {
        crc ^= *p++;

        uint8_t lowestBit = crc & 1;
        crc >>= 1;
        if (lowestBit) crc ^= Polynomial;

        lowestBit = crc & 1;
        crc >>= 1;
        if (lowestBit) crc ^= Polynomial;

        lowestBit = crc & 1;
        crc >>= 1;
        if (lowestBit) crc ^= Polynomial;

        lowestBit = crc & 1;
        crc >>= 1;
        if (lowestBit) crc ^= Polynomial;

        lowestBit = crc & 1;
        crc >>= 1;
        if (lowestBit) crc ^= Polynomial;

        lowestBit = crc & 1;
        crc >>= 1;
        if (lowestBit) crc ^= Polynomial;

        lowestBit = crc & 1;
        crc >>= 1;
        if (lowestBit) crc ^= Polynomial;

        lowestBit = crc & 1;
        crc >>= 1;
        if (lowestBit) crc ^= Polynomial;
    }
    return ~crc;
}

//////////////////////////////////////////////////////////////////////////
}