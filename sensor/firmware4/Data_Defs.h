#pragma once

#include <stdint.h>
#include <math.h>
#include "Chrono.h"

namespace data
{
namespace sensor
{
namespace v1
{
static const uint8_t k_version = 1;

enum class Type : uint8_t
{
    /*  0 */ MEASUREMENT_BATCH_REQUEST,
    /*  1 */ PAIR_REQUEST,
    /*  2 */ PAIR_RESPONSE,
    /*  3 */ CONFIG_REQUEST,
    /*  4 */ CONFIG_RESPONSE,
    /*  5 */ FIRST_CONFIG_REQUEST,
    /*  6 */ FIRST_CONFIG_RESPONSE,
};


#ifndef __AVR__
#   pragma pack(push, 1) // exact fit - no padding
#endif

struct Measurement
{
    void pack(float humidity, float temperature)
    {
        this->qhumidity = static_cast<uint16_t>(fmin(fmax(humidity, 0.f), 100.f) * 655.f + 0.5f);
        this->qtemperature = static_cast<int16_t>(fmin(fmax(temperature, -100.f), 100.f) * 327.f + 0.5f);
    }

    void unpack(float& humidity, float& temperature) const
    {
        humidity = static_cast<float>(this->qhumidity) / 655.f;
        temperature = static_cast<float>(this->qtemperature) / 327.f;
    }

    //packed data
    uint16_t qhumidity = 0; //*655
    int16_t qtemperature = 0; //*327
};
static_assert(sizeof(Measurement) == 4, "");

struct QVCC
{
    uint8_t value;
};
inline QVCC pack_qvcc(float vcc)
{
    vcc -= 2.f;
    return { static_cast<uint8_t>(fmin(fmax(vcc, 0.f), 1.27f) * 200.f + 0.5f) };
}
inline float unpack_qvcc(QVCC qvcc)
{
    return static_cast<float>(qvcc.value) / 200.f + 2.f;
}

struct QSS //signal strength
{
    uint8_t value;
};
inline QSS pack_qss(int16_t ss) //signal strength
{
    if (ss > 0) ss = 0;
    if (ss < 200) ss = 200;
    return { static_cast<uint8_t>(ss + 200) };
}
inline int16_t unpack_qss(QSS qss)
{
    return static_cast<int16_t>(qss.value) - int16_t(200);
}

struct Measurement_Batch_Request
{
    uint32_t start_index = 0;
    uint8_t last_batch : 1; //indicates if there are more batches after this one
    uint8_t count : 7;
    QVCC qvcc = { 0 }; //(vcc - 2) * 200

    enum { MAX_COUNT = 12 };
    Measurement measurements[MAX_COUNT];
};
static_assert(sizeof(Measurement_Batch_Request) == 54, "");
//template<int s> struct Size_Checker;
//Size_Checker<sizeof(Measurement_Batch)> size_checker;

struct Calibration
{
    int16_t temperature_bias = 0; //*100
    int16_t humidity_bias = 0; //*100
};
static_assert(sizeof(Calibration) == 4, "");

enum class Reboot_Flag : uint8_t
{
    REBOOT_POWER_ON = 1 << 0,
    REBOOT_RESET = 1 << 1,
    REBOOT_BROWNOUT = 1 << 2,
    REBOOT_WATCHDOG = 1 << 3,
    REBOOT_UNKNOWN = 1 << 4,
};

struct Stats
{
	uint8_t reboot_flags = 0;
    uint8_t comms_errors = 0;
    uint8_t comms_retries = 0;
    uint32_t asleep_ms = 0;
    uint32_t awake_ms = 0;
    uint16_t comms_rounds = 0;
    uint16_t measurement_rounds = 0;
};
static_assert(sizeof(Stats) == 15, "");

struct Config_Request
{
    Stats stats;
    uint32_t first_measurement_index = 0;
    uint32_t measurement_count = 0;
    QSS b2s_qss = { 255 };
    bool sleeping = false;
    Measurement measurement;
    QVCC qvcc;

    Calibration calibration;
};
static_assert(sizeof(Config_Request) == 34, "");

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
    uint8_t retries = 1;
};
static_assert(sizeof(Config_Response) == 27, "");

struct Descriptor
{
    uint8_t sensor_type = 0;
    uint8_t hardware_version = 0;
    uint8_t software_version = 0;
    uint32_t serial_number;
};

struct First_Config_Request : Config_Request
{
    Descriptor descriptor;
};
static_assert(sizeof(First_Config_Request) == 41, "");

struct First_Config_Response : Config_Response
{
    uint32_t first_measurement_index = 0;
};
static_assert(sizeof(First_Config_Response) == 31, "");

struct Pair_Request
{
    Descriptor descriptor;
    Calibration calibration;
};
static_assert(sizeof(Pair_Request) == 11, "");

struct Pair_Response
{
    uint32_t address = 0;
};
static_assert(sizeof(Pair_Response) == 4, "");

#ifndef __AVR__
#   pragma pack(pop)
#endif
}
} //v1


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
