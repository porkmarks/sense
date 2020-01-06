#pragma once

#include <QObject>
#include <array>
#include <chrono>
#include <optional>
#include <string>
#include <vector>
#include <set>
#include <map>
#include <mutex>
#include <thread>
#include <atomic>
#include <condition_variable>
#include "Result.h"
#include "Radio.h"

struct sqlite3;
struct sqlite3_stmt;

class DB : public QObject
{
    Q_OBJECT
public:
    DB();
    ~DB();

    typedef std::chrono::system_clock Clock;

    void test();

    void process();

	static Result<void> create(sqlite3& db);
    Result<void> load(sqlite3& db);

    ////////////////////////////////////////////////////////////////////////////

    struct BaseStationDescriptor
    {
        typedef std::array<uint8_t, 6> Mac;

        std::string name;
        Mac mac = {};
        //QHostAddress address;
    };

    typedef uint32_t BaseStationId;
    struct BaseStation
    {
        BaseStationDescriptor descriptor;
        BaseStationId id = 0;
    };

    size_t getBaseStationCount() const;
    BaseStation const& getBaseStation(size_t index) const;
    int32_t findBaseStationIndexByName(std::string const& name) const;
    int32_t findBaseStationIndexById(BaseStationId id) const;
    int32_t findBaseStationIndexByMac(BaseStationDescriptor::Mac const& mac) const;
    bool addBaseStation(BaseStationDescriptor const& descriptor);
    bool setBaseStation(BaseStationId id, BaseStationDescriptor const& descriptor);
    void removeBaseStation(size_t index);

    ////////////////////////////////////////////////////////////////////////////

	struct SensorSettings
	{
		int8_t radioPower = 10; //dBm. 0 means invalid
		float alertBatteryLevel = 0.1f; //0 - 1 mu
		float alertSignalStrengthLevel = 0.1f; //0 - 1 mu
	};

	Result<void> setSensorSettings(SensorSettings const& settings);
    SensorSettings const& getSensorSettings() const;

    struct SensorTimeConfigDescriptor
    {
        Clock::duration measurementPeriod = std::chrono::minutes(5);
        Clock::duration commsPeriod = std::chrono::minutes(10);
    };

    struct SensorTimeConfig
    {
        SensorTimeConfigDescriptor descriptor;
        Clock::duration computedCommsPeriod = std::chrono::minutes(10);
        //when did this config become active?
        Clock::time_point baselineMeasurementTimePoint = Clock::time_point(Clock::duration::zero());
        uint32_t baselineMeasurementIndex = 0;
    };


    Result<void> addSensorTimeConfig(SensorTimeConfigDescriptor const& descriptor);
    Result<void> setSensorTimeConfigs(std::vector<SensorTimeConfig> const& configs);

    size_t getSensorTimeConfigCount() const;
    SensorTimeConfig const& getSensorTimeConfig(size_t index) const;
    SensorTimeConfig const& getLastSensorTimeConfig() const;
    Clock::duration computeActualCommsPeriod(SensorTimeConfigDescriptor const& descriptor) const;

    struct SensorErrors
    {
        enum type : uint32_t
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
        uint32_t sensorErrors = 0;
    };
    struct Measurement
    {
        MeasurementId id = 0;
        MeasurementDescriptor descriptor;
        Clock::time_point timePoint;
        Clock::time_point receivedTimePoint;
        uint32_t alarmTriggers = 0;
    };

    struct ErrorCounters
    {
        uint32_t comms = 0;
        uint32_t unknownReboots = 0;
        uint32_t powerOnReboots = 0;
        uint32_t resetReboots = 0;
        uint32_t brownoutReboots = 0;
        uint32_t watchdogReboots = 0;
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

        struct DeviceInfo
        {
			uint8_t sensorType = 0;
			uint8_t hardwareVersion = 0;
			uint8_t softwareVersion = 0;
        };

        enum class State : uint8_t
        {
            Active,
            Sleeping,
            Unbound
        };

        SensorDescriptor descriptor;
        SensorId id = 0;
        SensorAddress address = 0;
        DeviceInfo deviceInfo;
        Calibration calibration;
        SensorSerialNumber serialNumber = 0;
        State state = State::Active; //the current state of the sensor
        bool shouldSleep = false; //the command given to the sensor
        Clock::time_point sleepStateTimePoint = Clock::time_point(Clock::duration::zero());

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

