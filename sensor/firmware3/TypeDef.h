#pragma once

//#define KITELIB_DEBUG

#ifdef KITELIB_DEBUG
  #define DEBUG_PRINT(x)                printf_P(PSTR("%S"), (x))
  #define DEBUG_PRINT_BIN(x)            printf_P(PSTR("%d"), x)
  #define DEBUG_PRINTLN_BIN(x)          printf_P(PSTR("%d\n"), int(x))
  #define DEBUG_PRINT_DEC(x)            printf_P(PSTR("%d"), int(x))
  #define DEBUG_PRINTLN_DEC(x)          printf_P(PSTR("%d\n"), int(x))
  #define DEBUG_PRINT_HEX(x)            printf_P(PSTR("%x"), int(x))
  #define DEBUG_PRINTLN_HEX(x)          printf_P(PSTR("%x\n"), int(x))
  #define DEBUG_PRINTLN(x)              printf_P(PSTR("%S\n"), (x))
  #define DEBUG_PRINT_STR(x)            printf_P(PSTR("%S"), (x))
  #define DEBUG_PRINTLN_STR(x)          printf_P(PSTR("%S\n"), (x))
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINT_BIN(x)
  #define DEBUG_PRINTLN_BIN(x)
  #define DEBUG_PRINT_DEC(x)
  #define DEBUG_PRINTLN_DEC(x)
  #define DEBUG_PRINT_HEX(x)
  #define DEBUG_PRINTLN_HEX(x)
  #define DEBUG_PRINTLN(x)
  #define DEBUG_PRINT_STR(x)
  #define DEBUG_PRINTLN_STR(x)
#endif

// Shield configuration
#define USE_SPI                               0x00
#define USE_UART                              0x01
#define USE_I2C                               0x02
#define INT_NONE                              0x00
#define INT_0                                 0x01
#define INT_1                                 0x02
#define INT_BOTH                              0x03

#define ERR_NONE                               0
#define ERR_UNKNOWN                           -1

#define ERR_CHIP_NOT_FOUND                    -2
#define ERR_EEPROM_NOT_INITIALIZED            -3
#define ERR_PACKET_TOO_LONG                   -4
#define ERR_TX_TIMEOUT                        -5
#define ERR_RX_TIMEOUT                        -6
#define ERR_CRC_MISMATCH                      -7
#define ERR_INVALID_BANDWIDTH                 -8
#define ERR_INVALID_SPREADING_FACTOR          -9
#define ERR_INVALID_CODING_RATE               -10
#define ERR_INVALID_BIT_RANGE                 -11
#define ERR_INVALID_FREQUENCY                 -12
#define ERR_INVALID_OUTPUT_POWER              -13
#define PREAMBLE_DETECTED                     -14
#define CHANNEL_FREE                          -15
#define ERR_SPI_WRITE_FAILED                  -16
#define ERR_INVALID_CURRENT_LIMIT             -17
#define ERR_INVALID_PREAMBLE_LENGTH           -18
#define ERR_INVALID_GAIN                      -19
#define ERR_WRONG_MODEM                       -20
#define ERR_INVALID_BIT_RATE                  -21
#define ERR_INVALID_RX_BANDWIDTH              -22
#define ERR_INVALID_FREQUENCY_DEVIATION       -23
#define ERR_INVALID_SYNC_WORD                 -24
