#include "i2c.h"

#define I2C_HARDWARE 1
#define I2C_CPUFREQ 921600
#define I2C_FASTMODE 0
#define I2C_SLOWMODE 1
#define I2C_PULLUP 0
#define I2C_NOINTERRUPT 0
#define I2C_TIMEOUT 1000

#include "SoftI2CMaster.h"

void i2c_shutdown(void)
{
  // turn off I2C
  TWCR0 &= ~(bit(TWEN) | bit(TWIE) | bit(TWEA));  
}