        //real-time measurement. This represents what the sensor is seeing 'now' (at the last comms).
        bool isRTMeasurementValid = false;
        float rtMeasurementTemperature = 0;
        float rtMeasurementHumidity = 0;
        float rtMeasurementVcc = 0;
    };

    size_t getSensorCount() const;
    Sensor const& getSensor(size_t index) const;
    Result<void> addSensor(SensorDescriptor const& descriptor);
    Result<void> setSensor(SensorId id, SensorDescriptor const& descriptor);
    Result<void> setSensorCalibration(SensorId id, Sensor::Calibration const& calibration);
    Result<void> setSensorSleep(SensorId id, bool sleep);
    Result<SensorId> bindSensor(uint32_t serialNumber, uint8_t sensorType, uint8_t hardwareVersion, uint8_t softwareRevision, Sensor::Calibration const& calibration);
    Result<void> clearErrorCounters(SensorId id);

    struct SensorInputDetails
    {
        SensorId id = 0;

        bool hasDeviceInfo = false;
        Sensor::DeviceInfo deviceInfo;

        bool hasMeasurement = false;
        float measurementTemperature = 0;
        float measurementHumidity = 0;
        float measurementVcc = 0;

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
        //uint32_t baselineMeasurementIndex = 0; //this sensor will measure starting from this index only
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
        float lowTemperatureSoft = 0;
        float lowTemperatureHard = 0;

        bool highTemperatureWatch = false;
        float highTemperatureSoft = 0;
        float highTemperatureHard = 0;

        bool lowHumidityWatch = false;
        float lowHumiditySoft = 0;
        float lowHumidityHard = 0;

        bool highHumidityWatch = false;
        float highHumiditySoft = 0;
        float highHumidityHard = 0;

        bool lowVccWatch = false;
        bool lowSignalWatch = false;
//        bool sensorErrorsWatch = false;

        bool sendEmailAction = false;

		Clock::duration resendPeriod = std::chrono::hours(1 * 24);
    };

    typedef uint32_t AlarmId;
    struct Alarm
    {
        AlarmDescriptor descriptor;
        AlarmId id = 0;

        std::map<SensorId, uint32_t> triggersPerSensor;
        Clock::time_point lastTriggeredTimePoint = Clock::time_point(Clock::duration::zero());
    };

    size_t getAlarmCount() const;
    Alarm const& getAlarm(size_t index) const;
    int32_t findAlarmIndexByName(std::string const& name) const;
    int32_t findAlarmIndexById(AlarmId id) const;
    Result<void> addAlarm(AlarmDescriptor const& descriptor);
    Result<void> setAlarm(AlarmId id, AlarmDescriptor const& descriptor);
    void removeAlarm(size_t index);

    ////////////////////////////////////////////////////////////////////////////

    struct AlarmTrigger
    {
        enum
        {
            LowTemperatureSoft  = 1 << 0,
            LowTemperatureHard  = 1 << 1,
            LowTemperature      = LowTemperatureSoft | LowTemperatureHard,
            HighTemperatureSoft = 1 << 2,
            HighTemperatureHard = 1 << 3,
            HighTemperature     = HighTemperatureSoft | HighTemperatureHard,
            Temperature         = LowTemperature | HighTemperature,

			LowHumiditySoft     = 1 << 4,
			LowHumidityHard     = 1 << 5,
			LowHumidity         = LowHumiditySoft | LowHumidityHard,
			HighHumiditySoft    = 1 << 6,
			HighHumidityHard    = 1 << 7,
			HighHumidity        = HighHumiditySoft | HighHumidityHard,
            Humidity            = LowHumidity | HighHumidity,

            LowVcc              = 1 << 8,
            LowSignal           = 1 << 9,
        };
    };

    struct ReportDescriptor
    {
        std::string name;

        enum class Period : uint8_t
        {
            Daily,
            Weekly,
            Monthly,
            Custom
        };

        Period period = Period::Weekly;
        Clock::duration customPeriod = std::chrono::hours(48);

        enum class Data : uint8_t
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
        ReportId id = 0;
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
    bool addMeasurements(std::vector<MeasurementDescriptor> descriptors);

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

    size_t getAllMeasurementCount() const;

    std::vector<Measurement> getFilteredMeasurements(Filter const& filter) const;
    size_t getFilteredMeasurementCount(Filter const& filter) const;

    Result<Measurement> getLastMeasurementForSensor(SensorId sensorId) const;

