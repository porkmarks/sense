#include "SX127x.h"

SX127x::SX127x(Module* mod) {
  _mod = mod;
}


int16_t SX127x::begin(uint8_t chipVersion, uint8_t syncWord, uint8_t currentLimit, uint16_t preambleLength) {
  // set module properties
  _mod->init(USE_SPI, INT_BOTH);
  
  // try to find the SX127x chip
  if(!SX127x::findChip(chipVersion)) {
    DEBUG_PRINTLN_STR("No SX127x found!");
    SPI.end();
    return(ERR_CHIP_NOT_FOUND);
  } else {
    DEBUG_PRINTLN_STR("Found SX127x!");
  }
  
  // check active modem
  int16_t state;
  if(getActiveModem() != SX127X_LORA) {
    // set LoRa mode
    state = setActiveModem(SX127X_LORA);
    if(state != ERR_NONE) {
      return(state);
    }
  }
  
  // set LoRa sync word
  state = SX127x::setSyncWord(syncWord);
  if(state != ERR_NONE) {
    return(state);
  }
  
  // set over current protection
  state = SX127x::setCurrentLimit(currentLimit);
  if(state != ERR_NONE) {
    return(state);
  }
  
  // set preamble length
  state = SX127x::setPreambleLength(preambleLength);
  
  // initalize internal variables
  _dataRate = 0.0;
  
  return(state);
}


int16_t SX127x::transmit(uint8_t* data, size_t len) {
  // set mode to standby
  int16_t state = setMode(SX127X_STANDBY);
  
  // check packet length
  if(len >= 256) {
    return(ERR_PACKET_TOO_LONG);
  }

  // calculate timeout
  uint16_t base = 1;
  float symbolLength = (float)(base << _sf) / (float)_bw;
  float de = 0;
  if(symbolLength >= 16.0) {
    de = 1;
  }
  float ih = (float)_mod->SPIgetRegValue(SX127X_REG_MODEM_CONFIG_1, 0, 0);
  float crc = (float)(_mod->SPIgetRegValue(SX127X_REG_MODEM_CONFIG_2, 2, 2) >> 2);
  float n_pre = (float)_mod->SPIgetRegValue(SX127X_REG_PREAMBLE_LSB);
  float n_pay = 8.0 + max(ceil((8.0 * (float)len - 4.0 * (float)_sf + 28.0 + 16.0 * crc - 20.0 * ih)/(4.0 * (float)_sf - 8.0 * de)) * (float)_cr, 0.0);
  uint32_t timeout = ceil(symbolLength * (n_pre + n_pay + 4.25) * 1.5);
  
  // set DIO mapping
  _mod->SPIsetRegValue(SX127X_REG_DIO_MAPPING_1, SX127X_DIO0_TX_DONE, 7, 6);
  
  // clear interrupt flags
  clearIRQFlags();
  
  // set packet length
  state |= _mod->SPIsetRegValue(SX127X_REG_PAYLOAD_LENGTH, len);
  
  // set FIFO pointers
  state |= _mod->SPIsetRegValue(SX127X_REG_FIFO_TX_BASE_ADDR, SX127X_FIFO_TX_BASE_ADDR_MAX);
  state |= _mod->SPIsetRegValue(SX127X_REG_FIFO_ADDR_PTR, SX127X_FIFO_TX_BASE_ADDR_MAX);
  
  // write packet to FIFO
  _mod->SPIwriteRegisterBurst(SX127X_REG_FIFO, data, len);
  
  // start transmission
  state |= setMode(SX127X_TX);
  if(state != ERR_NONE) {
    return(state);
  }
  
  // wait for packet transmission or timeout
  uint32_t start = millis();
  while(!digitalRead(_mod->int0())) {
    if(millis() - start > timeout) {
      clearIRQFlags();
      return(ERR_TX_TIMEOUT);
    }
  }
  uint32_t elapsed = millis() - start;
  
  // update data rate
  _dataRate = (len*8.0)/((float)elapsed/1000.0);
  
  // clear interrupt flags
  clearIRQFlags();
  
  return(ERR_NONE);
}


