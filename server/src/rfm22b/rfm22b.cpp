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

#include "rfm22b.h"

#ifdef RASPBERRY_PI
#   include "rfm22b_spi.h"
#   include <chrono>
#   include <thread>
#	include <stdio.h>
#	include <stdlib.h>
#	include <math.h>
#else
#   include <SPI.h>
#endif


bool RFM22B::init()
{
    // Software reset the device
    reset();

    idle_mode();

    // Get the device type and check it
    // This also tests whether we are really connected to a device
    uint8_t deviceType = get_register(Register::DEVICE_TYPE);
    if (deviceType != (uint8_t)Device_Type::RX_TRX
            && deviceType != (uint8_t)Device_Type::TX)
    {
#ifdef ARDUINO
        Serial.print("Wrong device: ");
        Serial.println(deviceType);
#endif
        return false;
    }

    return true;
}

// Set the frequency of the carrier wave
//	This function calculates the values of the registers 0x75-0x77 to achieve the 
//	desired carrier wave frequency (without any hopping set)
//	Frequency should be passed in integer Hertz
bool RFM22B::set_carrier_frequency(float centre, float afcPullInRange)
{
    uint8_t fbsel = (uint8_t)Frequency_Band_Select::SBSEL;
    uint8_t afclimiter;
    if (centre < 240.0 || centre > 960.0) // 930.0 for early silicon
        return false;
    if (centre >= 480.0)
    {
        if (afcPullInRange < 0.0 || afcPullInRange > 0.318750)
            return false;
        centre /= 2;
        fbsel |= (uint8_t)Frequency_Band_Select::HBSEL;
        afclimiter = afcPullInRange * 1000000.0 / 1250.0;
    }
    else
    {
        if (afcPullInRange < 0.0 || afcPullInRange > 0.159375)
            return false;
        afclimiter = afcPullInRange * 1000000.0 / 625.0;
    }
    centre /= 10.0;
    float integerPart = floor(centre);
    float fractionalPart = centre - integerPart;

    uint8_t fb = (uint8_t)integerPart - 24; // Range 0 to 23
    fbsel |= fb;
    uint16_t fc = fractionalPart * 64000;
    set_register(Register::FREQUENCY_OFFSET_1, 0);  // REVISIT
    set_register(Register::FREQUENCY_OFFSET_2, 0);
    set_register(Register::FREQUENCY_BAND_SELECT, fbsel);
    set_register(Register::NOMINAL_CARRIER_FREQUENCY_1, fc >> 8);
    set_register(Register::NOMINAL_CARRIER_FREQUENCY_0, fc & 0xff);
    set_register(Register::AFC_LIMITER, afclimiter);
    return !(get_register(Register::DEVICE_STATUS) & (uint8_t)Device_Status::FREQERR);
}

// Get the frequency of the carrier wave in integer Hertz
//	Without any frequency hopping
float RFM22B::get_carrier_frequency()
{
    // Read the register values
    uint8_t fbs = get_register(Register::FREQUENCY_BAND_SELECT);
    uint8_t ncf1 = get_register(Register::NOMINAL_CARRIER_FREQUENCY_1);
    uint8_t ncf0 = get_register(Register::NOMINAL_CARRIER_FREQUENCY_0);

    // The following calculations ceom from Section 3.5.1 of the datasheet

    // Determine the integer part
    uint8_t fb = fbs & 0x1F;

    // Are we in the 'High Band'?
    uint8_t hbsel = (fbs >> 5) & 1;

    // Determine the fractional part
    uint16_t fc = (ncf1 << 8) | ncf0;

    // Return the frequency
    return 10.f*(hbsel+1)*(fb+24+fc/64000.f);
}

// Get and set the frequency hopping step size
//	Values are in Hertz (to stay SI) but floored to the nearest 10kHz
void RFM22B::set_frequency_hopping_step_size(uint32_t step)
{
    if (step > uint32_t(2550000))
    {
        step = uint32_t(2550000);
    }
    set_register(Register::FREQUENCY_HOPPING_STEP_SIZE, step/10000);
}
uint32_t RFM22B::get_frequency_hopping_step_size()
{
    return get_register(Register::FREQUENCY_HOPPING_STEP_SIZE)*uint32_t(10000);
}

