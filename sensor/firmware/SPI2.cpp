/*
 * Copyright (c) 2010 by Cristian Maglie <c.maglie@arduino.cc>
 * Copyright (c) 2014 by Paul Stoffregen <paul@pjrc.com> (Transaction API)
 * Copyright (c) 2014 by Matthijs Kooijman <matthijs@stdin.nl> (SPISettings AVR)
 * Copyright (c) 2014 by Andrew J. Kroll <xxxajk@gmail.com> (atomicity fixes)
 * SPI Master library for arduino.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of either the GNU General Public License version 2
 * or the GNU Lesser General Public License version 2.1, both as
 * published by the Free Software Foundation.
 */

#include "SPI2.h"

SPI2Class SPI2;

uint8_t SPI2Class::initialized = 0;
uint8_t SPI2Class::interruptMode = 0;
uint8_t SPI2Class::interruptMask = 0;
uint8_t SPI2Class::interruptSave = 0;
#ifdef SPI_TRANSACTION_MISMATCH_LED
uint8_t SPI2Class::inTransactionFlag = 0;
#endif

void SPI2Class::begin()
{
  uint8_t sreg = SREG;
  cli(); // Protect from a scheduler and prevent transactionBegin
  if (!initialized) {
    // Set SS to high so a connected chip will be "deselected" by default
    uint8_t port = digitalPinToPort(SS);
    uint8_t bit = digitalPinToBitMask(SS);
    volatile uint8_t *reg = portModeRegister(port);

    // if the SS pin is not already configured as an output
    // then set it high (to enable the internal pull-up resistor)
    if(!(*reg & bit)){
      digitalWriteFast(SS, HIGH);
    }

    // When the SS pin is set as OUTPUT, it can be used as
    // a general purpose output port (it doesn't influence
    // SPI operations).
    pinModeFast(SS, OUTPUT);

    // Warning: if the SS pin ever becomes a LOW INPUT then SPI
    // automatically switches to Slave, so the data direction of
    // the SS pin MUST be kept as OUTPUT.
    SPCR0 |= _BV(MSTR);
    SPCR0 |= _BV(SPE);

    // Set direction register for SCK and MOSI pin.
    // MISO pin automatically overrides to INPUT.
    // By doing this AFTER enabling SPI, we avoid accidentally
    // clocking in a single bit since the lines go directly
    // from "input" to SPI control.
    // http://code.google.com/p/arduino/issues/detail?id=888
    pinModeFast(SCK, OUTPUT);
    pinModeFast(MOSI, OUTPUT);
  }
  initialized++; // reference count
  SREG = sreg;
}

void SPI2Class::end() {
  uint8_t sreg = SREG;
  cli(); // Protect from a scheduler and prevent transactionBegin
  // Decrease the reference counter
  if (initialized)
    initialized--;
  // If there are no more references disable SPI
  if (!initialized) {
    SPCR0 &= ~_BV(SPE);
    interruptMode = 0;
    #ifdef SPI_TRANSACTION_MISMATCH_LED
    inTransactionFlag = 0;
    #endif
  }
  SREG = sreg;
}

// mapping of interrupt numbers to bits within SPI_AVR_EIMSK
#if defined(__AVR_ATmega32U4__)
  #define SPI_INT0_MASK  (1<<INT0)
  #define SPI_INT1_MASK  (1<<INT1)
  #define SPI_INT2_MASK  (1<<INT2)
  #define SPI_INT3_MASK  (1<<INT3)
  #define SPI_INT4_MASK  (1<<INT6)
#elif defined(__AVR_AT90USB646__) || defined(__AVR_AT90USB1286__)
  #define SPI_INT0_MASK  (1<<INT0)
  #define SPI_INT1_MASK  (1<<INT1)
  #define SPI_INT2_MASK  (1<<INT2)
  #define SPI_INT3_MASK  (1<<INT3)
  #define SPI_INT4_MASK  (1<<INT4)
  #define SPI_INT5_MASK  (1<<INT5)
  #define SPI_INT6_MASK  (1<<INT6)
  #define SPI_INT7_MASK  (1<<INT7)
