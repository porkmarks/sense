#pragma once

#include <QObject>
#include <chrono>
#include <string>
#include <vector>
#include <set>
#include <map>
#include <mutex>
#include <thread>
#include <atomic>
#include <condition_variable>
#include "Result.h"
#include "Sensor_Comms.h"

class DB : public QObject
{
    Q_OBJECT
public:
    DB();
    ~DB();

    typedef std::chrono::system_clock Clock;

    void test();

    void process();
    Result<void> create(std::string const& name);
    Result<void> load(std::string const& name);

    ////////////////////////////////////////////////////////////////////////////

    struct SensorsConfigDescriptor
    {
        std::string name = "Base Station";
        int8_t sensorsPower = 10;
        Clock::duration measurementPeriod = std::chrono::minutes(5);
        Clock::duration commsPeriod = std::chrono::minutes(10);
    };

    struct SensorsConfig
    {
        SensorsConfigDescriptor descriptor;
        Clock::duration computedCommsPeriod = std::chrono::minutes(10);
        //when did this config become active?
        Clock::time_point baselineMeasurementTimePoint = Clock::time_point(Clock::duration::zero());
        uint32_t baselineMeasurementIndex = 0;
    };


    Result<void> addSensorsConfig(SensorsConfigDescriptor const& descriptor);
    Result<void> setSensorsConfigs(std::vector<SensorsConfig> const& configs);

    size_t getSensorsConfigCount() const;
    SensorsConfig const& getSensorsConfig(size_t index) const;
    SensorsConfig const& getLastSensorsConfig() const;

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
    typedef uint32_t SensorSerialNumber;
    typedef uint64_t MeasurementId;

    struct SignalStrength
    {
        int8_t s2b = 0;
        int8_t b2s = 0;
    };

    struct MeasurementDescriptor
    {
        SensorId sensorId = 0;
        uint32_t index = 0;
        float temperature = 0;
        float humidity = 0;
        float vcc = 0;
        SignalStrength signalStrength;
        uint8_t sensorErrors = 0;
    };
    struct Measurement
    {
        MeasurementId id;
        MeasurementDescriptor descriptor;
        Clock::time_point timePoint;
        uint8_t triggeredAlarms = 0;
        int8_t combinedSignalStrength = 0;
    };

    struct ErrorCounters
    {
        uint32_t comms = 0;
        uint32_t reboot_unknown = 0;
        uint32_t reboot_power_on = 0;
        uint32_t reboot_reset = 0;
        uint32_t reboot_brownout = 0;
        uint32_t reboot_watchdog = 0;
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
        SensorSerialNumber serialNumber = 0;
        State state = State::Active;

        ErrorCounters errorCounters;

        Clock::time_point lastCommsTimePoint = Clock::time_point(Clock::duration::zero());

        //which is the last confirmed measurement for this sensor.
        uint32_t lastConfirmedMeasurementIndex = 0;

        //the range of stored measurements in this sensor
        uint32_t firstStoredMeasurementIndex = 0;
        uint32_t storedMeasurementCount = 0;

        //how much it's estimated the sensor actually stores
        //The storedMeasurementCount is from the last communication, so it doesn't reflect the current reality
        //It's delayes by one comms cycle
        //This value is a more accurate estmation of what the sensor stores now
        uint32_t estimatedStoredMeasurementCount = 0;

        //the signal strength from the base station to this sensor, in dBm
        int8_t lastSignalStrengthB2S = 0; //dBm

        SignalStrength averageSignalStrength;
        int8_t averageCombinedSignalStrength = 0; //dBm

        bool isLastMeasurementValid = false;
        Measurement lastMeasurement;
    };

    size_t getSensorCount() const;
    Sensor const& getSensor(size_t index) const;
    Result<void> addSensor(SensorDescriptor const& descriptor);
    Result<void> setSensor(SensorId id, SensorDescriptor const& descriptor);
    Result<void> bindSensor(uint32_t serialNumber, Sensor::Calibration const& calibration, SensorId& id);
    Result<void> setSensorState(SensorId id, Sensor::State state);

    struct SensorInputDetails
    {
        SensorId id;

        bool hasStoredData = false;
        uint32_t firstStoredMeasurementIndex = 0;
        uint32_t storedMeasurementCount = 0;

        bool hasSignalStrength = false;
        int8_t signalStrengthB2S = 0; //dBm

        bool hasLastCommsTimePoint = false;
        Clock::time_point lastCommsTimePoint = Clock::time_point(Clock::duration::zero());

        bool hasSleepingData = false;
        bool sleeping = false;

        bool hasErrorCountersDelta = false;
        ErrorCounters errorCountersDelta;
    };

    bool setSensorInputDetails(SensorInputDetails const& details);
    bool setSensorsInputDetails(std::vector<SensorInputDetails> const& details);

    struct SensorOutputDetails
    {
        Clock::duration commsPeriod = Clock::duration::zero();
        Clock::time_point nextCommsTimePoint = Clock::time_point(Clock::duration::zero());
        Clock::duration measurementPeriod = Clock::duration::zero();
        Clock::time_point nextMeasurementTimePoint = Clock::time_point(Clock::duration::zero());
        uint32_t nextRealTimeMeasurementIndex = 0; //the next measurement index for the crt date/time
        uint32_t nextMeasurementIndex = 0; //the next measurement index for this sensor
        uint32_t baselineMeasurementIndex = 0; //this sensor will measure starting from this index only
    };