// Get and set the frequency hopping channel
void RFM22B::set_channel(uint8_t channel)
{
    set_register(Register::FREQUENCY_HOPPING_CHANNEL_SELECT, channel);
}
uint8_t RFM22B::get_channel()
{
    return get_register(Register::FREQUENCY_HOPPING_CHANNEL_SELECT);
}

// Set or get the frequency deviation (in Hz, but floored to the nearest 625Hz)
void RFM22B::set_frequency_deviation(uint32_t deviation)
{
    if (deviation > uint32_t(320000))
    {
        deviation = uint32_t(320000);
    }
    set_register(Register::FREQUENCY_DEVIATION, deviation/625);
}
uint32_t RFM22B::get_frequency_deviation()
{
    return get_register(Register::FREQUENCY_DEVIATION)*625;
}

// Set or get the data rate (bps)
void RFM22B::set_data_rate(uint32_t rate)
{
    // Get the Modulation Mode Control 1 register (for scaling bit)
    uint8_t mmc1 = get_register(Register::MODULATION_MODE_CONTROL_1);

    uint16_t txdr;
    // Set the scale bit (5th bit of 0x70) high if data rate is below 30kbps
    // and calculate the value for txdr registers (0x6E and 0x6F)
    if (rate < uint32_t(30000))
    {
        mmc1 |= (1<<5);
        txdr = rate * ((uint32_t(1) << (16 + 5)) / uint32_t(1E6));
    }
    else
    {
        mmc1 &= ~(1<<5);
        txdr = rate * ((uint32_t(1) << (16)) / uint32_t(1E6));
    }

    // Set the data rate bytes
    set_register16(Register::TX_DATA_RATE_1, txdr);

    // Set the scaling byte
    set_register(Register::MODULATION_MODE_CONTROL_1, mmc1);
}
uint32_t RFM22B::get_data_rate()
{
    // Get the data rate scaling value (5th bit of 0x70)
    uint8_t txdtrtscale = (get_register(Register::MODULATION_MODE_CONTROL_1) >> 5) & 1;

    // Get the data rate registers
    uint16_t txdr = get_register16(Register::TX_DATA_RATE_1);

    // Return the data rate (in bps, hence extra 1E3)
    return ((uint32_t) txdr * uint32_t(1E6)) / (uint32_t(1) << (16 + 5 * txdtrtscale));
}

// Set or get the modulation type
void RFM22B::set_modulation_type(Modulation_Type modulation)
{
    // Get the Modulation Mode Control 2 register
    uint8_t mmc2 = get_register(Register::MODULATION_MODE_CONTROL_2);

    // Clear the modtyp bits
    mmc2 &= ~0x03;

    // Set the desired modulation
    mmc2 |= (uint8_t)modulation;

    // Set the register
    set_register(Register::MODULATION_MODE_CONTROL_2, mmc2);
}
RFM22B::Modulation_Type RFM22B::get_modulation_type()
{
    // Get the Modulation Mode Control 2 register
    uint8_t mmc2 = get_register(Register::MODULATION_MODE_CONTROL_2);

    // Determine modtyp bits
    uint8_t modtyp = mmc2 & 0x03;

    // Ugly way to return correct enum
    switch (modtyp)
    {
    case 1:
        return Modulation_Type::OOK;
    case 2:
        return Modulation_Type::FSK;
    case 3:
        return Modulation_Type::GFSK;
    case 0:
    default:
        return Modulation_Type::UNMODULATED_CARRIER;
    }
}

