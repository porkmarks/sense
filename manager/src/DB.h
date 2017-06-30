#pragma once

#include <chrono>
#include <string>
#include <vector>
#include <map>
#include "Data_Defs.h"

class DB
{
public:
    DB();

    typedef uint32_t SensorId;
    typedef std::chrono::high_resolution_clock Clock;

    bool create(std::string const& name);
    bool open(std::string const& name);

    bool save() const;
    bool saveAs(std::string const& filename) const;

    struct ErrorFlag
    {
        enum type : uint8_t
        {
            SensorError   = 1 << 0,
            CommsError    = 1 << 1
        };
    };


    struct Alarm
    {
        bool lowTemperatureWatch = false;
        float lowTemperature = 0;

        bool highTemperatureWatch = false;
        float highTemperature = 0;

        bool lowHumidityWatch = false;
        float lowHumidity = 0;

        bool highHumidityWatch = false;
        float highHumidity = 0;

        bool vccWatch = false;
        bool signalWatch = false;
        bool errorFlagsWatch = false;

        bool sendEmailAction = false;
        std::string emailRecipient;
    };

    struct Measurement
    {
        SensorId sensorId = 0;
        uint32_t index = 0;
        Clock::time_point timePoint;
        float temperature = 0;
        float humidity = 0;
        float vcc = 0;
        int8_t b2s = 0;
        int8_t s2b = 0;
        uint8_t errorFlags = 0;
    };



    size_t getAlarmCount() const;
    Alarm const& getAlarm(size_t index) const;
    void addAlarm(Alarm const& alarm);
    void removeAlarm(size_t index);

    struct TriggeredAlarm
    {
        enum
        {
            Temperature     = 1 << 0,
            Humidity        = 1 << 1,
            Vcc             = 1 << 2,
            Signal          = 1 << 3,
            ErrorFlags      = 1 << 4,

            All             = Temperature | Humidity | Vcc | Signal | ErrorFlags
        };
    };

    uint8_t computeTriggeredAlarm(Measurement const& measurement) const;
    uint8_t computeTriggeredAlarm(SensorId sensorId) const;


    bool addMeasurement(Measurement const& measurement);

    template <typename T>
    struct Range
    {
        T min = T();
        T max = T();
    };

    struct Filter
    {
        bool useSensorFilter = false;
        std::vector<SensorId> sensorIds;

        bool useIndexFilter = false;
        Range<uint32_t> indexFilter;

        bool useTimePointFilter = false;
        Range<Clock::time_point> timePointFilter;

        bool useTemperatureFilter = false;
        Range<float> temperatureFilter;

        bool useHumidityFilter = false;
        Range<float> humidityFilter;

        bool useVccFilter = false;
        Range<float> vccFilter;

        bool useB2SFilter = false;
        Range<int8_t> b2sFilter;

        bool useS2BFilter = false;
        Range<int8_t> s2bFilter;

        bool useErrorsFilter = false;
        bool errorsFilter = true;
    };

    std::vector<Measurement> getAllMeasurements() const;
    size_t getAllMeasurementCount() const;

    std::vector<Measurement> getFilteredMeasurements(Filter const& filter) const;
    size_t getFilteredMeasurementCount(Filter const& filter) const;

    bool getLastMeasurementForSensor(SensorId sensor_id, Measurement& measurement) const;

private:

    std::vector<Alarm> m_alarms;

    bool cull(Measurement const& measurement, Filter const& filter) const;

    struct StoredMeasurement
    {
        uint32_t index;
        time_t time_point;
        int16_t temperature;    //t * 100
        int16_t humidity;       //h * 100
        uint8_t vcc;            //(vcc - 2) * 100
        int8_t b2s;
        int8_t s2b;
        uint8_t flags;
    };

    typedef std::vector<StoredMeasurement> StoredMeasurements;
    typedef uint64_t PrimaryKey;

    static inline StoredMeasurement pack(Measurement const& m);
    static inline Measurement unpack(SensorId sensor_id, StoredMeasurement const& m);
    static inline PrimaryKey computePrimaryKey(Measurement const& m);
    static inline PrimaryKey computePrimaryKey(SensorId sensor_id, StoredMeasurement const& m);

    std::map<SensorId, StoredMeasurements> m_measurements;
    std::vector<PrimaryKey> m_sortedPrimaryKeys;

    std::string m_filename;
};
