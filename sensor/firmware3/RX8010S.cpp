#include <inttypes.h>
#include <stdio.h>
#include <avr/pgmspace.h>

#include "RX8010S.h"
#include "i2c.h"
#include "Chrono.h"
#include "Arduino_Compat.h"
#include "Log.h"

#define Address 0x32

#define REG_SEC_10            0x10
#define REG_MIN_11            0x11
#define REG_HOUR_12           0x12
#define REG_WEEK_13           0x13
#define REG_DAY_14            0x14
#define REG_MONTH_15          0x15
#define REG_YEAR_16           0x16

#define REG_RESERVED_17       0x17
#define REG_TIMER_COUNTER_1B  0x1B
#define REG_TIMER_COUNTER_1C  0x1C
#define REG_EXTENSION_1D      0x1D
#define REG_FLAGS_1E          0x1E
#define REG_CONTROL_1F        0x1F
#define REG_RESERVED_30       0x30
#define REG_RESERVED_31       0x31
#define REG_IRQ_32            0x32

RX8010S::RX8010S()
{
  
}
bool RX8010S::init()
{
  chrono::delay(chrono::millis(40));

  //reset
  uint8_t dummy;
  i2c_read_register(Address, REG_RESERVED_17, dummy);
/*  
  if (!i2c_write_register(Address, 0x1F, 0x00) ||
      !i2c_write_register(Address, 0x1F, 0x80) ||
      !i2c_write_register(Address, 0x60, 0xD3) ||
      !i2c_write_register(Address, 0x66, 0x03) ||
      !i2c_write_register(Address, 0x6B, 0x02) ||
      !i2c_write_register(Address, 0x6B, 0x01))
  {
    LOG(PSTR("err2\n"));
//    return false;
  }
*/

  //write reserved bits
  if (!i2c_write_register(Address, REG_RESERVED_17, 0xD8) ||
      !i2c_write_register(Address, REG_RESERVED_30, 0x0) ||
      !i2c_write_register(Address, REG_RESERVED_31, 0x08) ||
      !i2c_write_register(Address, REG_CONTROL_1F, 0x44) ||
      !i2c_write_register(Address, REG_IRQ_32, 0x0))
  {
    //LOG(PSTR("err3\n"));
    return false;
  }

  chrono::delay(chrono::millis(10));

  if (!i2c_write_register(Address, REG_EXTENSION_1D, 0x0) ||
      !i2c_write_register(Address, REG_FLAGS_1E, 0x0)) //clear VLF
  {
    //LOG(PSTR("err4\n"));
    return false;
  }

  uint8_t tries = 0;
  uint8_t flags = 0;

  tries = 0;
  flags = 0;
  do
  {
    if (!i2c_read_register(Address, REG_FLAGS_1E, flags))
    {
      //LOG(PSTR("err5\n"));
      return false;
    }
    LOG(PSTR("%d flags:%d\n"), int(tries), int(flags));
    if ((flags & bit(1)) == 0)
    {
      break;
    }
    if (!i2c_write_register(Address, REG_FLAGS_1E, 0x0)) //clear VLF
    {
      return false;
    }
    chrono::delay(chrono::millis(1));
  } while (tries++ < 100);
  
  return tries < 100;
}

bool RX8010S::setAlarm(chrono::millis duration)
{
  return true;
}

