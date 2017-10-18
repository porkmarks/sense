#pragma once

#include <QObject>
#include <chrono>
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <thread>
#include <atomic>
#include <condition_variable>

class DB : public QObject
{
    Q_OBJECT
public:
    DB();
    ~DB();

    typedef std::chrono::high_resolution_clock Clock;

    void test();

    bool create(std::string const& name);
    bool load(std::string const& name);

    ////////////////////////////////////////////////////////////////////////////

    struct SensorSettingsDescriptor
    {
        std::string name = "Base Station";
        bool sensorsSleeping = false;
        Clock::duration measurementPeriod = std::chrono::minutes(5);
        Clock::duration commsPeriod = std::chrono::minutes(10);
    };

    struct SensorSettings
    {
        SensorSettingsDescriptor descriptor;
        Clock::duration computedCommsPeriod;

        //This is computed when creating the config so that this equation holds for any config:
        // measurement_time_point = config.baseline_time_point + measurement_index * config.measurement_period
        //
        //So when creating a new config, this is how to calculate the baseline:
        // m = some measurement (any)
        // config.baseline_time_point = m.time_point - m.index * config.measurement_period
        //
        //The reason for this is to keep the indices valid in all configs
        Clock::time_point baselineTimePoint = Clock::now();
    };

    bool setSensorSettings(SensorSettingsDescriptor const& descriptor);
    SensorSettings const& getSensorSettings() const;

    struct SensorErrors
    {
        enum type : uint8_t
        {
            Hardware    = 1 << 0,
            Comms       = 1 << 1
        };
    };

    typedef uint32_t SensorId;
    typedef uint32_t SensorAddress;
    typedef uint64_t MeasurementId;

    struct MeasurementDescriptor
    {
        SensorId sensorId = 0;
        uint32_t index = 0;
        Clock::time_point timePoint;
        float temperature = 0;
        float humidity = 0;
        float vcc = 0;
        int8_t b2s = 0;
        int8_t s2b = 0;
        uint8_t sensorErrors = 0;
    };
    struct Measurement
    {
        MeasurementId id;
        MeasurementDescriptor descriptor;
        uint8_t triggeredAlarms = 0;
    };

    struct SensorDescriptor
    {
        std::string name;
    };

    struct Sensor
    {
        struct Calibration
        {
            float temperatureBias = 0.f;
            float humidityBias = 0.f;
        };

        enum class State
        {
            Active,
            Sleeping,
            Unbound
        };

        SensorDescriptor descriptor;
        SensorId id = 0;
        SensorAddress address = 0;
        Calibration calibration;
        uint32_t serialNumber = 0;
        State state = State::Active;
        Clock::time_point nextCommsTimePoint = Clock::time_point(Clock::duration::zero());
        Clock::time_point nextMeasurementTimePoint = Clock::time_point(Clock::duration::zero());

        bool isLastMeasurementValid = false;
        Measurement lastMeasurement;
    };

    size_t getSensorCount() const;
    Sensor const& getSensor(size_t index) const;
    bool addSensor(SensorDescriptor const& descriptor);
    bool bindSensor(SensorId id, SensorAddress address, uint32_t serialNumber, Sensor::Calibration const& calibration);
    bool setSensorState(SensorId id, Sensor::State state);
    bool setSensorNextTimePoints(SensorId id, Clock::time_point nextMeasurementTimePoint, Clock::time_point nextCommsTimePoint);
    void removeSensor(size_t index);
    int32_t findSensorIndexByName(std::string const& name) const;
    int32_t findSensorIndexById(SensorId id) const;

    ////////////////////////////////////////////////////////////////////////////

    struct AlarmDescriptor
    {
        std::string name;

        bool filterSensors = false;
        std::vector<SensorId> sensors;

        bool lowTemperatureWatch = false;
        float lowTemperature = 0;

        bool highTemperatureWatch = false;
        float highTemperature = 0;

        bool lowHumidityWatch = false;
        float lowHumidity = 0;

        bool highHumidityWatch = false;
        float highHumidity = 0;

        bool lowVccWatch = false;
        bool lowSignalWatch = false;
//        bool sensorErrorsWatch = false;

        bool sendEmailAction = false;
    };

    typedef uint32_t AlarmId;
    struct Alarm
    {
        AlarmDescriptor descriptor;
        AlarmId id;

        std::vector<SensorId> triggeringSensors;
    };

    size_t getAlarmCount() const;
    Alarm const& getAlarm(size_t index) const;
    int32_t findAlarmIndexByName(std::string const& name) const;
    int32_t findAlarmIndexById(AlarmId id) const;
    bool addAlarm(AlarmDescriptor const& descriptor);
    bool setAlarm(AlarmId id, AlarmDescriptor const& descriptor);
    void removeAlarm(size_t index);

    ////////////////////////////////////////////////////////////////////////////

    struct TriggeredAlarm
    {
        enum
        {
            Temperature     = 1 << 0,
            Humidity        = 1 << 1,
            LowVcc          = 1 << 2,
            LowSignal       = 1 << 3,
//            SensorErrors    = 1 << 4,

            All             = Temperature | Humidity | LowVcc | LowSignal/* | SensorErrors*/
        };
    };

    struct ReportDescriptor
    {
        std::string name;

        enum class Period
        {
            Daily,
            Weekly,
            Monthly,
            Custom
        };

