#pragma once

#ifdef RASPBERRY_PI

#include <stdint.h>
#include <stddef.h>

class SPI {
public:
	// Constructor, device path required
	SPI();
	
	// Set or get the SPI mode
    void set_mode(uint8_t m_mode);
    uint8_t get_mode();
	
	// Set or get the bits per word
    void set_bits_per_word(uint8_t m_bits);
    uint8_t get_bits_per_word();
	
	// Set or get the SPI clock speed
    void set_speed(uint32_t speed);
    uint32_t get_speed();
	
	// Set or get the SPI delay
    void set_delay(uint16_t us);
    uint16_t get_delay();
	
	// Transfer some data
	//	tx:	Array of bytes to be transmitted
	//	rx: Array of bytes to be received
	//	length:	Length of arrays (must be equal)
	// If you just want to send data you still need to pass in
	// an rx array, but you can safely ignore its output
	// Returns true if transfer was successful (false otherwise)
    bool transfer(uint8_t const* tx, uint8_t* rx, size_t length);
	
	// Close the bus
	void close();
private:
    int m_fd = -1;
    uint8_t m_mode = 0;
    uint8_t m_bits = 0;
    uint32_t m_speed = 0;
    uint16_t m_delay = 0;
};

#endif
