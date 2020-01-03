#pragma once

#include <string>
#include <QIcon>
#include <functional>

namespace utils
{

std::pair<std::string, time_t> getMostRecentBackup(std::string const& filename, std::string const& folder);
void copyToBackup(std::string const& filename, std::string const& srcFilepath, std::string const& folder, size_t maxBackups);
void moveToBackup(std::string const& filename, std::string const& srcFilepath, std::string const& folder, size_t maxBackups);
bool renameFile(std::string const& oldFilepath, std::string const& newFilepath);
std::string getLastErrorAsString();

static constexpr float k_maxBatteryLevel = 2.9f;
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

class epilogue final
{
public:
	inline epilogue(std::function<void()> func)
		: m_operation(func)
	{}
	inline epilogue(epilogue&& other)
		: m_operation(std::move(other.m_operation))
	{}
	inline epilogue& operator=(epilogue&& other)
	{
		m_operation = std::move(other.m_operation);
	}
	inline ~epilogue()
	{
		if (m_operation)
		{
			m_operation();
		}
	}
private:
	std::function<void()> m_operation;
};

}