    Result<Measurement> findMeasurementById(MeasurementId id) const;
    Result<void> setMeasurement(MeasurementId id, MeasurementDescriptor const& measurement);

    ////////////////////////////////////////////////////////////////////////////

signals:
    void baseStationAdded(BaseStationId id);
    void baseStationRemoved(BaseStationId id);
    void baseStationChanged(BaseStationId id);

    void sensorTimeConfigAdded();
    void sensorTimeConfigChanged();

    void sensorSettingsChanged();

    void sensorAdded(SensorId id);
    void sensorBound(SensorId id);
    void sensorRemoved(SensorId id);
    void sensorChanged(SensorId id);
    void sensorDataChanged(SensorId id);

    void alarmAdded(AlarmId id);
    void alarmRemoved(AlarmId id);
    void alarmChanged(AlarmId id);

    void reportAdded(ReportId id);
    void reportRemoved(ReportId id);
    void reportChanged(ReportId id);

    void measurementsAdded(SensorId id);
    void measurementsChanged(SensorId id);
    void measurementsRemoved(SensorId id);

   void alarmTriggersChanged(AlarmId alarmId, Measurement const& m, uint32_t oldTriggers, uint32_t newTriggers, uint32_t addedTriggers, uint32_t removedTriggers);
   void alarmStillTriggered(AlarmId alarmId);

private:
    Result<void> checkAlarmDescriptor(AlarmDescriptor const& descriptor) const;
    void checkRepetitiveAlarms();
    uint32_t _computeAlarmTriggers(Alarm& alarm, Measurement const& m);
    bool _addMeasurements(SensorId sensorId, std::vector<MeasurementDescriptor> mds);
    size_t _getFilteredMeasurements(Filter const& filter, std::vector<Measurement>* result) const;

    static inline MeasurementId computeMeasurementId(MeasurementDescriptor const& md);
    static inline SensorId getSensorIdFromMeasurementId(MeasurementId id);
    static Measurement unpackMeasurement(sqlite3_stmt* stmt);

    Clock::time_point computeNextCommsTimePoint(Sensor const& sensor, size_t sensorIndex) const;
    Clock::time_point computeNextMeasurementTimePoint(Sensor const& sensor) const;
    uint32_t computeNextMeasurementIndex(Sensor const& sensor) const;
    uint32_t computeNextRealTimeMeasurementIndex() const;
    SensorTimeConfig const& findSensorTimeConfigForMeasurementIndex(uint32_t index) const;
    Clock::time_point computeMeasurementTimepoint(MeasurementDescriptor const& md) const;
    uint32_t computeAlarmTriggers(Measurement const& m);

    struct Data
    {
        std::vector<BaseStation> baseStations;
        std::vector<SensorTimeConfig> sensorTimeConfigs;
        std::vector<Sensor> sensors;
        std::vector<Alarm> alarms;
        std::vector<Report> reports;
        SensorSettings sensorSettings;

        BaseStationId lastBaseStationId = 0;
        SensorId lastSensorId = 0;
        AlarmId lastAlarmId = 0;
        ReportId lastReportId = 0;
        MeasurementId lastMeasurementId = 0;
        uint32_t lastSensorAddress = Radio::SLAVE_ADDRESS_BEGIN;
    };

	sqlite3* m_sqlite = nullptr;
	std::shared_ptr<sqlite3_stmt> m_saveBaseStationsStmt;
	std::shared_ptr<sqlite3_stmt> m_saveSensorTimeConfigsStmt;
	std::shared_ptr<sqlite3_stmt> m_saveSensorSettingsStmt;
	std::shared_ptr<sqlite3_stmt> m_saveSensorsStmt;
	std::shared_ptr<sqlite3_stmt> m_saveAlarmsStmt;
	std::shared_ptr<sqlite3_stmt> m_saveReportsStmt;
	std::shared_ptr<sqlite3_stmt> m_addMeasurementsStmt;


    mutable std::recursive_mutex m_dataMutex;
    Data m_data;

	mutable std::recursive_mutex m_asyncMeasurementsMutex;
    std::vector<Measurement> m_asyncMeasurements;
    void addAsyncMeasurements();

    SignalStrength computeAverageSignalStrength(SensorId sensorId, Data const& data) const;

    bool m_saveScheduled = false;
    void scheduleSave();
    void save();
    void save(Data const& data) const;
};
