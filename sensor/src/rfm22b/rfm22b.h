/* Copyright (c) 2013 Owen McAree
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* Class to control the HopeRF RFM22B module
 *	Inherits SPI functionality from SPI class
 *	This class is a work in progress, at present all it can do is get/set the carrier
 *	frequency. This has been tested on the 868MHz modules but should work for all
 *	frequencies (High or Low bands 24-47, See Table 12 of the datasheet).
 */
 
#ifndef rfm22b_h
#define rfm22b_h

#include <stdint.h>
#include <stdio.h>

#ifdef RASPBERRY_PI
#   include "rfm22b_spi.h"
#endif

class RFM22B {
public:
	#include "rfm22b_enums.h"

	// Constructor requires SPI device path, passes this is SPI class
	RFM22B() {}
	
	bool init();

	// Set or get the carrier frequency (in Hz);
	bool setCarrierFrequency(float center, float afcPullInRange = 0.05);
	float getCarrierFrequency();
	
	// Set or get the frequency hopping step size (in Hz, but it is floored to nearest 10kHz)
	void setFrequencyHoppingStepSize(uint32_t step);
	uint32_t getFrequencyHoppingStepSize();
	
	// Set or get the frequency hopping channel
	void setChannel(uint8_t channel);
	uint8_t getChannel();
	
	// Set or get the frequency deviation (in Hz, but floored to the nearest 625Hz)
	void setFrequencyDeviation(uint32_t deviation);
	uint32_t getFrequencyDeviation();
	
	// Set or get the TX data rate (bps)
	// NOTE: This does NOT configure the receive data rate! To properly set
	// up the device for receiving, use the magic register values
	// calculated using the Si443x-Register-Settings_RevB1.xls Excel sheet.
	void setDataRate(uint32_t rate);
	uint32_t getDataRate();
	
	// Set or get the modulation type
	void setModulationType(Modulation_Type modulation);
	Modulation_Type getModulationType();
	
	// Set or get the modulation data source
	void setModulationDataSource(Modulation_Data_Source source);
	Modulation_Data_Source getModulationDataSource();
	
	// Set or get the data clock source
	void setDataClockConfiguration(Data_Clock_Configuration clock);
	Data_Clock_Configuration getDataClockConfiguration();
	
	// Set or get the transmission power
	void setTransmissionPower(uint8_t power);
	uint8_t getTransmissionPower();
	
	// Set or get the GPIO configuration
	void setGPIOFunction(GPIO gpio, GPIO_Function funct);
	// This should probably return enum, but this needs a lot of cases
	uint8_t getGPIOFunction(GPIO gpio);
	
	// Enable or disable interrupts
	// No ability to get interrupt enable status as this would need a lot of case statements
	void setInterruptEnable(Interrupt interrupt, bool enable);
	
	// Get the status of an interrupt
	bool getInterruptStatus(Interrupt interrupt);
	
	// Set the operating mode
	//	This function does not toggle individual pins as with other functions
	//	It expects a bitwise-ORed combination of the modes you want set
	void setOperatingMode(uint16_t mode);
	
	// Get operating mode (bitwise-ORed)
	uint16_t getOperatingMode();
	
	// Manually enable RX or TX modes
	void enableRXMode();
	void enableTXMode();
	
	void reset();
	void idleMode();
	void sleepMode();
	void standByMode();
	
	// Set or get the trasmit header
	void setTransmitHeader(uint32_t header);
	uint32_t getTransmitHeader();
	
	// Set or get the check header
	void setCheckHeader(uint32_t header);
	uint32_t getCheckHeader();

	// Set or get the CRC mode
	void setCRCMode(CRC_Mode mode);
	CRC_Mode getCRCMode();
	
	// Set or get the CRC polynominal
	void setCRCPolynomial(CRC_Polynomial poly);
	CRC_Polynomial getCRCPolynomial();
	
	// Get and set all the FIFO threshold
	void setTXFIFOAlmostFullThreshold(uint8_t thresh);
	void setTXFIFOAlmostEmptyThreshold(uint8_t thresh);
	void setRXFIFOAlmostFullThreshold(uint8_t thresh);
	uint8_t getTXFIFOAlmostFullThreshold();
	uint8_t getTXFIFOAlmostEmptyThreshold();
	uint8_t getRXFIFOAlmostFullThreshold();
	
	// Get RSSI value or input dBm
	uint8_t getRSSI();
	int8_t getInputPower();
	
	// Get length of last received packet
	uint8_t getReceivedPacketLength();
	
	// Set length of packet to be transmitted
	void setTransmitPacketLength(uint8_t length);
	
	// Clear the FIFOs
	void clearRXFIFO();
	void clearTXFIFO();
	
	// Send data
	void send(uint8_t *data, uint8_t length);
	
	// Receive data (blocking with timeout). Returns number of bytes received
	int8_t receive(uint8_t *data, uint8_t length, uint32_t timeout=30000);
	
	// Helper functions for getting and getting individual registers
	uint8_t getRegister(Register reg);
	uint16_t get16BitRegister(Register reg);
	uint32_t get32BitRegister(Register reg);
	void setRegister(Register reg, uint8_t value);	
	void set16BitRegister(Register reg, uint16_t value);
	void set32BitRegister(Register reg, uint32_t value);
	
	static const uint8_t MAX_PACKET_LENGTH = 64;
private:
	
	void setFIFOThreshold(Register reg, uint8_t thresh);

    void transfer(uint8_t* data, size_t size);

#ifdef RASPBERRY_PI
    SPI m_spi;
#endif

};
#endif