void RFM22B::set_modulation_data_source(Modulation_Data_Source source)
{
    // Get the Modulation Mode Control 2 register
    uint8_t mmc2 = get_register(Register::MODULATION_MODE_CONTROL_2);

    // Clear the dtmod bits
    mmc2 &= ~(0x03<<4);

    // Set the desired data source
    mmc2 |= (uint8_t)source << 4;

    // Set the register
    set_register(Register::MODULATION_MODE_CONTROL_2, mmc2);
}
RFM22B::Modulation_Data_Source RFM22B::get_modulation_data_source()
{
    // Get the Modulation Mode Control 2 register
    uint8_t mmc2 = get_register(Register::MODULATION_MODE_CONTROL_2);

    // Determine modtyp bits
    uint8_t dtmod = (mmc2 >> 4) & 0x03;

    // Ugly way to return correct enum
    switch (dtmod)
    {
    case 1:
        return Modulation_Data_Source::DIRECT_SDI;
    case 2:
        return Modulation_Data_Source::FIFO;
    case 3:
        return Modulation_Data_Source::PN9;
    case 0:
    default:
        return Modulation_Data_Source::DIRECT_GPIO;
    }
}

void RFM22B::set_data_clock_configuration(Data_Clock_Configuration clock)
{
    // Get the Modulation Mode Control 2 register
    uint8_t mmc2 = get_register(Register::MODULATION_MODE_CONTROL_2);

    // Clear the trclk bits
    mmc2 &= ~(0x03<<6);

    // Set the desired data source
    mmc2 |= (uint8_t)clock << 6;

    // Set the register
    set_register(Register::MODULATION_MODE_CONTROL_2, mmc2);
}
RFM22B::Data_Clock_Configuration RFM22B::get_data_clock_configuration()
{
    // Get the Modulation Mode Control 2 register
    uint8_t mmc2 = get_register(Register::MODULATION_MODE_CONTROL_2);

    // Determine modtyp bits
    uint8_t dtmod = (mmc2 >> 6) & 0x03;

    // Ugly way to return correct enum
    switch (dtmod)
    {
    case 1:
        return Data_Clock_Configuration::GPIO;
    case 2:
        return Data_Clock_Configuration::SDO;
    case 3:
        return Data_Clock_Configuration::NIRQ;
    case 0:
    default:
        return Data_Clock_Configuration::NONE;
    }
}

// Set or get the transmission power
void RFM22B::set_transmission_power(uint8_t power)
{
    // Saturate to maximum power
    if (power > 20)
    {
        power = 20;
    }

    // Get the TX power register
    uint8_t txp = get_register(Register::TX_POWER);

    // Clear txpow bits
    txp &= ~(0x07);

    // Calculate txpow bits (See Section 5.7.1 of datasheet)
    uint8_t txpow = (power + 1) / 3;

    // Set txpow bits
    txp |= txpow;

    // Set the register
    set_register(Register::TX_POWER, txp);
}
uint8_t RFM22B::get_transmission_power()
{
    // Get the TX power register
    uint8_t txp = get_register(Register::TX_POWER);

    // Get the txpow bits
    uint8_t txpow = txp & 0x07;

    // Calculate power (see Section 5.7.1 of datasheet)
    if (txpow == 0)
    {
        return 1;
    }
    else
    {
        return txpow * 3 - 1;
    }
}

// Set or get the GPIO configuration
void RFM22B::set_gpio_function(GPIO gpio, GPIO_Function func)
{
    // Get the GPIO register
    uint8_t gpioX = get_register((Register)gpio);

    // Clear gpioX bits
    gpioX &= ~((1<<5)-1);

    // Set the gpioX bits
    gpioX |= (uint8_t)func;

    // Set the register
    set_register((Register)gpio, gpioX);
}

uint8_t RFM22B::get_gpio_function(GPIO gpio)
{
    // Get the GPIO register
    uint8_t gpioX = get_register((Register)gpio);

    // Return the gpioX bits
    // This should probably return an enum, but this needs a lot of cases
    return gpioX & ((1<<5)-1);
}

// Enable or disable interrupts
void RFM22B::set_interrupt_enable(Interrupt interrupt, bool enable)
{
    // Get the (16 bit) register value
    uint16_t intEnable = get_register16(Register::INTERRUPT_ENABLE_1);

    // Either enable or disable the interrupt
    if (enable)
    {
        intEnable |= (uint8_t)interrupt;
    }
    else
    {
        intEnable &= ~(uint8_t)interrupt;
    }

    // Set the (16 bit) register value
    set_register16(Register::INTERRUPT_ENABLE_1, intEnable);
}

