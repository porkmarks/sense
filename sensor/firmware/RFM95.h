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
    int16_t begin(float freq = 434.0, float bw = 125.0, uint8_t sf = 9, uint8_t cr = 7, uint8_t syncWord = SX127X_SYNC_WORD, int8_t power = 17, uint8_t currentLimit = 100, uint16_t preambleLength = 8, uint8_t gain = 0);
    int16_t transmit(uint8_t* data, size_t len);
    int16_t receive(uint8_t* data, size_t& len);
    int16_t receive(uint8_t* data, size_t& len, chrono::millis timeout);
    int16_t scanChannel();
    int16_t sleep();
    int16_t standby();

    // configuration methods
    int16_t setFrequency(float freq);
    int16_t setSyncWord(uint8_t syncWord);
    int16_t setCurrentLimit(uint8_t currentLimit);
    int16_t setPreambleLength(uint16_t preambleLength);
    int16_t setBandwidth(float bw);
    int16_t setSpreadingFactor(uint8_t sf);
    int16_t setCodingRate(uint8_t cr);
    int16_t setOutputPower(int8_t power);
    int16_t setGain(uint8_t gain);
    float getFrequencyError(bool autoCorrect = false);
    int8_t getRSSI();
    float getSNR();
    float getDataRate();
    int16_t setBitRate(float br);
    int16_t setFrequencyDeviation(float freqDev);

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

    int16_t tx(char* data, uint8_t length);
    int16_t rxSingle(char* data, uint8_t* length);
    int16_t setFrequencyRaw(float newFreq);
    int16_t setBandwidthRaw(uint8_t newBandwidth);
    int16_t setSpreadingFactorRaw(uint8_t newSpreadingFactor);
    int16_t setCodingRateRaw(uint8_t newCodingRate);
    int16_t config();
    int16_t getActiveModem();

private:
    float _dataRate;

    bool findChip(uint8_t ver);
    int16_t setMode(uint8_t mode);
    int16_t setActiveModem(uint8_t modem);
    void clearIRQFlags();
};