int16_t SX127x::receive(uint8_t* data, size_t& len) {
  // set mode to standby
  int16_t state = setMode(SX127X_STANDBY);

  // set DIO pin mapping
  state |= _mod->SPIsetRegValue(SX127X_REG_DIO_MAPPING_1, SX127X_DIO0_RX_DONE | SX127X_DIO1_RX_TIMEOUT, 7, 4);
  
  // clear interrupt flags
  clearIRQFlags();
  
  // set FIFO pointers
  state |= _mod->SPIsetRegValue(SX127X_REG_FIFO_RX_BASE_ADDR, SX127X_FIFO_RX_BASE_ADDR_MAX);
  state |= _mod->SPIsetRegValue(SX127X_REG_FIFO_ADDR_PTR, SX127X_FIFO_RX_BASE_ADDR_MAX);
  
  // set mode to receive
  state |= setMode(SX127X_RXSINGLE);
  if(state != ERR_NONE) {
    return(state);
  }
  
  // wait for packet reception or timeout
  while(!digitalRead(_mod->int0())) {
    if(digitalRead(_mod->int1())) {
      clearIRQFlags();
      return(ERR_RX_TIMEOUT);
    }
  }
  
  // check integrity CRC
  if(_mod->SPIgetRegValue(SX127X_REG_IRQ_FLAGS, 5, 5) == SX127X_CLEAR_IRQ_FLAG_PAYLOAD_CRC_ERROR) {
    return(ERR_CRC_MISMATCH);
  }
  
  // get packet length
  if(_sf != 6) {
    len = _mod->SPIgetRegValue(SX127X_REG_RX_NB_BYTES);
  }
  
  _mod->SPIreadRegisterBurst(SX127X_REG_FIFO, len, data);
  
  // clear interrupt flags
  clearIRQFlags();
  
  return(ERR_NONE);
}


int16_t SX127x::scanChannel() {
  // set mode to standby
  int16_t state = setMode(SX127X_STANDBY);
  
  // set DIO pin mapping
  state |= _mod->SPIsetRegValue(SX127X_REG_DIO_MAPPING_1, SX127X_DIO0_CAD_DONE | SX127X_DIO1_CAD_DETECTED, 7, 4);
  
  // clear interrupt flags
  clearIRQFlags();
  
  // set mode to CAD
  state |= setMode(SX127X_CAD);
  if(state != ERR_NONE) {
    return(state);
  }
  
  // wait for channel activity detected or timeout
  while(!digitalRead(_mod->int0())) {
    if(digitalRead(_mod->int1())) {
      clearIRQFlags();
      return(PREAMBLE_DETECTED);
    }
  }
  
  // clear interrupt flags
  clearIRQFlags();
  
  return(CHANNEL_FREE);
}


int16_t SX127x::sleep() {
  // set mode to sleep
  return(setMode(SX127X_SLEEP));
}


int16_t SX127x::standby() {
  // set mode to standby
  return(setMode(SX127X_STANDBY));
}


int16_t SX127x::setSyncWord(uint8_t syncWord) {
  // set mode to standby
  setMode(SX127X_STANDBY);
  
  // write register 
  return(_mod->SPIsetRegValue(SX127X_REG_SYNC_WORD, syncWord));
}


int16_t SX127x::setCurrentLimit(uint8_t currentLimit) {
  // check allowed range
  if(!(((currentLimit >= 45) && (currentLimit <= 240)) || (currentLimit == 0))) {
    return(ERR_INVALID_CURRENT_LIMIT);
  }
  
  // set mode to standby
  int16_t state = setMode(SX127X_STANDBY);
  
  // set OCP limit
  uint8_t raw;
  if(currentLimit == 0) {
    // limit set to 0, disable OCP
    state |= _mod->SPIsetRegValue(SX127X_REG_OCP, SX127X_OCP_OFF, 5, 5);
  } else if(currentLimit <= 120) {
    raw = (currentLimit - 45) / 5;
    state |= _mod->SPIsetRegValue(SX127X_REG_OCP, SX127X_OCP_ON | raw, 5, 0);
  } else if(currentLimit <= 240) {
    raw = (currentLimit + 30) / 10;
    state |= _mod->SPIsetRegValue(SX127X_REG_OCP, SX127X_OCP_ON | raw, 5, 0);
  }
  return(state);
}


int16_t SX127x::setPreambleLength(uint16_t preambleLength) {
  // check allowed range
  if(preambleLength < 6) {
    return(ERR_INVALID_PREAMBLE_LENGTH);
  }
  
  // set mode to standby
  int16_t state = setMode(SX127X_STANDBY);
  
  // set preamble length
  state |= _mod->SPIsetRegValue(SX127X_REG_PREAMBLE_MSB, (preambleLength & 0xFF00) >> 8);
  state |= _mod->SPIsetRegValue(SX127X_REG_PREAMBLE_LSB, preambleLength & 0x00FF);
  return(state);
}


float SX127x::getFrequencyError(bool autoCorrect) {
  // get raw frequency error
  uint32_t raw = (uint32_t)_mod->SPIgetRegValue(SX127X_REG_FEI_MSB, 3, 0) << 16;
  raw |= _mod->SPIgetRegValue(SX127X_REG_FEI_MID) << 8;
  raw |= _mod->SPIgetRegValue(SX127X_REG_FEI_LSB);
  
  uint32_t base = (uint32_t)2 << 23;
  float error;
  
  // check the first bit
  if(raw & 0x80000) {
    // frequency error is negative
    raw |= (uint32_t)0xFFF00000;
    raw = ~raw + 1;
    error = (((float)raw * (float)base)/32000000.0) * (_bw/500.0) * -1.0;
  } else {
    error = (((float)raw * (float)base)/32000000.0) * (_bw/500.0);
  }
  
  if(autoCorrect) {
    // adjust LoRa modem data rate
    float ppmOffset = 0.95 * (error/32.0);
    _mod->SPIwriteRegister(0x27, (uint8_t)ppmOffset);
  }
  
  return(error);
}