// Get the status of an interrupt
bool RFM22B::get_interrupt_status(Interrupt interrupt)
{
    // Get the (16 bit) register value
    uint16_t intStatus = get_register16(Register::INTERRUPT_STATUS_1);

    // Determine if interrupt bit is set and return
    if ((intStatus & (uint8_t)interrupt) > 0)
    {
        return true;
    }
    else
    {
        return false;
    }
}

// Set the operating mode
//	This function does not toggle individual pins as with other functions
//	It expects a bitwise-ORed combination of the modes you want set
void RFM22B::set_operating_mode(uint16_t mode)
{
    set_register16(Register::OPERATING_MODE_AND_FUNCTION_CONTROL_1, mode);
}

// Get operating mode (bitwise-ORed)
uint16_t RFM22B::get_operating_moed()
{
    return get_register16(Register::OPERATING_MODE_AND_FUNCTION_CONTROL_1);
}

// Manuall enter RX or TX mode
void RFM22B::rx_mode()
{
    set_operating_mode((uint16_t)Operating_Mode::READY_MODE | (uint16_t)Operating_Mode::RX_MODE);
}
void RFM22B::tx_mode()
{
    set_operating_mode((uint16_t)Operating_Mode::READY_MODE | (uint16_t)Operating_Mode::TX_MODE);
}

// Reset the device
void RFM22B::reset()
{
    set_operating_mode((uint16_t)Operating_Mode::READY_MODE | (uint16_t)Operating_Mode::RESET);
#ifdef RASPBERRY_PI
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
#else
    delay(10);
#endif
}

void RFM22B::idle_mode()
{
    set_operating_mode((uint16_t)Operating_Mode::READY_MODE);
}

void RFM22B::sleep_mode()
{
    set_operating_mode((uint16_t)Operating_Mode::ENABLE_WAKE_UP_TIMER);
}

void RFM22B::stand_by_mode()
{
    get_register(Register::INTERRUPT_STATUS_1);
    get_register(Register::INTERRUPT_STATUS_2);
    set_operating_mode(0);
}

// Set or get the trasmit header
void RFM22B::set_transmit_header(uint32_t header)
{
    set_register32(Register::TRANSMIT_HEADER_3, header);
}
uint32_t RFM22B::get_transmit_header()
{
    return get_register32(Register::TRANSMIT_HEADER_3);
}

// Set or get the check header
void RFM22B::set_check_header(uint32_t header)
{
    set_register32(Register::CHECK_HEADER_3, header);
}
uint32_t RFM22B::get_check_header()
{
    return get_register32(Register::CHECK_HEADER_3);
}

// Set or get the CRC mode
void RFM22B::set_crc_mode(CRC_Mode mode)
{
    uint8_t dac = get_register(Register::DATA_ACCESS_CONTROL);

    dac &= ~0x24;

    switch (mode)
    {
    case CRC_Mode::CRC_DISABLED:
        break;
    case CRC_Mode::CRC_DATA_ONLY:
        dac |= 0x24;
        break;
    case CRC_Mode::CRC_NORMAL:
    default:
        dac |= 0x04;
        break;
    }

    set_register(Register::DATA_ACCESS_CONTROL, dac);
}
RFM22B::CRC_Mode RFM22B::get_crc_mode()
{
    uint8_t dac = get_register(Register::DATA_ACCESS_CONTROL);

    if (! (dac & 0x04))
    {
        return CRC_Mode::CRC_DISABLED;
    }
    if (dac & 0x20)
    {
        return CRC_Mode::CRC_DATA_ONLY;
    }
    return CRC_Mode::CRC_NORMAL;
}

// Set or get the CRC polynomial
void RFM22B::set_crc_polynomial(CRC_Polynomial poly)
{
    uint8_t dac = get_register(Register::DATA_ACCESS_CONTROL);

    dac &= ~0x03;

    dac |= (uint8_t)poly;

    set_register(Register::DATA_ACCESS_CONTROL, dac);
}
RFM22B::CRC_Polynomial RFM22B::get_crc_polynomial()
{
    uint8_t dac = get_register(Register::DATA_ACCESS_CONTROL);

    switch (dac & 0x03)
    {
    case 0:
        return CRC_Polynomial::CCITT;
    case 1:
        return CRC_Polynomial::CRC16;
    case 2:
        return CRC_Polynomial::IEC16;
    case 3:
        return CRC_Polynomial::BIACHEVA;
    }
    return CRC_Polynomial::CRC16;
}