        Period period = Period::Weekly;
        Clock::duration customPeriod = std::chrono::hours(48);

        enum class Data
        {
            Summary,
            All
        };

        Data data = Data::Summary;

        bool filterSensors = false;
        std::vector<SensorId> sensors;
    };

    typedef uint32_t ReportId;
    struct Report
    {
        ReportDescriptor descriptor;
        ReportId id;
        Clock::time_point lastTriggeredTimePoint = Clock::time_point(Clock::duration::zero());
    };

    size_t getReportCount() const;
    Report const& getReport(size_t index) const;
    int32_t findReportIndexByName(std::string const& name) const;
    int32_t findReportIndexById(ReportId id) const;
    bool addReport(ReportDescriptor const& descriptor);
    bool setReport(ReportId id, ReportDescriptor const& descriptor);
    void removeReport(size_t index);
    bool isReportTriggered(ReportId id) const;
    void setReportExecuted(ReportId id);

    ////////////////////////////////////////////////////////////////////////////

    bool addMeasurement(MeasurementDescriptor const& descriptor);

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

//        bool useSensorErrorsFilter = false;
//        bool sensorErrorsFilter = true;
    };

    std::vector<Measurement> getAllMeasurements() const;
    size_t getAllMeasurementCount() const;

    std::vector<Measurement> getFilteredMeasurements(Filter const& filter) const;
    size_t getFilteredMeasurementCount(Filter const& filter) const;

    bool getLastMeasurementForSensor(SensorId sensor_id, Measurement& measurement) const;

    ////////////////////////////////////////////////////////////////////////////

signals:
    void sensorSettingsWillBeChanged();
    void sensorSettingsChanged();

    void sensorWillBeAdded(SensorId id);
    void sensorAdded(SensorId id);
    void sensorBound(SensorId id);
    void sensorWillBeRemoved(SensorId id);
    void sensorRemoved(SensorId id);
    void sensorChanged(SensorId id);
    void sensorDataChanged(SensorId id);

    void alarmWillBeAdded(AlarmId id);
    void alarmAdded(AlarmId id);
    void alarmWillBeRemoved(AlarmId id);
    void alarmRemoved(AlarmId id);
    void alarmChanged(AlarmId id);
    void alarmTriggered(AlarmId id);

    void reportWillBeAdded(ReportId id);
    void reportAdded(ReportId id);
    void reportWillBeRemoved(ReportId id);
    void reportRemoved(ReportId id);
    void reportChanged(ReportId id);

    void measurementsWillBeAdded(SensorId id);
    void measurementsAdded(SensorId id);
    void measurementsWillBeRemoved(SensorId id);
    void measurementsRemoved(SensorId id);

    void alarmWasTriggered(AlarmId alarmId, SensorId sensorId, MeasurementDescriptor const& md);
    void alarmWasUntriggered(AlarmId alarmId, SensorId sensorId, MeasurementDescriptor const& md);

private:
    Clock::time_point computeBaselineTimePoint(SensorSettings const& oldSensorSettings, SensorSettingsDescriptor const& newDescriptor);

    size_t _getFilteredMeasurements(Filter const& filter, std::vector<Measurement>* result) const;
    bool cull(Measurement const& measurement, Filter const& filter) const;

#pragma pack(push, 1) // exact fit - no padding

    struct StoredMeasurement
    {
        uint32_t index;
        uint64_t timePoint;
        int16_t temperature;    //t * 100
        int16_t humidity;       //h * 100
        uint8_t vcc;            //(vcc - 2) * 100
        int8_t b2s;
        int8_t s2b;
        uint8_t sensorErrors;
        uint8_t triggeredAlarms;
    };

#pragma pack(pop) // exact fit - no padding

    typedef std::vector<StoredMeasurement> StoredMeasurements;

    static inline StoredMeasurement pack(Measurement const& m);
    static inline Measurement unpack(SensorId sensor_id, StoredMeasurement const& m);
    static inline MeasurementId computeMeasurementId(MeasurementDescriptor const& md);
    static inline MeasurementId computeMeasurementId(SensorId sensor_id, StoredMeasurement const& m);

    uint8_t computeTriggeredAlarm(MeasurementDescriptor const& md);

    struct Data
    {
        SensorSettings sensorSettings;
        std::vector<Sensor> sensors;
        std::vector<Alarm> alarms;
        std::vector<Report> reports;
        std::map<SensorId, StoredMeasurements> measurements;
        std::vector<MeasurementId> sortedMeasurementIds;
        SensorId lastSensorId = 0;
        AlarmId lastAlarmId = 0;
        ReportId lastReportId = 0;
        MeasurementId lastMeasurementId = 0;
    };

    Data m_mainData;

    std::atomic_bool m_threadsExit = { false };

    void triggerSave();
    void storeThreadProc();
    void save(Data const& data) const;
    std::atomic_bool m_storeThreadTriggered = { false };
    std::thread m_storeThread;
    std::condition_variable m_storeCV;
    std::mutex m_storeMutex;
    Data m_storeData;

    std::string m_dbName;
    std::string m_dataName;

    mutable Clock::time_point m_lastDailyBackupTP = Clock::time_point(Clock::duration::zero());
    mutable Clock::time_point m_lastWeeklyBackupTP = Clock::time_point(Clock::duration::zero());
};
