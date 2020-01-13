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

QString getQDateTimeFormatString(DB::DateTimeFormat format, bool millis)
{
    switch (format)
	{
    case DB::DateTimeFormat::DD_MM_YYYY_Dash: return QString("dd-MM-yyyy HH:mm%1").arg(millis ? ".zzz" : "");
    case DB::DateTimeFormat::YYYY_MM_DD_Slash: return QString("yyyy/MM/dd HH:mm%1").arg(millis ? ".zzz" : "");
    case DB::DateTimeFormat::YYYY_MM_DD_Dash: return QString("yyyy-MM-dd HH:mm%1").arg(millis ? ".zzz" : "");
    case DB::DateTimeFormat::MM_DD_YYYY_Slash: return QString("MM/dd/yyyy HH:mm%1").arg(millis ? ".zzz" : "");
	}
	return QString("dd-MM-yyyy HH:mm").arg(millis ? ".zzz" : "");
}

std::string getMacStr(DB::BaseStationDescriptor::Mac const& mac)
{
	char macStr[128];
	sprintf(macStr, "%X:%X:%X:%X:%X:%X", mac[0] & 0xFF, mac[1] & 0xFF, mac[2] & 0xFF, mac[3] & 0xFF, mac[4] & 0xFF, mac[5] & 0xFF);
	return macStr;
}

std::pair<std::string, int32_t> computeDurationString(DB::Clock::duration d)
{
	int32_t totalSeconds = std::chrono::duration_cast<std::chrono::seconds>(d).count();
	int32_t seconds = std::abs(totalSeconds);

	int32_t days = seconds / (24 * 3600);
	seconds -= days * (24 * 3600);

	int32_t hours = seconds / 3600;
	seconds -= hours * 3600;

	int32_t minutes = seconds / 60;
	seconds -= minutes * 60;

	char buf[256] = { '\0' };
	if (days > 0)
	{
		sprintf(buf, "%dd %02dh %02dm %02ds", days, hours, minutes, seconds);
	}
	else if (hours > 0)
	{
		sprintf(buf, "%dh %02dm %02ds", hours, minutes, seconds);
	}
	else if (minutes > 0)
	{
		sprintf(buf, "%dm %02ds", minutes, seconds);
	}
	else if (seconds > 0)
	{
		sprintf(buf, "%ds", seconds);
	}

	std::string str(buf);
	return std::make_pair(str, totalSeconds);
}

std::pair<std::string, int32_t> computeRelativeTimePointString(DB::Clock::time_point tp)
{
	return computeDurationString(tp - DB::Clock::now());
}

uint32_t getSensorStorageCapacity(DB::Sensor const& sensor)
{
	if (sensor.deviceInfo.sensorType == 1)
	{
		if (sensor.deviceInfo.hardwareVersion >= 2)
		{
			return 1000;
		}
	}
	return 0;
}

DB::Clock::duration computeBatteryLife(float capacity, DB::Clock::duration measurementPeriod, DB::Clock::duration commsPeriod, float power, uint8_t hardwareVersion)
{
	float idleMAh = 0.01f;
	float commsMAh = 0;
	float measurementMAh = 3;

    float minPower = -3.f;
    float maxPower = 17.f;
    float minCommsMAh = 0;
    float maxCommsMAh = 0;
	if (hardwareVersion >= 3)
	{
		minCommsMAh = 20.f;
		maxCommsMAh = 80.f;
	}
	else
	{
		minCommsMAh = 60.f;
		maxCommsMAh = 130.f;
	}
	float powerMu = std::clamp(power - minPower, 0.f, maxPower - minPower) / (maxPower - minPower);
	commsMAh = minCommsMAh + (maxCommsMAh - minCommsMAh) * powerMu;

	float commsDuration = 2.f;
	float commsPeriodS = std::chrono::duration<float>(commsPeriod).count();

	float measurementDuration = 1.f;
	float measurementPeriodS = std::chrono::duration<float>(measurementPeriod).count();

	float measurementPerHour = 3600.f / measurementPeriodS;
	float commsPerHour = 3600.f / commsPeriodS;

	float measurementDurationPerHour = measurementPerHour * measurementDuration;
	float commsDurationPerHour = commsPerHour * commsDuration;

	float commsUsagePerHour = commsMAh * commsDurationPerHour / 3600.f;
	float measurementUsagePerHour = measurementMAh * measurementDurationPerHour / 3600.f;

	float usagePerHour = commsUsagePerHour + measurementUsagePerHour + idleMAh;

	float hours = capacity / usagePerHour;
	return std::chrono::hours(static_cast<int32_t>(hours));
}

