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

constexpr size_t k_maxBackups = 10;

typedef std::pair<std::string, time_t> FD;
static std::vector<FD> getBackupFiles(std::string const& filename, std::string const& folder)
{
    std::vector<FD> bkFiles;

    QDirIterator it(folder.c_str(), QDirIterator::Subdirectories);
    while (it.hasNext())
    {
        QFileInfo info = it.fileInfo();
        QString fn = info.fileName();
        if (!info.isFile())
        {
            std::cout << "Skipping folder '" << fn.toUtf8().data() << "'.\n";
            it.next();
            continue;
        }

        if (!fn.startsWith((filename + "_").c_str()))
        {
            std::cout << "Skipping unrecognized file '" << fn.toUtf8().data() << "'.\n";
            it.next();
            continue;
        }

        QString tsStr = fn.mid(static_cast<int>(filename.size()) + 1);
        if (tsStr.isEmpty())
        {
            std::cout << "Skipping unrecognized file '" << fn.toUtf8().data() << "'.\n";
            it.next();
            continue;
        }

        bool ok = true;
        time_t tt = tsStr.toULongLong(&ok);
        if (!ok)
        {
            std::cout << "Skipping file '" << fn.toUtf8().data() << "' because of malformed timestamp.\n";
            it.next();
            continue;
        }

        bkFiles.emplace_back(std::string(fn.toUtf8().data()), tt);

        it.next();
    }

    return bkFiles;
}

static void clipBackups(std::string const& filename, std::string const& folder)
{
    std::vector<FD> bkFiles = getBackupFiles(filename, folder);
    if (bkFiles.size() <= k_maxBackups)
    {
        std::cout << "File '" << filename << "' has " << std::to_string(bkFiles.size()) << " backups. Not trimming yet.\n";
        return;
    }

    std::sort(bkFiles.begin(), bkFiles.end(), [](FD const& a, FD const& b) { return a.second < b.second; });
    while (bkFiles.size() > k_maxBackups)
    {
        std::cout << "Deleting bk file '" << bkFiles.front().first << "'.\n";
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

void copyToBackup(std::string const& filename, std::string const& srcFilepath, std::string const& folder)
{
    time_t nowTT = QDateTime::currentDateTime().toTime_t();
    std::string newFilepath = folder + "/" + filename + "_" + std::to_string(nowTT);

    QDir().mkpath(folder.c_str());

    if (!QFile::copy(srcFilepath.c_str(), newFilepath.c_str()))
    {
        return;
    }

    clipBackups(filename, folder);
}

void moveToBackup(std::string const& filename, std::string const& srcFilepath, std::string const& folder)
{
    time_t nowTT = QDateTime::currentDateTime().toTime_t();
    std::string newFilepath = folder + "/" + filename + "_" + std::to_string(nowTT);

    QDir().mkpath(folder.c_str());

    if (!renameFile(srcFilepath, newFilepath))
    {
        std::cerr << "Error renaming files: " << getLastErrorAsString() << "\n";
        return;
    }

    clipBackups(filename, folder);
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
    if(errorMessageID == 0)
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