// Get and set all the FIFO threshold
void RFM22B::set_tx_fifo_almost_full_threshold(uint8_t thresh)
{
    set_fifo_threshold(Register::TX_FIFO_CONTROL_1, thresh);
}
void RFM22B::set_tx_fifo_almost_empty_threshold(uint8_t thresh)
{
    set_fifo_threshold(Register::TX_FIFO_CONTROL_2, thresh);
}
void RFM22B::set_rx_fifo_almost_full_threshold(uint8_t thresh)
{
    set_fifo_threshold(Register::RX_FIFO_CONTROL, thresh);
}
uint8_t RFM22B::get_tx_fifo_almost_full_threshold()
{
    return get_register(Register::TX_FIFO_CONTROL_1);
}
uint8_t RFM22B::get_tx_fifo_almost_empty_threshold()
{
    return get_register(Register::TX_FIFO_CONTROL_2);
}
uint8_t RFM22B::get_rx_fifo_almost_full_threshold()
{
    return get_register(Register::RX_FIFO_CONTROL);
}
void RFM22B::set_fifo_threshold(Register reg, uint8_t thresh)
{
    thresh &= ((1 << 6) - 1);
    set_register(reg, thresh);
}

// Get RSSI value
uint8_t RFM22B::get_rssi()
{
    return get_register(Register::RECEIVED_SIGNAL_STRENGTH_INDICATOR);
}
// Get input power (in dBm)
//	Coefficients approximated from the graph in Section 8.10 of the datasheet
int8_t RFM22B::get_input_power()
{
    return 0.56*get_rssi()-128.8;
}

// Get length of last received packet
uint8_t RFM22B::get_received_packet_length()
{
    return get_register(Register::RECEIVED_PACKET_LENGTH);
}

// Set length of packet to be transmitted
void RFM22B::set_transmit_packet_length(uint8_t length)
{
    return set_register(Register::TRANSMIT_PACKET_LENGTH, length);
}

void RFM22B::clear_rx_fifo()
{
    //Toggle ffclrrx bit high and low to clear RX FIFO
    set_register(Register::OPERATING_MODE_AND_FUNCTION_CONTROL_2, 2);
    set_register(Register::OPERATING_MODE_AND_FUNCTION_CONTROL_2, 0);
}

void RFM22B::clear_tx_fifo()
{
    //Toggle ffclrtx bit high and low to clear TX FIFO
    set_register(Register::OPERATING_MODE_AND_FUNCTION_CONTROL_2, 1);
    set_register(Register::OPERATING_MODE_AND_FUNCTION_CONTROL_2, 0);
}

// Send data
void RFM22B::send(uint8_t *data, uint8_t length)
{
    // Clear TX FIFO
    clear_tx_fifo();

    // Initialise rx and tx arrays
    uint8_t tx[MAX_PACKET_LENGTH+1] = { 0 };

    // Set FIFO register address (with write flag)
    tx[0] = (uint8_t)Register::FIFO_ACCESS | (1<<7);

    // Truncate data if its too long
    if (length > MAX_PACKET_LENGTH)
    {
        length = MAX_PACKET_LENGTH;
    }

    // Copy data from input array to tx array
    for (uint8_t i = 1; i <= length; i++)
    {
        tx[i] = data[i-1];
    }

    // Set the packet length
    set_transmit_packet_length(length);

    // Make the transfer
    transfer(tx, length+1);

    // Enter TX mode
    tx_mode();

    // Loop until packet has been sent (device has left TX mode)
    while (((get_register(Register::OPERATING_MODE_AND_FUNCTION_CONTROL_1)>>3) & 1))
    {}

    return;
};