//////////////////////////////////////////////////////////////////////////

bool exportCsvTo(std::ostream& stream, DB::GeneralSettings const& settings, DB::CsvSettings const& csvSettings, CsvDataProvider provider, size_t count, bool unicode)
{
	//header
	if (csvSettings.exportId)
	{
		stream << "Id";
		stream << csvSettings.separator;
	}
	if (csvSettings.exportSensorName)
	{
		stream << "Sensor Name";
		stream << csvSettings.separator;
	}
	if (csvSettings.exportSensorSN)
	{
		stream << "Sensor S/N";
		stream << csvSettings.separator;
	}
	if (csvSettings.exportIndex)
	{
		stream << "Index";
		stream << csvSettings.separator;
	}
	if (csvSettings.exportTimePoint)
	{
		stream << "Timestamp";
		stream << csvSettings.separator;
	}
	if (csvSettings.exportReceivedTimePoint)
	{
		stream << "Received Timestamp";
		stream << csvSettings.separator;
	}
	if (csvSettings.exportTemperature)
	{
		stream << "Temperature";
		stream << csvSettings.separator;
		if (csvSettings.unitsFormat == DB::CsvSettings::UnitsFormat::SeparateColumn)
		{
			stream << "Temperature Unit";
			stream << csvSettings.separator;
		}
	}
	if (csvSettings.exportHumidity)
	{
		stream << "Humidity";
		stream << csvSettings.separator;
		if (csvSettings.unitsFormat == DB::CsvSettings::UnitsFormat::SeparateColumn)
		{
			stream << "Humidity Unit";
			stream << csvSettings.separator;
		}
	}
	if (csvSettings.exportBattery)
	{
		stream << "Battery";
		stream << csvSettings.separator;
		if (csvSettings.unitsFormat == DB::CsvSettings::UnitsFormat::SeparateColumn)
		{
			stream << "Battery Unit";
			stream << csvSettings.separator;
		}
	}
	if (csvSettings.exportSignal)
	{
		stream << "Signal";
		stream << csvSettings.separator;
		if (csvSettings.unitsFormat == DB::CsvSettings::UnitsFormat::SeparateColumn)
		{
			stream << "Signal Unit";
			stream << csvSettings.separator;
		}
	}
	stream << std::endl;

	for (size_t i = 0; i < count; i++)
	{
		std::optional<CsvData> data = provider(i);
		if (!data.has_value())
		{
			return false;
		}
		DB::Measurement const& m = data->measurement;
		DB::Sensor const& s = data->sensor;
		if (csvSettings.exportId)
		{
			stream << m.id;
			stream << csvSettings.separator;
		}
		if (csvSettings.exportSensorName)
		{
			stream << s.descriptor.name;
			stream << csvSettings.separator;
		}
		if (csvSettings.exportSensorSN)
		{
			stream << QString("%1").arg(s.serialNumber, 8, 16, QChar('0')).toUtf8().data();
			stream << csvSettings.separator;
		}
		if (csvSettings.exportIndex)
		{
			stream << m.descriptor.index;
			stream << csvSettings.separator;
		}
		if (csvSettings.exportTimePoint)
		{
			DB::DateTimeFormat dateTimeFormat = csvSettings.dateTimeFormatOverride.has_value() ? *csvSettings.dateTimeFormatOverride : settings.dateTimeFormat;
			QString str = toString<DB::Clock>(m.timePoint, dateTimeFormat);
			stream << str.toUtf8().data();
			stream << csvSettings.separator;
		}
		if (csvSettings.exportReceivedTimePoint)
		{
			DB::DateTimeFormat dateTimeFormat = csvSettings.dateTimeFormatOverride.has_value() ? *csvSettings.dateTimeFormatOverride : settings.dateTimeFormat;
			QString str = toString<DB::Clock>(m.receivedTimePoint, dateTimeFormat);
			stream << str.toUtf8().data();
			stream << csvSettings.separator;
		}
		if (csvSettings.exportTemperature)
		{
			stream << std::fixed << std::setprecision(csvSettings.decimalPlaces) << m.descriptor.temperature;
			if (csvSettings.unitsFormat == DB::CsvSettings::UnitsFormat::Embedded)
			{
				stream << (unicode ? u8"°C" : u8"\xB0""C");
			}
			stream << csvSettings.separator;
			if (csvSettings.unitsFormat == DB::CsvSettings::UnitsFormat::SeparateColumn)
			{
				stream << (unicode ? u8"°C" : u8"\xB0""C");
				stream << csvSettings.separator;
			}
		}
		if (csvSettings.exportHumidity)
		{
			stream << std::fixed << std::setprecision(csvSettings.decimalPlaces) << m.descriptor.humidity;
			if (csvSettings.unitsFormat == DB::CsvSettings::UnitsFormat::Embedded)
			{
				stream << " %RH";
			}
			stream << csvSettings.separator;
			if (csvSettings.unitsFormat == DB::CsvSettings::UnitsFormat::SeparateColumn)
			{
				stream << "%RH";
				stream << csvSettings.separator;
			}
		}
		if (csvSettings.exportBattery)
		{
			stream << std::fixed << std::setprecision(csvSettings.decimalPlaces) << utils::getBatteryLevel(m.descriptor.vcc) * 100.f;
			if (csvSettings.unitsFormat == DB::CsvSettings::UnitsFormat::Embedded)
			{
				stream << "%";
			}
			stream << csvSettings.separator;
			if (csvSettings.unitsFormat == DB::CsvSettings::UnitsFormat::SeparateColumn)
			{
				stream << "%";
				stream << csvSettings.separator;
			}
		}
		if (csvSettings.exportSignal)
		{
			stream << std::fixed << std::setprecision(csvSettings.decimalPlaces) << utils::getSignalLevel(std::min(m.descriptor.signalStrength.s2b, m.descriptor.signalStrength.b2s)) * 100.f;
			if (csvSettings.unitsFormat == DB::CsvSettings::UnitsFormat::Embedded)
			{
				stream << "%";
			}
			stream << csvSettings.separator;
			if (csvSettings.unitsFormat == DB::CsvSettings::UnitsFormat::SeparateColumn)
			{
				stream << "%";
				stream << csvSettings.separator;
			}
		}
		stream << std::endl;
	}

	return true;
}