bool RX8010S::enablePeriodicTimer(Frequency freq, uint16_t cycles, chrono::micros& actualDuration)
{
  uint8_t tiex = 0;
  if (!i2c_read_register(Address, REG_EXTENSION_1D, tiex))
  {
    return false;
  }
  //LOG(PSTR("tiex:%d\n"), int(tiex));
  uint8_t ctrl = 0;
  if (!i2c_read_register(Address, REG_CONTROL_1F, ctrl))
  {
    return false;
  }
  //LOG(PSTR("ctrl:%d\n"), int(ctrl));
  uint8_t irq = 0;
  if (!i2c_read_register(Address, REG_IRQ_32, irq))
  {
    return false;
  }
  //LOG(PSTR("irq:%d\n"), int(irq));
  uint8_t flags = 0;
  if (!i2c_read_register(Address, REG_FLAGS_1E, flags))
  {
    return false;
  }
  //LOG(PSTR("flags:%d\n"), int(flags));

  ctrl &= ~bit(6); //stop = 0
  ctrl &= ~bit(2); //tstp = 0
  ctrl |= bit(4); //activate irq
  
  irq &= ~bit(2); //irq2

  //disable timer and set the freq
  tiex &= ~bit(4);
  tiex &= ~7; //clear the freq bits
  tiex |= uint8_t(freq);

  flags &= ~bit(4); //clear TF

//  LOG(PSTR("tiex:%d\n"), int(tiex));
//  LOG(PSTR("ctrl:%d\n"), int(ctrl));
//  LOG(PSTR("irq:%d\n"), int(irq));
//  LOG(PSTR("flags:%d\n"), int(flags));

  //send the extension register and the cycles
  if (!i2c_write_register(Address, REG_EXTENSION_1D, tiex) ||
      !i2c_write_register(Address, REG_FLAGS_1E, flags) ||
      !i2c_write_register(Address, REG_CONTROL_1F, ctrl) ||
      !i2c_write_register(Address, REG_IRQ_32, irq) ||
      !i2c_write_register16(Address, REG_TIMER_COUNTER_1B, cycles))
  {
    return false;  
  }

  uint16_t div[] = { 4096, 64, 1 };
  actualDuration = chrono::micros(1000000 * cycles / uint32_t(div[uint8_t(freq)]));

  //enable the timer
  tiex |= bit(4);

  //send the extension register and the cycles
  return i2c_write_register(Address, REG_EXTENSION_1D, tiex);
}

bool RX8010S::disablePeriodicTimer()
{
  uint8_t tiex = 0;
  if (!i2c_read_register(Address, REG_EXTENSION_1D, tiex))
  {
    return false;
  }
  //disable timer
  tiex &= ~bit(4);
  //send the extension register and the cycles
  return i2c_write_register(Address, REG_EXTENSION_1D, tiex);
}

static uint8_t bcd2dec(uint8_t hex)
{
    uint8_t dec = ((hex & 0xF0) >> 4) * 10 + (hex & 0x0F);
    return dec;
}

static uint8_t dec2bcd(uint8_t dec)
{
    uint8_t ones = dec % 10;
    uint8_t temp = dec / 10;
    //I used modulus here to get rid of the improper display issue
    uint8_t tens = (temp % 10) << 4; 
    return (tens + ones);
}


bool RX8010S::getTime(tm& time)
{
  uint8_t data[7];
  if (!i2c_read_registers(Address, REG_SEC_10, data, 7))
  {
    return false;
  }
  time.tm_sec = bcd2dec(data[0]);
  time.tm_min = bcd2dec(data[1]);
  time.tm_hour = bcd2dec(data[2]);
  //time.tm_wday = bcd2dec(data[3]);
  time.tm_mday = bcd2dec(data[4]);
  time.tm_mon = bcd2dec(data[5]);
  time.tm_year = bcd2dec(data[6]) + 100;
  time.tm_isdst = 0;
  return true;
}

bool RX8010S::getTime(time_t& time)
{
  tm t;
  if (!getTime(t))
  {
    return false;
  }
  time = mk_gmtime(&t);
  return true;
}

bool RX8010S::setTime(const tm& time)
{
  uint8_t ctrl = 0;
  if (!i2c_read_register(Address, REG_CONTROL_1F, ctrl))
  {
    return false;
  }
  
  uint8_t data[7];
  data[0] = dec2bcd(time.tm_sec);
  data[1] = dec2bcd(time.tm_min);
  data[2] = dec2bcd(time.tm_hour);
  data[3] = dec2bcd(week_of_year(&time, MONDAY));
  data[4] = dec2bcd(time.tm_mday);
  data[5] = dec2bcd(time.tm_mon);
  data[6] = dec2bcd(time.tm_year - 100);
  return i2c_write_register(Address, REG_CONTROL_1F, ctrl | bit(6)) && //stop the clock
        i2c_write_registers(Address, REG_SEC_10, data, 7) && //set date/time
        i2c_write_register(Address, REG_CONTROL_1F, ctrl & (~bit(6))); //start the clock
}

bool RX8010S::setTime(const time_t& time)
{
  return setTime(*gmtime(&time));
}
