#include "rfm22b_spi.h"

#ifdef RASPBERRY_PI

#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>

// Constructor
//	Opens the SPI device and sets up some default values
//	Default bits per word is 8
//	Default clock speed is 10kHz
SPI::SPI() {
    const char* device = "/dev/spidev1.0";
    m_fd = open(device, O_RDWR);
    if (m_fd < 0) {
        perror("Unable to open SPI device");
    }
    set_mode(0);
    set_bits_per_word(8);
    set_speed(1000000);
    set_delay(0);
}

// Set the mode of the bus (see linux/spi/spidev.h)
void SPI::set_mode(uint8_t mode) {
    int ret = ioctl(m_fd, SPI_IOC_WR_MODE, &mode);
    if (ret == -1)
        perror("Unable to set SPI mode");

    ret = ioctl(m_fd, SPI_IOC_RD_MODE, &m_mode);
    if (ret == -1)
        perror("Unable to get SPI mode");
}

// Get the mode of the bus
uint8_t SPI::get_mode() {
    return m_mode;
}

// Set the number of bits per word
void SPI::set_bits_per_word(uint8_t bits) {
    int ret = ioctl(m_fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
    if (ret == -1)
        perror("Unable to set bits per word");

    ret = ioctl(m_fd, SPI_IOC_RD_BITS_PER_WORD, &m_bits);
    if (ret == -1)
        perror("Unable to get bits per word");
}

// Get the number of bits per word
uint8_t SPI::get_bits_per_word() {
    return m_bits;
}

// Set the bus clock speed
void SPI::set_speed(uint32_t speed) {
    int ret = ioctl(m_fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
    if (ret == -1)
        perror("Unable to set max speed Hz");

    ret = ioctl(m_fd, SPI_IOC_RD_MAX_SPEED_HZ, &m_speed);
    if (ret == -1)
        perror("Unable to get max speed Hz");
}

// Get the bus clock speed
uint32_t SPI::get_speed() {
    return m_speed;
}

// Set the bus delay
void SPI::set_delay(uint16_t delay) {
    m_delay = delay;
}

// Get the bus delay
uint16_t SPI::get_delay() {
    return m_delay;
}

// Transfer some data
bool SPI::transfer(uint8_t *tx, uint8_t *rx, size_t length) {
    struct spi_ioc_transfer tr;
    memset(&tr, 0, sizeof(spi_ioc_transfer));

    tr.tx_buf = (unsigned long)tx;	//tx and rx MUST be the same length!
    tr.rx_buf = (unsigned long)rx;
    tr.len = length;
    tr.delay_usecs = m_delay;
    tr.speed_hz = m_speed;
    tr.bits_per_word = m_bits;

    int ret = ioctl(m_fd, SPI_IOC_MESSAGE(1), &tr);
    if (ret < 0)
    {
        printf("transfer failed: %s\n", strerror(errno));
        return false;
    }
    return true;
}

// Close the bus
void SPI::close() {
    ::close(m_fd);
}

#endif
