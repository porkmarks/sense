#pragma once

#include <string>

void copyToBackup(std::string const& filename, std::string const& folder);
void moveToBackup(std::string const& filename, std::string const& folder);
bool renameFile(std::string const& oldName, std::string const& newName);
