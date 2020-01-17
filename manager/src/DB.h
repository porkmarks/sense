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
class Emailer;

class DB : public QObject
{
    Q_OBJECT
public:
    DB();
    ~DB();

    typedef std::chrono::system_clock Clock;

    void process();

	static Result<void> create(sqlite3& db);
    Result<void> load(sqlite3& db);

	////////////////////////////////////////////////////////////////////////////

	enum class DateTimeFormat
	{
		DD_MM_YYYY_Dash,
		YYYY_MM_DD_Slash,
		YYYY_MM_DD_Dash,
		MM_DD_YYYY_Slash
	};

	struct GeneralSettings
	{
		DateTimeFormat dateTimeFormat = DateTimeFormat::DD_MM_YYYY_Dash;
	};

	bool setGeneralSettings(GeneralSettings const& settings);
	GeneralSettings const& getGeneralSettings() const;

	////////////////////////////////////////////////////////////////////////////

	struct CsvSettings
	{
		enum class UnitsFormat
		{
			None,
			Embedded,
			SeparateColumn
		};

		std::optional<DateTimeFormat> dateTimeFormatOverride;
		UnitsFormat unitsFormat = UnitsFormat::Embedded;
		bool exportId = true;
		bool exportIndex = true;
		bool exportSensorName = true;
		bool exportSensorSN = true;
		bool exportTimePoint = true;
		bool exportReceivedTimePoint = true;
		bool exportTemperature = true;
		bool exportHumidity = true;
		bool exportBattery = true;
		bool exportSignal = true;
		uint32_t decimalPlaces = 1;
		std::string separator = ",";
	};

	bool setCsvSettings(CsvSettings const& settings);
	CsvSettings const& getCsvSettings() const;

	////////////////////////////////////////////////////////////////////////////

	struct EmailSettings
	{
		enum class Connection : uint8_t
		{
			Ssl,
			Tls,
			Tcp,
		};

		std::string host;
		uint16_t port = 0;
		Connection connection = Connection::Ssl;
		std::string username;
		std::string password;
		std::string sender;
		std::vector<std::string> recipients;
	};

	bool setEmailSettings(EmailSettings const& settings);
	EmailSettings const& getEmailSettings() const;

	////////////////////////////////////////////////////////////////////////////

	struct FtpSettings
	{
		std::string host;
		uint16_t port = 0;
		std::string username;
		std::string password;
		std::string folder;
		bool uploadBackups = false;
		Clock::duration uploadBackupsPeriod = std::chrono::hours(24);
	};

	bool setFtpSettings(FtpSettings const& settings);
	FtpSettings const& getFtpSettings() const;

	////////////////////////////////////////////////////////////////////////////

	struct UserDescriptor
	{
		std::string name;
		std::string passwordHash;

		//do not change the order of these!!!!
		enum Permissions
		{
			PermissionAddRemoveSensors = 1 << 0,
			PermissionChangeSensors = 1 << 1,
			PermissionAddRemoveBaseStations = 1 << 2,
			PermissionChangeBaseStations = 1 << 3,
			PermissionChangeMeasurements = 1 << 4,
			PermissionChangeEmailSettings = 1 << 5,
			PermissionChangeFtpSettings = 1 << 6,
			PermissionAddRemoveAlarms = 1 << 7,
			PermissionChangeAlarms = 1 << 8,
			PermissionAddRemoveReports = 1 << 9,
			PermissionChangeReports = 1 << 10,
			PermissionAddRemoveUsers = 1 << 11,
			PermissionChangeUsers = 1 << 12,
			PermissionChangeSensorSettings = 1 << 13,
		};

		uint32_t permissions = 0;

		enum class Type : uint8_t
		{
			Admin, //has all the permissions
			Normal
		};
		Type type = Type::Normal;
	};

	typedef uint32_t UserId;
	struct User
	{
		UserDescriptor descriptor;
		UserId id = 0;
		Clock::time_point lastLogin = Clock::time_point(Clock::duration::zero());
	};

