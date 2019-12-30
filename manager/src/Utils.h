#pragma once

#include <string>
#include <QIcon>

namespace utils
{

std::pair<std::string, time_t> getMostRecentBackup(std::string const& filename, std::string const& folder);
void copyToBackup(std::string const& filename, std::string const& srcFilepath, std::string const& folder, size_t maxBackups);
void moveToBackup(std::string const& filename, std::string const& srcFilepath, std::string const& folder, size_t maxBackups);
bool renameFile(std::string const& oldFilepath, std::string const& newFilepath);
std::string getLastErrorAsString();

static constexpr float k_maxBatteryLevel = 3.0f;
static constexpr float k_minBatteryLevel = 2.0f;
static constexpr float k_alertBatteryLevel = 0.1f;

static constexpr float k_maxSignalLevel = -70.f;
static constexpr float k_minSignalLevel = -120.f;
static constexpr float k_alertSignalLevel = 0.1f;

float getBatteryLevel(float vcc);
QIcon getBatteryIcon(float vcc);
float getSignalLevel(int8_t dBm);
QIcon getSignalIcon(int8_t dBm);

uint32_t crc32(const void* data, size_t size);

}
