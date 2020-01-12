#pragma once

#include <string>
#include <QIcon>
#include <functional>
#include "DB.h"

namespace utils
{

std::pair<std::string, time_t> getMostRecentBackup(std::string const& filename, std::string const& folder);
void copyToBackup(std::string const& filename, std::string const& srcFilepath, std::string const& folder, size_t maxBackups);
void moveToBackup(std::string const& filename, std::string const& srcFilepath, std::string const& folder, size_t maxBackups);
bool renameFile(std::string const& oldFilepath, std::string const& newFilepath);
std::string getLastErrorAsString();

QString getQDateTimeFormatString(DB::DateTimeFormat format, bool millis = false);
template<typename Clock>
QString toString(typename Clock::time_point tp, DB::DateTimeFormat format)
{
	return QDateTime::fromTime_t(Clock::to_time_t(tp)).toString(getQDateTimeFormatString(format, false));
}

std::string getMacStr(DB::BaseStationDescriptor::Mac const& mac);

std::pair<std::string, int32_t> computeDurationString(DB::Clock::duration d);
std::pair<std::string, int32_t> computeRelativeTimePointString(DB::Clock::time_point tp);
uint32_t getSensorStorageCapacity(DB::Sensor const& sensor);
DB::Clock::duration computeBatteryLife(float capacity, DB::Clock::duration measurementPeriod, DB::Clock::duration commsPeriod, float power, uint8_t hardwareVersion);

struct CsvData
{
	DB::Measurement measurement;
	DB::Sensor sensor;
};
using CsvDataProvider = std::function<CsvData(size_t index)>;
bool exportToCsv(std::ostream& stream, DB::GeneralSettings const& settings, DB::CsvSettings const& csvSettings, CsvDataProvider provider, size_t count, bool unicode);


static constexpr float k_maxBatteryLevel = 2.95f;
static constexpr float k_minBatteryLevel = 2.0f;
//static constexpr float k_alertBatteryLevel = 0.1f;

static constexpr float k_maxSignalLevel = -70.f;
static constexpr float k_minSignalLevel = -120.f;
//static constexpr float k_alertSignalLevel = 0.1f;

static uint32_t k_lowThresholdSoftColor = 0xFF004a96;
static uint32_t k_lowThresholdHardColor = 0xFF011fFF;
static uint32_t k_highThresholdSoftColor = 0xFFf37736;
static uint32_t k_highThresholdHardColor = 0xFFff2015;
static uint32_t k_inRangeColor = 0xFF00b159;

float getBatteryLevel(float vcc);
QIcon getBatteryIcon(DB::SensorSettings const& settings, float vcc);
float getSignalLevel(int16_t dBm);
QIcon getSignalIcon(DB::SensorSettings const& settings, int16_t dBm);

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