// Receive data (blocking with timeout). Returns number of bytes received
int8_t RFM22B::receive(uint8_t *data, uint8_t length, uint32_t timeout)
{
    // Make sure RX FIFO is empty, ready for new data
    clear_rx_fifo();

    // Enter RX mode
    rx_mode();

    // Initialise rx and tx arrays
    uint8_t buffer[MAX_PACKET_LENGTH+1] = { 0 };

    // Set FIFO register address
    buffer[0] = (uint8_t)Register::FIFO_ACCESS;

    // Timing for the interrupt loop timeout
#ifdef RASPBERRY_PI
    auto start = std::chrono::high_resolution_clock::now();
    uint32_t elapsed = 0;
#else
    uint32_t start = millis();
    uint32_t elapsed = 0;
#endif

    // Loop endlessly on interrupt or timeout
    //	Don't use interrupt registers here as these don't seem to behave consistently
    //	Watch the operating mode register for the device leaving RX mode. This is indicitive
    //	of a valid packet being received
    while (((get_register(Register::OPERATING_MODE_AND_FUNCTION_CONTROL_1)>>2) & 1) && elapsed < timeout)
    {
        // Determine elapsed time
#ifdef RASPBERRY_PI
        elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start).count();
#else
        elapsed = millis() - start;
#endif
    }

    // If timeout occured, return -1
    if (elapsed >= timeout)
    {
        return -1;
    }

    // Get length of packet received
    uint8_t rxLength = get_received_packet_length();

    if (rxLength > length)
    {
        rxLength = length;
    }

    // Make the transfer
    transfer(buffer, rxLength+1);

    // Copy the data to the output array
    for (uint8_t i = 1; i <= rxLength; i++)
    {
        data[i-1] = buffer[i];
    }

    return rxLength;
};

// Helper function to read a single byte from the device
uint8_t RFM22B::get_register(Register reg)
{
    // rx and tx arrays must be the same length
    // Must be 2 elements as the device only responds whilst it is being sent
    // data. tx[0] should be set to the requested register value and tx[1] left
    // clear. Once complete, rx[0] will be left clear (no data was returned whilst
    // the requested register was being sent), and rx[1] will contain the value
    uint8_t buffer[] = {uint8_t(reg), 0x00};
    transfer(buffer, 2);
    return buffer[1];
}

// Similar to function above, but for readying 2 consequtive registers as one
uint16_t RFM22B::get_register16(Register reg)
{
    uint8_t buffer[] = {uint8_t(reg), 0x00, 0x00};
    transfer(buffer,3);
    return (buffer[1] << 8) | buffer[2];
}

// Similar to function above, but for readying 4 consequtive registers as one
uint32_t RFM22B::get_register32(Register reg)
{
    uint8_t buffer[] = {uint8_t(reg), 0x00, 0x00, 0x00, 0x00};
    transfer(buffer,5);
    return (buffer[1] << 24) | (buffer[2] << 16) | (buffer[3] << 8) | buffer[4];
}

// Helper function to write a single byte to a register
void RFM22B::set_register(Register reg, uint8_t value)
{
    uint8_t buffer[] = {uint8_t(uint8_t(reg) | (1 << 7)), value};
    transfer(buffer, 2);
}

// As above, but for 2 consequitive registers
void RFM22B::set_register16(Register reg, uint16_t value)
{
    uint8_t buffer[] = {uint8_t(uint8_t(reg) | (1 << 7)), uint8_t(value >> 8), uint8_t(value & 0xFF)};
    transfer(buffer, 3);
}

// As above, but for 4 consequitive registers
void RFM22B::set_register32(Register reg, uint32_t value)
{
    uint8_t buffer[] = {uint8_t(uint8_t(reg) | (1 << 7)), uint8_t(value >> 24), uint8_t((value >> 16) & 0xFF), uint8_t((value >> 8) & 0xFF), uint8_t(value & 0xFF)};
    transfer(buffer,5);
}

void RFM22B::transfer(uint8_t* data, size_t size)
{
#ifdef RASPBERRY_PI
    m_spi.transfer(data, data, size);
#else
    digitalWrite(SS, LOW);
    SPI.transfer(data, size);
    digitalWrite(SS, HIGH);
#endif
}