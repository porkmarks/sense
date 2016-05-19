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

#pragma once

#include <stdint.h>
#include <stdio.h>

#ifdef RASPBERRY_PI
#   include "rfm22b_spi.h"
#endif

class RFM22B
{
public:
#include "rfm22b_enums.h"

    // Constructor requires SPI device path, passes this is SPI class
    RFM22B() = default;

    bool init();

    void set_preamble_length(uint8_t nibbles);
    void set_sync_words(uint8_t const* ptr, uint8_t size);

    // Set or get the carrier frequency (in Hz);
    bool set_carrier_frequency(float center, float afcPullInRange = 0.05);
    float get_carrier_frequency();

    // Set or get the frequency hopping step size (in Hz, but it is floored to nearest 10kHz)
    void set_frequency_hopping_step_size(uint32_t step);
    uint32_t get_frequency_hopping_step_size();

    // Set or get the frequency hopping channel
    void set_channel(uint8_t channel);
    uint8_t get_channel();

    // Set or get the frequency deviation (in Hz, but floored to the nearest 625Hz)
    void set_frequency_deviation(uint32_t deviation);
    uint32_t get_frequency_deviation();

    // Set or get the TX data rate (bps)
    // NOTE: This does NOT configure the receive data rate! To properly set
    // up the device for receiving, use the magic register values
    // calculated using the Si443x-Register-Settings_RevB1.xls Excel sheet.
    void set_data_rate(uint32_t rate);
    uint32_t get_data_rate();

    // Set or get the modulation type
    void set_modulation_type(Modulation_Type modulation);
    Modulation_Type get_modulation_type();

    // Set or get the modulation data source
    void set_modulation_data_source(Modulation_Data_Source source);
    Modulation_Data_Source get_modulation_data_source();

    // Set or get the data clock source
    void set_data_clock_configuration(Data_Clock_Configuration clock);
    Data_Clock_Configuration get_data_clock_configuration();

    // Set or get the transmission power
    void set_transmission_power(uint8_t power);
    uint8_t get_transmission_power();

    // Set or get the GPIO configuration
    void set_gpio_function(GPIO gpio, GPIO_Function funct);
    // This should probably return enum, but this needs a lot of cases
    uint8_t get_gpio_function(GPIO gpio);

    // Enable or disable interrupts
    // No ability to get interrupt enable status as this would need a lot of case statements
    void set_interrupt_enable(Interrupt interrupt, bool enable);

    // Get the status of an interrupt
    bool get_interrupt_status(Interrupt interrupt);

    // Set the operating mode
    //	This function does not toggle individual pins as with other functions
    //	It expects a bitwise-ORed combination of the modes you want set
    void set_operating_mode(uint16_t mode);

    // Get operating mode (bitwise-ORed)
    uint16_t get_operating_moed();

    void reset();
    void rx_mode();
    void tx_mode();
    void idle_mode();
    void sleep_mode();
    void stand_by_mode();

    // Set or get the trasmit header
    void set_transmit_header(uint32_t header);
    uint32_t get_transmit_header();

    // Set or get the check header
    void set_check_header(uint32_t header);
    uint32_t get_check_header();

    // Set or get the CRC mode
    void set_crc_mode(CRC_Mode mode);
    CRC_Mode get_crc_mode();

    // Set or get the CRC polynominal
    void set_crc_polynomial(CRC_Polynomial poly);
    CRC_Polynomial get_crc_polynomial();

    // Get and set all the FIFO threshold
    void set_tx_fifo_almost_full_threshold(uint8_t thresh);
    void set_tx_fifo_almost_empty_threshold(uint8_t thresh);
    void set_rx_fifo_almost_full_threshold(uint8_t thresh);
    uint8_t get_tx_fifo_almost_full_threshold();
    uint8_t get_tx_fifo_almost_empty_threshold();
    uint8_t get_rx_fifo_almost_full_threshold();

    // Get RSSI value or input dBm
    uint8_t get_rssi();
    int8_t get_input_power();

    // Get length of last received packet
    uint8_t get_received_packet_length();

    // Set length of packet to be transmitted
    void set_transmit_packet_length(uint8_t length);

    // Clear the FIFOs
    void clear_rx_fifo();
    void clear_tx_fifo();

    // Send data
    bool send(uint8_t *data, uint8_t length, uint32_t timeout = 200);

    // Receive data (blocking with timeout). Returns number of bytes received
    int8_t receive(uint8_t *data, uint8_t length, uint32_t timeout = 30000);

    // Helper functions for getting and getting individual registers
    uint8_t get_register(Register reg);
    uint16_t get_register16(Register reg);
    uint32_t get_register32(Register reg);
    void set_register(Register reg, uint8_t value);
    void set_register16(Register reg, uint16_t value);
    void set_register32(Register reg, uint32_t value);

    static const uint8_t MAX_PACKET_LENGTH = 64;
private:

    void set_fifo_threshold(Register reg, uint8_t thresh);

    void transfer(uint8_t* data, size_t size);

#ifdef RASPBERRY_PI
    SPI m_spi;
#endif

};