#elif defined(EICRA) && defined(EICRB) && defined(EIMSK)
  #define SPI_INT0_MASK  (1<<INT4)
  #define SPI_INT1_MASK  (1<<INT5)
  #define SPI_INT2_MASK  (1<<INT0)
  #define SPI_INT3_MASK  (1<<INT1)
  #define SPI_INT4_MASK  (1<<INT2)
  #define SPI_INT5_MASK  (1<<INT3)
  #define SPI_INT6_MASK  (1<<INT6)
  #define SPI_INT7_MASK  (1<<INT7)
#else
  #ifdef INT0
  #define SPI_INT0_MASK  (1<<INT0)
  #endif
  #ifdef INT1
  #define SPI_INT1_MASK  (1<<INT1)
  #endif
  #ifdef INT2
  #define SPI_INT2_MASK  (1<<INT2)
  #endif
#endif

void SPI2Class::usingInterrupt(uint8_t interruptNumber)
{
  uint8_t mask = 0;
  uint8_t sreg = SREG;
  cli(); // Protect from a scheduler and prevent transactionBegin
  switch (interruptNumber) {
  #ifdef SPI_INT0_MASK
  case 0: mask = SPI_INT0_MASK; break;
  #endif
  #ifdef SPI_INT1_MASK
  case 1: mask = SPI_INT1_MASK; break;
  #endif
  #ifdef SPI_INT2_MASK
  case 2: mask = SPI_INT2_MASK; break;
  #endif
  #ifdef SPI_INT3_MASK
  case 3: mask = SPI_INT3_MASK; break;
  #endif
  #ifdef SPI_INT4_MASK
  case 4: mask = SPI_INT4_MASK; break;
  #endif
  #ifdef SPI_INT5_MASK
  case 5: mask = SPI_INT5_MASK; break;
  #endif
  #ifdef SPI_INT6_MASK
  case 6: mask = SPI_INT6_MASK; break;
  #endif
  #ifdef SPI_INT7_MASK
  case 7: mask = SPI_INT7_MASK; break;
  #endif
  default:
    interruptMode = 2;
    break;
  }
  interruptMask |= mask;
  if (!interruptMode)
    interruptMode = 1;
  SREG = sreg;
}

void SPI2Class::notUsingInterrupt(uint8_t interruptNumber)
{
  // Once in mode 2 we can't go back to 0 without a proper reference count
  if (interruptMode == 2)
    return;
  uint8_t mask = 0;
  uint8_t sreg = SREG;
  cli(); // Protect from a scheduler and prevent transactionBegin
  switch (interruptNumber) {
  #ifdef SPI_INT0_MASK
  case 0: mask = SPI_INT0_MASK; break;
  #endif
  #ifdef SPI_INT1_MASK
  case 1: mask = SPI_INT1_MASK; break;
  #endif
  #ifdef SPI_INT2_MASK
  case 2: mask = SPI_INT2_MASK; break;
  #endif
  #ifdef SPI_INT3_MASK
  case 3: mask = SPI_INT3_MASK; break;
  #endif
  #ifdef SPI_INT4_MASK
  case 4: mask = SPI_INT4_MASK; break;
  #endif
  #ifdef SPI_INT5_MASK
  case 5: mask = SPI_INT5_MASK; break;
  #endif
  #ifdef SPI_INT6_MASK
  case 6: mask = SPI_INT6_MASK; break;
  #endif
  #ifdef SPI_INT7_MASK
  case 7: mask = SPI_INT7_MASK; break;
  #endif
  default:
    break;
    // this case can't be reached
  }
  interruptMask &= ~mask;
  if (!interruptMask)
    interruptMode = 0;
  SREG = sreg;
}

void SPI2Class::beginTransaction(SPI2Settings settings) {
  if (interruptMode > 0) {
    uint8_t sreg = SREG;
    cli();

    #ifdef SPI_AVR_EIMSK
    if (interruptMode == 1) {
      interruptSave = SPI_AVR_EIMSK;
      SPI_AVR_EIMSK &= ~interruptMask;
      SREG = sreg;
    } else
    #endif
    {
      interruptSave = sreg;
    }
  }

  #ifdef SPI_TRANSACTION_MISMATCH_LED
  if (inTransactionFlag) {
    pinModeFast(SPI_TRANSACTION_MISMATCH_LED, OUTPUT);
    digitalWriteFast(SPI_TRANSACTION_MISMATCH_LED, HIGH);
  }
  inTransactionFlag = 1;
  #endif

  SPCR0 = settings.spcr;
  SPSR0 = settings.spsr;
}

