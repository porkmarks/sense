#pragma once

#include "TypeDef.h"
#include "Module.h"
#include <string.h>
#include "Chrono.h"

// SX127X_REG_VERSION
#define RFM95_CHIP_VERSION                            0x11
#define SX127X_SYNC_WORD                              0x12        //  7     0     default LoRa sync word

class RFM95
{
public:
    // constructor
    RFM95(Module* mod);

    // basic methods
    int8_t begin(float freq = 434.0, float bw = 125.0, uint8_t sf = 9, uint8_t cr = 7, uint8_t syncWord = SX127X_SYNC_WORD, int8_t power = 17, uint8_t currentLimit = 0, uint16_t preambleLength = 8, uint8_t gain = 0);
    int8_t transmit(uint8_t* data, uint8_t len);
    int8_t receive(uint8_t* data, uint8_t& len);
    int8_t receive(uint8_t* data, uint8_t& len, chrono::millis timeout);

    int8_t startReceiving();
    int8_t getReceivedPackage(uint8_t* data, uint8_t& len, bool& gotIt);
    int8_t stopReceiving();

    int8_t sleep();
    int8_t standby();

    // configuration methods
    int8_t setFrequency(float freq);
    int8_t setSyncWord(uint8_t syncWord);
    int8_t setCurrentLimit(uint8_t currentLimit);
    int8_t setPreambleLength(uint16_t preambleLength);
    int8_t setBandwidth(float bw);
    int8_t setSpreadingFactor(uint8_t sf);
    int8_t setCodingRate(uint8_t cr);
    int8_t setOutputPower(int8_t power, bool useRFO);
    int8_t setGain(uint8_t gain);
    int16_t getRSSI();
    float getSNR();
    int8_t setBitRate(float br);
    int8_t setFrequencyDeviation(float freqDev);
    chrono::millis computeAirTimeUpperBound(uint8_t length);

#ifdef KITELIB_DEBUG
    void regDump();
#endif

protected:
    Module* _mod;

    float _freq;
    float _bw;
    uint8_t _sf;
    uint8_t _cr;
    uint16_t _preambleLength;
    bool _sleeping = false;
    bool _isReceivingPacket = false;

    struct TC
    {
        int16_t symbolLengthUS = 0;
        int16_t t1 = 0;
        int16_t t2 = 0;
        int16_t rxTimeoutSymbols = 0;
        int16_t rxTimeoutMS = 0;
    } _tc;

    int8_t tx(char* data, uint8_t length);
    int8_t rxSingle(char* data, uint8_t* length);
    int8_t setFrequencyRaw(float newFreq);
    int8_t setBandwidthRaw(uint8_t newBandwidth);
    int8_t setSpreadingFactorRaw(uint8_t newSpreadingFactor);
    int8_t setCodingRateRaw(uint8_t newCodingRate);
    int8_t config();
    uint8_t getActiveModem();
    void refreshTimeoutConstants();
    void refreshRXTimeoutConstants();

private:
    bool findChip(uint8_t ver);
    int8_t setMode(uint8_t mode, bool checked = true);
    int8_t setActiveModem(uint8_t modem);
    void clearIRQFlags();
};