float getBatteryLevel(float vcc)
{
    float level = std::max(std::min(vcc, k_maxBatteryLevel) - k_minBatteryLevel, 0.f) / (k_maxBatteryLevel - k_minBatteryLevel);
    level = std::pow(level, 5.f); //to revert the battery curve
    return level;
}

QIcon getBatteryIcon(DB::SensorSettings const& settings, float vcc)
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
    if (level <= settings.alertBatteryLevel)
    {
        return k_alertIcon;
    }

    //renormalize the remaining range
    level = (level - settings.alertBatteryLevel) / (1.f - settings.alertBatteryLevel);

    size_t index = std::min(static_cast<size_t>(level * static_cast<float>(k_icons.size())), k_icons.size() - 1);
    return k_icons[index];
}

float getSignalLevel(int16_t dBm)
{
    if (dBm == 0)
    {
        return 0.f;
    }
    float level = std::max(std::min(static_cast<float>(dBm), k_maxSignalLevel) - k_minSignalLevel, 0.f) / (k_maxSignalLevel - k_minSignalLevel);
    return level;
}

QIcon getSignalIcon(DB::SensorSettings const& settings, int16_t dBm)
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
    if (level <= settings.alertSignalStrengthLevel)
    {
        return k_alertIcon;
    }

    //renormalize the remaining range
    level = (level - settings.alertSignalStrengthLevel) / (1.f - settings.alertSignalStrengthLevel);

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