    SensorOutputDetails computeSensorOutputDetails(SensorId id) const;

    void removeSensor(size_t index);
    int32_t findSensorIndexByName(std::string const& name) const;
    int32_t findSensorIndexById(SensorId id) const;
    int32_t findSensorIndexByAddress(SensorAddress address) const;
    int32_t findSensorIndexBySerialNumber(SensorSerialNumber serialNumber) const;
    int32_t findUnboundSensorIndex() const;

    ////////////////////////////////////////////////////////////////////////////

    struct AlarmDescriptor
    {
        std::string name;

        bool filterSensors = false;
        std::set<SensorId> sensors;

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

        std::set<SensorId> triggeringSensors;
    };

    size_t getAlarmCount() const;
    Alarm const& getAlarm(size_t index) const;
    int32_t findAlarmIndexByName(std::string const& name) const;
    int32_t findAlarmIndexById(AlarmId id) const;
    Result<void> addAlarm(AlarmDescriptor const& descriptor);
    Result<void> setAlarm(AlarmId id, AlarmDescriptor const& descriptor);
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
        std::set<SensorId> sensors;
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
    Result<void> addReport(ReportDescriptor const& descriptor);
    Result<void> setReport(ReportId id, ReportDescriptor const& descriptor);
    void removeReport(size_t index);
    bool isReportTriggered(ReportId id) const;
    void setReportExecuted(ReportId id);

    ////////////////////////////////////////////////////////////////////////////

    bool addMeasurement(MeasurementDescriptor const& descriptor);
    bool addMeasurements(std::vector<MeasurementDescriptor> const& descriptors);

    template <typename T>
    struct Range
    {
        T min = T();
        T max = T();
    };

    struct Filter
    {
        bool useSensorFilter = false;
        std::set<SensorId> sensorIds;

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

        bool useSignalStrengthFilter = false;
        Range<int8_t> signalStrengthFilter;

//        bool useSensorErrorsFilter = false;
//        bool sensorErrorsFilter = true;
    };

    std::vector<Measurement> getAllMeasurements() const;
    size_t getAllMeasurementCount() const;

    std::vector<Measurement> getFilteredMeasurements(Filter const& filter) const;
    size_t getFilteredMeasurementCount(Filter const& filter) const;

    bool getLastMeasurementForSensor(SensorId sensorId, Measurement& measurement) const;

    ////////////////////////////////////////////////////////////////////////////

signals:
    void sensorsConfigWillBeAdded();
    void sensorsConfigAdded();

    void sensorsConfigWillBeChanged();
    void sensorsConfigChanged();

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

    void alarmWasTriggered(AlarmId alarmId, Measurement const& m);
    void alarmWasUntriggered(AlarmId alarmId, Measurement const& m);

private:
    bool _addMeasurements(SensorId sensorId, std::vector<MeasurementDescriptor> mds);
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

    Clock::time_point computeNextCommsTimePoint(Sensor const& sensor, size_t sensorIndex) const;
    Clock::time_point computeNextMeasurementTimePoint(Sensor const& sensor) const;
    uint32_t computeNextMeasurementIndex(Sensor const& sensor) const;
    Clock::duration computeCommsPeriod() const;
    uint32_t computeNextRealTimeMeasurementIndex() const;
    SensorsConfig const& findSensorsConfigForMeasurementIndex(uint32_t index) const;
    Clock::time_point computeMeasurementTimepoint(MeasurementDescriptor const& md) const;
    uint8_t computeTriggeredAlarm(Measurement const& m);

    struct Data
    {
        std::vector<SensorsConfig> sensorsConfigs;
        std::vector<Sensor> sensors;
        std::vector<Alarm> alarms;
        std::vector<Report> reports;
        std::map<SensorId, StoredMeasurements> measurements;
        std::vector<MeasurementId> sortedMeasurementIds;
        SensorId lastSensorId = 0;
        AlarmId lastAlarmId = 0;
        ReportId lastReportId = 0;
        MeasurementId lastMeasurementId = 0;
        uint32_t lastSensorAddress = Sensor_Comms::SLAVE_ADDRESS_BEGIN;
    };

    Data m_mainData;

    SignalStrength computeAverageSignalStrength(SensorId sensorId, Data const& data) const;

    std::atomic_bool m_threadsExit = { false };

    void triggerSave();
    void triggerDelayedSave(Clock::duration i_dt);
    void storeThreadProc();
    void save(Data const& data) const;
    std::atomic_bool m_storeThreadTriggered = { false };
    std::thread m_storeThread;
    std::condition_variable m_storeCV;
    std::mutex m_storeMutex;
    Data m_storeData;

    std::string m_dbName;
    std::string m_dataName;

    std::mutex m_delayedTriggerSaveMutex;
    bool m_delayedTriggerSave = false;
    Clock::time_point m_delayedTriggerSaveTP;

    mutable Clock::time_point m_lastDailyBackupTP = Clock::time_point(Clock::duration::zero());
    mutable Clock::time_point m_lastWeeklyBackupTP = Clock::time_point(Clock::duration::zero());
};