int8_t SX127x::getRSSI() {
  int8_t lastPacketRSSI;
  
  // RSSI calculation uses different constant for low-frequency and high-frequency ports
  if(_freq < 868.0) {
    lastPacketRSSI = -164 + _mod->SPIgetRegValue(SX127X_REG_PKT_RSSI_VALUE);
  } else {
    lastPacketRSSI = -157 + _mod->SPIgetRegValue(SX127X_REG_PKT_RSSI_VALUE);
  }
  
  // spread-spectrum modulation signal can be received below noise floor
  // check last packet SNR and if it's less than 0, add it to reported RSSI to get the correct value
  float lastPacketSNR = SX127x::getSNR();
  if(lastPacketSNR < 0.0) {
    lastPacketRSSI += lastPacketSNR;
  }
  
  return(lastPacketRSSI);
}


float SX127x::getSNR() {
  // get SNR value
  int8_t rawSNR = (int8_t)_mod->SPIgetRegValue(SX127X_REG_PKT_SNR_VALUE);
  return(rawSNR / 4.0);
}


float SX127x::getDataRate() {
  return(_dataRate);
}


int16_t SX127x::setFrequencyRaw(float newFreq) {
  // set mode to standby
  int16_t state = setMode(SX127X_STANDBY);
  
  // calculate register values
  uint32_t base = 1;
  uint32_t FRF = (newFreq * (base << 19)) / 32.0;
  
  // write registers
  state |= _mod->SPIsetRegValue(SX127X_REG_FRF_MSB, (FRF & 0xFF0000) >> 16);
  state |= _mod->SPIsetRegValue(SX127X_REG_FRF_MID, (FRF & 0x00FF00) >> 8);
  state |= _mod->SPIsetRegValue(SX127X_REG_FRF_LSB, FRF & 0x0000FF);
  return(state);
}


int16_t SX127x::config() {
  // turn off frequency hopping
  int16_t state = _mod->SPIsetRegValue(SX127X_REG_HOP_PERIOD, SX127X_HOP_PERIOD_OFF);
  return(state);
}


bool SX127x::findChip(uint8_t ver) {
  uint8_t i = 0;
  bool flagFound = false;
  while((i < 10) && !flagFound) {
    uint8_t version = _mod->SPIreadRegister(SX127X_REG_VERSION);
    if(version == ver) {
      flagFound = true;
    } else {
      #ifdef KITELIB_DEBUG
        Serial.print(F("SX127x not found! ("));
        Serial.print(i + 1);
        Serial.print(F(" of 10 tries) SX127X_REG_VERSION == "));
        
        char buffHex[5];
        sprintf(buffHex, "0x%02X", version);
        Serial.print(buffHex);
        Serial.print(F(", expected 0x00"));
        Serial.print(ver, HEX);
        Serial.println();
      #endif
      delay(1000);
      i++;
    }
  }
  
  return(flagFound);
}


int16_t SX127x::setMode(uint8_t mode) {
  return(_mod->SPIsetRegValue(SX127X_REG_OP_MODE, mode, 2, 0, 5));
}


int16_t SX127x::getActiveModem() {
  return(_mod->SPIgetRegValue(SX127X_REG_OP_MODE, 7, 7));
}


int16_t SX127x::setActiveModem(uint8_t modem) {
  // set mode to SLEEP
  int16_t state = setMode(SX127X_SLEEP);
  
  // set LoRa mode
  state |= _mod->SPIsetRegValue(SX127X_REG_OP_MODE, modem, 7, 7, 5);
  
  // set mode to STANDBY
  state |= setMode(SX127X_STANDBY);
  return(state);
}


void SX127x::clearIRQFlags() {
  _mod->SPIwriteRegister(SX127X_REG_IRQ_FLAGS, 0b11111111);
}


#ifdef KITELIB_DEBUG
void SX127x::regDump() {
  Serial.println();
  Serial.println(F("ADDR\tVALUE"));
  for(uint16_t addr = 0x01; addr <= 0x70; addr++) {
    if(addr <= 0x0F) {
      Serial.print(F("0x0"));
    } else {
      Serial.print(F("0x"));
    }
    Serial.print(addr, HEX);
    Serial.print('\t');
    uint8_t val = _mod->SPIreadRegister(addr);
    if(val <= 0x0F) {
      Serial.print(F("0x0"));
    } else {
      Serial.print(F("0x"));
    }
    Serial.println(val, HEX);
    
    delay(50);
  }
}
#endif
