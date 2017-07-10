#pragma once

#include <string>

std::pair<std::string, time_t> getMostRecentBackup(std::string const& filename, std::string const& folder);
void copyToBackup(std::string const& filename, std::string const& folder);
void moveToBackup(std::string const& filename, std::string const& folder);
bool renameFile(std::string const& oldName, std::string const& newName);
