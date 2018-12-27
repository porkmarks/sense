#pragma once

#include <string>

std::pair<std::string, time_t> getMostRecentBackup(std::string const& filename, std::string const& folder);
void copyToBackup(std::string const& filename, std::string const& srcFilepath, std::string const& folder, size_t maxBackups);
void moveToBackup(std::string const& filename, std::string const& srcFilepath, std::string const& folder, size_t maxBackups);
bool renameFile(std::string const& oldFilepath, std::string const& newFilepath);
std::string getLastErrorAsString();