uint8_t SPI2Class::transfer(uint8_t data) {
  SPDR0 = data;
  /*
    * The following NOP introduces a small delay that can prevent the wait
    * loop form iterating when running at the maximum speed. This gives
    * about 10% more speed, even if it seems counter-intuitive. At lower
    * speeds it is unnoticed.
    */
  while (!(SPSR0 & _BV(SPIF))) ; // wait
  return SPDR0;
}
uint16_t SPI2Class::transfer16(uint16_t data) {
  union { uint16_t val; struct { uint8_t lsb; uint8_t msb; }; } in, out;
  in.val = data;
  if (!(SPCR0 & _BV(DORD))) {
    SPDR0 = in.msb;
    while (!(SPSR0 & _BV(SPIF))) ;
    out.msb = SPDR0;
    SPDR0 = in.lsb;
    while (!(SPSR0 & _BV(SPIF))) ;
    out.lsb = SPDR0;
  } else {
    SPDR0 = in.lsb;
    while (!(SPSR0 & _BV(SPIF))) ;
    out.lsb = SPDR0;
    SPDR0 = in.msb;
    while (!(SPSR0 & _BV(SPIF))) ;
    out.msb = SPDR0;
  }
  return out.val;
}
void SPI2Class::read(void *buf, size_t count) {
  if (count == 0) return;
  uint8_t *p = (uint8_t *)buf;
  SPDR0 = 0xFF;
  while (--count > 0) {
    while (!(SPSR0 & _BV(SPIF))) ;
    *p++ = SPDR0;
    SPDR0 = 0xFF;
  }
  while (!(SPSR0 & _BV(SPIF))) ;
  *p = SPDR0;
}
void SPI2Class::write(void *buf, size_t count) {
  if (count == 0) return;
  uint8_t *p = (uint8_t *)buf;
  SPDR0 = *p;
  while (--count > 0) {
    while (!(SPSR0 & _BV(SPIF))) ;
    uint8_t in = SPDR0;
    SPDR0 = *++p;
  }
  while (!(SPSR0 & _BV(SPIF))) ;
  uint8_t in = SPDR0;
}  // After performing a group of transfers and releasing the chip select
// signal, this function allows others to access the SPI bus
void SPI2Class::endTransaction(void) {
  #ifdef SPI_TRANSACTION_MISMATCH_LED
  if (!inTransactionFlag) {
    pinModeFast(SPI_TRANSACTION_MISMATCH_LED, OUTPUT);
    digitalWriteFast(SPI_TRANSACTION_MISMATCH_LED, HIGH);
  }
  inTransactionFlag = 0;
  #endif

  if (interruptMode > 0) {
    #ifdef SPI_AVR_EIMSK
    uint8_t sreg = SREG;
    #endif
    cli();
    #ifdef SPI_AVR_EIMSK
    if (interruptMode == 1) {
      SPI_AVR_EIMSK = interruptSave;
      SREG = sreg;
    } else
    #endif
    {
      SREG = interruptSave;
    }
  }
}

// This function is deprecated.  New applications should use
// beginTransaction() to configure SPI settings.
void SPI2Class::setBitOrder(uint8_t bitOrder) {
  if (bitOrder == LSBFIRST) SPCR0 |= _BV(DORD);
  else SPCR0 &= ~(_BV(DORD));
}
// This function is deprecated.  New applications should use
// beginTransaction() to configure SPI settings.
void SPI2Class::setDataMode(uint8_t dataMode) {
  SPCR0 = (SPCR0 & ~SPI_MODE_MASK) | dataMode;
}
// This function is deprecated.  New applications should use
// beginTransaction() to configure SPI settings.
void SPI2Class::setClockDivider(uint8_t clockDiv) {
  SPCR0 = (SPCR0 & ~SPI_CLOCK_MASK) | (clockDiv & SPI_CLOCK_MASK);
  SPSR0 = (SPSR0 & ~SPI_2XCLOCK_MASK) | ((clockDiv >> 2) & SPI_2XCLOCK_MASK);
}

