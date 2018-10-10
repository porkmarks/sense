#pragma once

#include <stdint.h>
#include <math.h>
#include "Chrono.h"

namespace data
{
namespace sensor
{

enum class Type : uint8_t
{
    RESPONSE,
    MEASUREMENT_BATCH,
    PAIR_REQUEST,
    PAIR_RESPONSE,
    CONFIG_REQUEST,
    CONFIG,
    FIRST_CONFIG_REQUEST,
    FIRST_CONFIG,
};


#ifndef __AVR__
#   pragma pack(push, 1) // exact fit - no padding
#endif

struct Measurement
{
    enum class Flag : uint8_t
    {
        SENSOR_ERROR    = 1 << 0,
        COMMS_ERROR     = 1 << 1,
        REBOOT_POWER_ON = 1 << 2,
        REBOOT_RESET    = 1 << 3,
        REBOOT_BROWNOUT = 1 << 4,
        REBOOT_WATCHDOG = 1 << 5
    };

    void pack(float humidity, float temperature)
    {
        this->humidity = static_cast<uint8_t>(humidity * 2.55f);
        this->temperature = static_cast<uint16_t>(temperature * 100.f);
    }

    void unpack(float& humidity, float& temperature) const
    {
        humidity = static_cast<float>(this->humidity) / 2.55f;
        temperature = static_cast<float>(this->temperature) / 100.f;
    }

//packed data
    uint8_t flags = 0;
    uint8_t humidity = 0; //*2.55
    int16_t temperature = 0; //*100
};
static_assert(sizeof(Measurement) == 4, "");

struct Measurement_Batch
{
    uint32_t start_index = 0;
    uint8_t count = 0;

    uint8_t vcc = 0; //(vcc - 2) * 100
    void pack(float vcc)
    {
        vcc -= 2.f;
        this->vcc = static_cast<uint8_t>(fmin(fmax(vcc, 0.f), 2.55f) * 100.f);
    }
    void unpack(float& vcc) const
    {
        vcc = static_cast<float>(this->vcc) / 100.f + 2.f;
    }

    enum { MAX_COUNT = 8 };
    Measurement measurements[MAX_COUNT];
};
static_assert(sizeof(Measurement_Batch) == 38, "");
//template<int s> struct Size_Checker;
//Size_Checker<sizeof(Measurement_Batch)> size_checker;

struct Calibration
{
    int16_t temperature_bias = 0; //*100
    int16_t humidity_bias = 0; //*100
};
static_assert(sizeof(Calibration) == 4, "");


struct Config_Request
{
    uint32_t first_measurement_index = 0;
    uint32_t measurement_count = 0;
    int8_t b2s_input_dBm = 0;

    Calibration calibration;
};
static_assert(sizeof(Config_Request) == 13, "");

struct Config
{
    //all are in seconds
    chrono::seconds next_comms_delay; //how much to wait for next comms
    chrono::seconds comms_period; //comms every this much seconds
    chrono::seconds next_measurement_delay; //how much to wait before th enext measurement
    chrono::seconds measurement_period; //measurements every this much seconds
    uint32_t last_confirmed_measurement_index = 0;

    //how much to change calibration. 0 means leave it unchanged
    Calibration calibration_change;
};
static_assert(sizeof(Config) == 24, "");

struct First_Config_Request
{
};
static_assert(sizeof(First_Config_Request) == 1, "");

struct First_Config
{
    Config config;
    uint32_t first_measurement_index = 0;
};
static_assert(sizeof(First_Config) == 28, "");

struct Response
{
    uint16_t ack : 1;
    uint16_t req_id : 15;
};
static_assert(sizeof(Response) == 2, "");

struct Pair_Request
{
    Calibration calibration;
    uint32_t serial_number;
};
static_assert(sizeof(Pair_Request) == 8, "");

struct Pair_Response
{
    uint32_t address = 0;
};
static_assert(sizeof(Pair_Response) == 4, "");

#ifndef __AVR__
#   pragma pack(pop)
#endif
}

enum class Server_Message
{
    SET_CONFIGS_REQ,         //manager
    SET_CONFIGS_RES,         //base station
    ADD_CONFIG_REQ,         //manager
    ADD_CONFIG_RES,         //base station
    SET_SENSORS_REQ,        //manager
    SET_SENSORS_RES,        //base station
    ADD_SENSOR_REQ,         //manager
    ADD_SENSOR_RES,         //base station
    REPORT_MEASUREMENTS_REQ, //base station
    REPORT_MEASUREMENTS_RES, //manager
    SENSOR_BOUND_REQ,       //base station
    SENSOR_BOUND_RES,       //manager
    POWER_OFF_REQ,          //manager
    REPORT_SENSORS_DETAILS, //base station
};


}