	size_t getUserCount() const;
	User getUser(size_t index) const;
	int32_t findUserIndexByName(std::string const& name) const;
	int32_t findUserIndexById(UserId id) const;
	int32_t findUserIndexByPasswordHash(std::string const& passwordHash) const;
	bool addUser(UserDescriptor const& descriptor);
	Result<void> setUser(UserId id, UserDescriptor const& descriptor);
	void removeUser(size_t index);
	bool needsAdmin() const;
	void setLoggedInUserId(UserId id);
	UserId getLoggedInUserId() const;
	std::optional<User> getLoggedInUser() const;
	bool isLoggedInAsAdmin() const;

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
    BaseStation getBaseStation(size_t index) const;
    int32_t findBaseStationIndexByName(std::string const& name) const;
    int32_t findBaseStationIndexById(BaseStationId id) const;
    int32_t findBaseStationIndexByMac(BaseStationDescriptor::Mac const& mac) const;
    bool addBaseStation(BaseStationDescriptor const& descriptor);
    bool setBaseStation(BaseStationId id, BaseStationDescriptor const& descriptor);
    void removeBaseStation(size_t index);

    ////////////////////////////////////////////////////////////////////////////

	struct SensorSettings
	{
		int8_t radioPower = 10; //dBm.
		float alertBatteryLevel = 0.1f; //0 - 1 mu
		float alertSignalStrengthLevel = 0.1f; //0 - 1 mu
	};

	Result<void> setSensorSettings(SensorSettings const& settings);
    SensorSettings getSensorSettings() const;

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
    SensorTimeConfig getSensorTimeConfig(size_t index) const;
    SensorTimeConfig getLastSensorTimeConfig() const;
    Clock::duration computeActualCommsPeriod(SensorTimeConfigDescriptor const& descriptor) const;
	SensorTimeConfig findSensorTimeConfigForMeasurementIndex(uint32_t index) const;

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
        int16_t s2b = 0;
        int16_t b2s = 0;
    };

    struct AlarmTriggers
    {
        uint32_t current = 0;
        uint32_t added = 0; //what was added
        uint32_t removed = 0; //what was silenced
        AlarmTriggers operator|(AlarmTriggers const& other) const { return { current | other.current, added | other.added, removed | other.removed }; }
		AlarmTriggers operator|(uint32_t other) const { return { current | other, added | other, removed | other}; }
		AlarmTriggers& operator|=(AlarmTriggers const& other) { current |= other.current; added |= other.added; removed |= other.removed; return *this; };
        AlarmTriggers& operator|=(uint32_t other) { current |= other; added |= other; removed |= other; return *this; };
        AlarmTriggers operator&(AlarmTriggers const& other) const { return { current & other.current, added & other.added, removed & other.removed }; }
		AlarmTriggers operator&(uint32_t other) const { return { current & other, added & other, removed & other}; }
		AlarmTriggers& operator&=(AlarmTriggers const& other) { current &= other.current; added &= other.added; removed &= other.removed; return *this; };
        AlarmTriggers& operator&=(uint32_t other) { current &= other; added &= other; removed &= other; return *this; };
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
        AlarmTriggers alarmTriggers;
    };

    struct ErrorCounters
    {
        uint32_t commsBlackouts = 0; //skipped coms cycles
        uint32_t commsFailures = 0; //generic comms failures reported by the sensor
        //various reboot reasons
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
        int16_t lastSignalStrengthB2S = 0; //dBm

        SignalStrength averageSignalStrength;

        //real-time measurement. This represents what the sensor is seeing 'now' (at the last comms).
        bool isRTMeasurementValid = false;
        float rtMeasurementTemperature = 0;
        float rtMeasurementHumidity = 0;
        float rtMeasurementVcc = 0;
    };

    size_t getSensorCount() const;
    Sensor getSensor(size_t index) const;
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
        int16_t signalStrengthB2S = 0; //dBm

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
    Alarm getAlarm(size_t index) const;
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

			HighSoft            = HighHumiditySoft | HighTemperatureSoft,
			HighHard            = HighHumidityHard | HighTemperatureHard,
			LowSoft             = LowHumiditySoft | LowTemperatureSoft,
			LowHard             = LowHumidityHard | LowTemperatureHard,
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
    Report getReport(size_t index) const;
    int32_t findReportIndexByName(std::string const& name) const;
    int32_t findReportIndexById(ReportId id) const;
    Result<void> addReport(ReportDescriptor const& descriptor);
    Result<void> setReport(ReportId id, ReportDescriptor const& descriptor);
    void removeReport(size_t index);

    ////////////////////////////////////////////////////////////////////////////

    bool addMeasurement(MeasurementDescriptor const& descriptor);
    bool addMeasurements(std::vector<MeasurementDescriptor> descriptors);
	bool addSingleSensorMeasurements(SensorId sensorId, std::vector<MeasurementDescriptor> descriptors);

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
        Range<int16_t> signalStrengthFilter;

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
	void generalSettingsChanged();
	void csvSettingsChanged();
	void emailSettingsChanged();
	void ftpSettingsChanged();

	void userAdded(UserId id);
	void userRemoved(UserId id);
	void userChanged(UserId id);
	void userLoggedIn(UserId id);

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
	void reportTriggered(ReportId id, Clock::time_point from, Clock::time_point to);

    void measurementsAdded(SensorId id);
    void measurementsRemoved(SensorId id);
    void measurementsChanged();

   void alarmTriggersChanged(AlarmId alarmId, Measurement const& m, uint32_t oldTriggers, AlarmTriggers triggers);
   void alarmStillTriggered(AlarmId alarmId);

