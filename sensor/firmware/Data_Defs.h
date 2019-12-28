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
    /*  0 */ ACK,
    /*  1 */ MEASUREMENT_BATCH_REQUEST,
    /*  2 */ PAIR_REQUEST,
    /*  3 */ PAIR_RESPONSE,
    /*  4 */ CONFIG_REQUEST,
    /*  5 */ CONFIG_RESPONSE,
    /*  6 */ FIRST_CONFIG_REQUEST,
    /*  7 */ FIRST_CONFIG_RESPONSE,
};


#ifndef __AVR__
#   pragma pack(push, 1) // exact fit - no padding
#endif

struct Ack
{
    uint16_t ack : 1;
    uint16_t req_id : 15;
};
static_assert(sizeof(Ack) == 2, "");

struct Measurement
{
    void pack(float humidity, float temperature)
    {
        this->qhumidity = static_cast<uint8_t>(humidity * 2.55f);
        this->qtemperature = static_cast<uint16_t>(temperature * 100.f);
    }

    void unpack(float& humidity, float& temperature) const
    {
        humidity = static_cast<float>(this->qhumidity) / 2.55f;
        temperature = static_cast<float>(this->qtemperature) / 100.f;
    }

//packed data
    uint8_t qhumidity = 0; //*2.55
    int16_t qtemperature = 0; //*100
};
static_assert(sizeof(Measurement) == 3, "");

typedef uint8_t QVCC;
inline QVCC pack_qvcc(float vcc)
{
    vcc -= 2.f;
    return static_cast<QVCC>(fmin(fmax(vcc, 0.f), 2.55f) * 100.f);
}
inline float unpack_qvcc(QVCC qvcc)
{
    return static_cast<float>(qvcc) / 100.f + 2.f;
}

struct Measurement_Batch_Request
{
    uint32_t start_index = 0;
    uint8_t last_batch : 1; //indicates if there are more batches after this one
    uint8_t count : 7;
    QVCC qvcc = 0; //(vcc - 2) * 100

    enum { MAX_COUNT = 8 };
    Measurement measurements[MAX_COUNT];
};
static_assert(sizeof(Measurement_Batch_Request) == 30, "");
//template<int s> struct Size_Checker;
//Size_Checker<sizeof(Measurement_Batch)> size_checker;

struct Calibration
{
    int16_t temperature_bias = 0; //*100
    int16_t humidity_bias = 0; //*100
    uint8_t reserved[16] = {};
};
static_assert(sizeof(Calibration) == 20, "");

enum class Reboot_Flag : uint8_t
{
    REBOOT_POWER_ON = 1 << 0,
    REBOOT_RESET    = 1 << 1,
    REBOOT_BROWNOUT = 1 << 2,
    REBOOT_WATCHDOG = 1 << 3,
    REBOOT_UNKNOWN  = 1 << 4,
};

struct Config_Request
{
    uint8_t reboot_flags = 0;
    uint8_t comms_errors = 0;
    uint32_t first_measurement_index = 0;
    uint32_t measurement_count = 0;
    int8_t b2s_input_dBm = 0;
    bool sleeping = false;
    Measurement measurement;
    QVCC qvcc;

    Calibration calibration;
    uint32_t reserved[3];
};
static_assert(sizeof(Config_Request) == 48, "");

struct Config_Response
{
    //all are in seconds
    chrono::seconds next_comms_delay; //how much to wait for next comms
    chrono::seconds comms_period; //comms every this much seconds
    chrono::seconds next_measurement_delay; //how much to wait before th enext measurement
    chrono::seconds measurement_period; //measurements every this much seconds
    uint32_t last_confirmed_measurement_index = 0;

    //how much to change calibration. 0 means leave it unchanged
    Calibration calibration;

    bool sleeping = false;
    int8_t power = 15;
};
static_assert(sizeof(Config_Response) == 42, "");

struct Descriptor
{
    uint8_t sensor_type = 0;
    uint8_t hardware_version = 0;
    uint8_t software_version = 0;
    uint32_t serial_number;
    uint8_t reserved[16];
};

struct First_Config_Request : Config_Request
{
    Descriptor descriptor;
};
static_assert(sizeof(First_Config_Request) == 71, "");

struct First_Config_Response : Config_Response
{
    uint32_t first_measurement_index = 0;
};
static_assert(sizeof(First_Config_Response) == 46, "");

struct Pair_Request
{
    Descriptor descriptor;
    Calibration calibration;
};
static_assert(sizeof(Pair_Request) == 43, "");

struct Pair_Response
{
    uint32_t address = 0;
};
static_assert(sizeof(Pair_Response) == 4, "");

#ifndef __AVR__
#   pragma pack(pop)
#endif
}

enum class Server_Message : uint8_t
{
    SENSOR_REQ,         //manager
    SENSOR_RES,         //base station
    PING,               //manager
    PONG,               //base station
    CHANGE_RADIO_STATE,   //manager
    REVERTED_RADIO_STATE_TO_NORMAL,   //base station
};

enum class Radio_State : uint8_t
{
    NORMAL,
    PAIRING
};

}