private:
    Result<void> checkAlarmDescriptor(AlarmDescriptor const& descriptor) const;
    void checkRepetitiveAlarms();
	bool isReportTriggered(Report const& report) const;
    void checkReports();
    AlarmTriggers _computeAlarmTriggers(Alarm& alarm, Measurement const& m);
    size_t _getFilteredMeasurements(Filter const& filter, std::vector<Measurement>* result) const;

    //static inline MeasurementId computeMeasurementId(MeasurementDescriptor const& md);
    //static inline SensorId getSensorIdFromMeasurementId(MeasurementId id);
    static Measurement unpackMeasurement(sqlite3_stmt* stmt);

    Clock::time_point computeNextCommsTimePoint(Sensor const& sensor, size_t sensorIndex) const;
    Clock::time_point computeNextMeasurementTimePoint(Sensor const& sensor) const;
    uint32_t computeNextMeasurementIndex(Sensor const& sensor) const;
    uint32_t computeNextRealTimeMeasurementIndex() const;
    Clock::time_point computeMeasurementTimepoint(MeasurementDescriptor const& md) const;
    AlarmTriggers computeAlarmTriggers(Measurement const& m);

    struct Data
    {
        bool generalSettingsChanged = false;
		GeneralSettings generalSettings;
		
        bool csvSettingsChanged = false;
		CsvSettings csvSettings;
		
        bool emailSettingsChanged = false;
		EmailSettings emailSettings;
		
        bool ftpSettingsChanged = false;
		FtpSettings ftpSettings;
		
        bool sensorSettingsChanged = false;
		SensorSettings sensorSettings;

        bool usersChanged = false;
		bool usersAddedOrRemoved = false;
		std::vector<User> users;
		
        bool baseStationsChanged = false;
		bool baseStationsAddedOrRemoved = false;
        std::vector<BaseStation> baseStations;
		
        bool sensorTimeConfigsChanged = false;
        std::vector<SensorTimeConfig> sensorTimeConfigs;
		
        bool sensorsChanged = false;
		bool sensorsAddedOrRemoved = false;
        std::vector<Sensor> sensors;
		
        bool alarmsChanged = false;
		bool alarmsAddedOrRemoved = false;
        std::vector<Alarm> alarms;
		
        bool reportsChanged = false;
		bool reportsAddedOrRemoved = false;
        std::vector<Report> reports;

		UserId loggedInUserId = UserId(-1);
		UserId lastUserId = 0;
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

	std::unique_ptr<Emailer> m_emailer;

    mutable std::recursive_mutex m_dataMutex;
    Data m_data;

	mutable std::recursive_mutex m_asyncMeasurementsMutex;
    std::vector<Measurement> m_asyncMeasurements;
    void addAsyncMeasurements();

    SignalStrength computeAverageSignalStrength(SensorId sensorId, Data const& data) const;

    bool m_saveScheduled = false;
    void scheduleSave();
	void save(bool newTransaction);
	void save(Data& data, bool newTransaction) const;
};
