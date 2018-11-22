//#include <Arduino.h>
//#include <HardwareSerial.h>
#include <avr/eeprom.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/wdt.h>
#include <avr/power.h>
#include <compat/deprecated.h>

#include "SPI.h"
#include "LEDs.h"
#include "Buttons.h"
#include "Battery.h"
#include "Sensor_Comms.h"
#include "Data_Defs.h"
#include "avr_stdio.h"
#include "Storage.h"
#include "Sleep.h"
#include "digitalWriteFast.h"

#define BAUD 57600
#include <util/setbaud.h>

#include "SHT2x.h"

__extension__ typedef int __guard __attribute__((mode (__DI__)));

//extern "C" int __cxa_guard_acquire(__guard *g) {return !*(char *)(g);};
//extern "C" void __cxa_guard_release (__guard *g) {*(char *)g = 1;};
//extern "C" void __cxa_guard_abort (__guard *) {};
//extern "C" void __cxa_pure_virtual() { while (1); }

#include <stdlib.h>
/*
inline void* operator new(size_t size) { return malloc(size); }
inline void* operator new[](size_t size) { return malloc(size); }
inline void operator delete(void* p) { free(p); }
inline void operator delete[](void* p) { free(p); }
inline void* operator new(size_t size_, void *ptr_) { return ptr_; }
*/
//////////////////////////////////////////////////////////////////////////////////////////


#define LOG(...) printf_P(__VA_ARGS__)
//#define LOG(...)

//////////////////////////////////////////////////////////////////////////////////////////

static void hash_combine(uint32_t& seed, uint32_t v)
{
    seed ^= v + 0x9e3779b9 + (seed<<6) + (seed>>2);
}

//////////////////////////////////////////////////////////////////////////////////////////

enum class State : uint8_t
{
    PAIR,
    FIRST_CONFIG,
    MEASUREMENT_LOOP,
    SLEEPING_LOOP
};
static State s_state = State::PAIR;

#define SENSOR_TYPE       1
#define HARDWARE_VERSION  2
#define SOFTWARE_VERSION  2

static uint8_t s_error_flags = 0;

static SHT2x s_sht;
static Sensor_Comms s_comms;
static FILE s_uart_output;

static uint32_t s_baseline_measurement_index = 0; //measurements should start from this index
static uint32_t s_first_stored_measurement_index = 0; //the index of the first measurement in storage

static chrono::time_ms s_next_measurement_time_point;
static chrono::millis s_measurement_period;

static chrono::time_ms s_next_comms_time_point;
static chrono::millis s_comms_period;

static int8_t s_last_input_dBm = 0;

static Storage s_storage;
static data::sensor::Calibration s_calibration;
static uint32_t s_serial_number = 0;
static bool s_sensor_sleeping = false;


//////////////////////////////////////////////////////////////////////////////////////////

ISR(BADISR_vect)
{
    while (true)
    {
      blink_led(Blink_Led::Red, 255, chrono::millis(50));
    }
}

void wdt_init(void) __attribute__((naked)) __attribute__((section(".init3")));

void wdt_init(void)
{
    wdt_disable();
    return;
}

#define STACK_CANARY 0xc5
extern uint8_t _end;
extern uint8_t __stack;

static void stack_paint(void)
{
    uint8_t *p = &_end;

    while(p < (uint8_t*)&p)
    {
        *p = 0xc5;
        p++;
    }  
}

static uint16_t initial_stack_size(void)
{
    return &__stack - &_end;
}

static uint16_t stack_size(void)
{
    const uint8_t *p = &_end;
    uint16_t       c = 0;

    while(*p == 0xc5 && p < (uint8_t*)&p)
    {
        p++;
        c++;
    }

    return c;
}

//////////////////////////////////////////////////////////////////////////////////////////

static constexpr uint8_t SETTINGS_VERSION = 2;

static uint8_t  EEMEM eeprom_version;
static uint32_t EEMEM eeprom_serial_number;
static uint32_t EEMEM eeprom_address;
static uint16_t EEMEM eeprom_temperature_bias;
static uint16_t EEMEM eeprom_humidity_bias;
static uint32_t EEMEM eeprom_crc;

static void reset_settings()
{
    eeprom_write_byte(&eeprom_version, 0);
}

static void save_settings(uint32_t serial_number, uint32_t address, data::sensor::Calibration const& calibration)
{
    LOG(PSTR("Saving settings..."));
    uint32_t crc = 123456789ULL;
    eeprom_write_byte(&eeprom_version, SETTINGS_VERSION);
    hash_combine(crc, SETTINGS_VERSION);
    
    eeprom_write_dword(&eeprom_serial_number, serial_number);
    hash_combine(crc, serial_number);
    
    eeprom_write_dword(&eeprom_address, address);
    hash_combine(crc, address);
    
    eeprom_write_word(&eeprom_temperature_bias, *(uint16_t*)&calibration.temperature_bias);
    hash_combine(crc, int32_t(calibration.temperature_bias) + 65535ULL);
    
    eeprom_write_word(&eeprom_humidity_bias, *(uint16_t*)&calibration.humidity_bias);
    hash_combine(crc, int32_t(calibration.humidity_bias) + 65535ULL);

    eeprom_write_dword(&eeprom_crc, crc);
    LOG(PSTR("done\n"));
}

static bool load_settings(uint32_t& serial_number, uint32_t& address, data::sensor::Calibration& calibration)
{
    LOG(PSTR("Loading settings..."));
    if (eeprom_read_byte(&eeprom_version) == SETTINGS_VERSION)
    {
        uint32_t crc = 123456789ULL;
        hash_combine(crc, SETTINGS_VERSION);
        
        serial_number = eeprom_read_dword(&eeprom_serial_number);
        hash_combine(crc, serial_number);
        
        address = eeprom_read_dword(&eeprom_address);
        hash_combine(crc, address);

        uint16_t v = eeprom_read_word(&eeprom_temperature_bias);
        calibration.temperature_bias = *(int16_t*)&v;
        hash_combine(crc, int32_t(calibration.temperature_bias) + 65535ULL);

        v = eeprom_read_word(&eeprom_humidity_bias);
        calibration.humidity_bias = *(int16_t*)&v;
        hash_combine(crc, int32_t(calibration.humidity_bias) + 65535ULL);

        uint32_t stored_crc = eeprom_read_dword(&eeprom_crc);
        if (stored_crc != crc)
        {
            LOG(PSTR("failed: crc mismatch\n"));
            //reset_settings();
            return false;
        }
        LOG(PSTR("done\n"));
        return true;
    }
    else
    {
        LOG(PSTR("failed: old version\n"));
        reset_settings();
        return false;
    }
}

//////////////////////////////////////////////////////////////////////////////////////////
extern bool _i2c_init();

static void setup()
{
    int mcusr_value = MCUSR;
    MCUSR = 0;

    //setup serial.
    //Serial.begin(57600);
    // Turn on the transmission/reception circuitry
    UCSR0B |= (1 << RXEN0) | (1 <<TXEN0); 
    UBRR0L = UBRRL_VALUE;
    UBRR0H = UBRRH_VALUE;

    fdev_setup_stream(&s_uart_output, uart_putchar, NULL, _FDEV_SETUP_WRITE);
    stdout = &s_uart_output;
    ///

    ///////////////////////////////////////////////
    //setup the leds and buttons
    init_leds();
    init_buttons();

    ///////////////////////////////////////////////
    //setup clock
    stack_paint();
    setup_clock(921600);

    chrono::delay(chrono::millis(2000)); //wait for the main clock to get calibrated

    

/*
    printf_P(PSTR("0 %d\n"), (int)(EECR & bit(EERIE)));
    printf_P(PSTR("1 %d\n"), (int)(XFDCSR & bit(XFDIE)));
    printf_P(PSTR("2 %d\n"), (int)(WDTCSR & bit(WDIE)));
    printf_P(PSTR("3 %d\n"), (int)(PCICR & bit(PCIE2)));
    printf_P(PSTR("4 %d\n"), (int)(PCICR & bit(PCIE1)));
    printf_P(PSTR("5 %d\n"), (int)(PCICR & bit(PCIE0)));
    printf_P(PSTR("6 %d\n"), (int)(TIMSK0 & bit(OCIE0B)));
    printf_P(PSTR("7 %d\n"), (int)(TIMSK0 & bit(OCIE0A)));
    printf_P(PSTR("8 %d\n"), (int)(TIMSK0 & bit(TOIE0)));
    printf_P(PSTR("9 %d\n"), (int)(TIMSK1 & bit(ICIE1)));
    printf_P(PSTR("a %d\n"), (int)(TIMSK1 & bit(OCIE1B)));
    printf_P(PSTR("b %d\n"), (int)(TIMSK1 & bit(OCIE1A)));
    printf_P(PSTR("c %d\n"), (int)(TIMSK1 & bit(TOIE1)));
    printf_P(PSTR("d %d\n"), (int)(TIMSK2 & bit(OCIE2B)));
    printf_P(PSTR("e %d\n"), (int)(TIMSK2 & bit(OCIE2A)));
    printf_P(PSTR("f %d\n"), (int)(TIMSK2 & bit(TOIE2)));
    printf_P(PSTR("g %d\n"), (int)(TIMSK3 & bit(ICIE3)));
    printf_P(PSTR("h %d\n"), (int)(TIMSK3 & bit(OCIE3B)));
    printf_P(PSTR("i %d\n"), (int)(TIMSK3 & bit(OCIE3A)));
    printf_P(PSTR("j %d\n"), (int)(TIMSK3 & bit(TOIE3)));
    printf_P(PSTR("k %d\n"), (int)(TIMSK4 & bit(ICIE4)));
    printf_P(PSTR("l %d\n"), (int)(TIMSK4 & bit(OCIE4B)));
    printf_P(PSTR("m %d\n"), (int)(TIMSK4 & bit(OCIE4A)));
    printf_P(PSTR("n %d\n"), (int)(TIMSK4 & bit(TOIE4)));
    printf_P(PSTR("o %d\n"), (int)(SPCR0 & bit(SPIE)));
    printf_P(PSTR("p %d\n"), (int)(SPCR1 & bit(SPIE)));
    printf_P(PSTR("q %d\n"), (int)(UCSR0B & bit(RXCIE0)));
    printf_P(PSTR("r %d\n"), (int)(UCSR0B & bit(TXCIE0)));
    printf_P(PSTR("s %d\n"), (int)(UCSR0B & bit(UDRIE0)));
    printf_P(PSTR("t %d\n"), (int)(UCSR1B & bit(RXCIE1)));
    printf_P(PSTR("u %d\n"), (int)(UCSR1B & bit(TXCIE1)));
    printf_P(PSTR("v %d\n"), (int)(UCSR1B & bit(UDRIE1)));
    printf_P(PSTR("w %d\n"), (int)(TWCR0 & bit(TWIE)));
    printf_P(PSTR("w %d\n"), (int)(TWCR1 & bit(TWIE)));
    printf_P(PSTR("x %d\n"), (int)(ACSR & bit(ACIE)));
    printf_P(PSTR("y %d\n"), (int)(ADCSRA & bit(ADIE)));
    printf_P(PSTR("z %d\n"), (int)(SPMCSR & bit(SPMIE)));
    printf_P(PSTR("0 %d\n"), (int)(EIMSK & bit(INT0)));
    printf_P(PSTR("1 %d\n"), (int)(EIMSK & bit(INT1)));
*/
/*
    int counter = 0;
    while (true)
    {
      if (counter++ < 100)
      {
        set_led(Led::Yellow);
        LOG(PSTR("%d, OSCCAL: %d, %ld, %d\n"), counter, (int)OSCCAL, (int32_t)s_xxx, (int)chrono::now().ticks);
        chrono::delay(chrono::millis(10));
        set_led(Led::None);
      }
      else
      {
        sleep(chrono::millis(1000), true);
        counter = 0;
      }
    }
    */
/*
    chrono::time_ms start = chrono::now();
    uint32_t count = 0;
    uint32_t loops = 1;
    while (true)
    {
      //printf_P(PSTR("%d, %d\n"), digitalReadFast(18), digitalReadFast(19));
      //chrono::delay(chrono::millis(20));
      int x = rand() % 200;
      uint32_t rt_count = (uint32_t)(chrono::now() - start).count;
      printf_P(PSTR("Sleeping for %d. TP: %ld, %ld. OSC: %d, T: %ld, D: %ld\n  "), x, rt_count, count, (int)OSCCAL, (uint32_t)s_xxx, (rt_count - count) / loops);
      sleep_exact(chrono::millis(x));
      count += x;
      loops++;
      //chrono::delay(chrono::millis(50));
    }*/

    ////////////////////////////////////////////

    LOG(PSTR("*** >> "));
    s_error_flags = 0;
    if (mcusr_value & (1<<PORF))
    {
        s_error_flags |= static_cast<uint8_t>(data::sensor::Measurement::Flag::REBOOT_POWER_ON);
        LOG(PSTR("Power-on reset."));
    }
    if (mcusr_value & (1<<EXTRF))
    {
        s_error_flags |= static_cast<uint8_t>(data::sensor::Measurement::Flag::REBOOT_RESET);
        LOG(PSTR("External reset!"));
    }
    if (mcusr_value & (1<<BORF))
    {
        s_error_flags |= static_cast<uint8_t>(data::sensor::Measurement::Flag::REBOOT_BROWNOUT);
        LOG(PSTR("Brownout reset!"));
    }
    if (mcusr_value & (1<<WDRF))
    {
        s_error_flags |= static_cast<uint8_t>(data::sensor::Measurement::Flag::REBOOT_WATCHDOG);
        LOG(PSTR("Watchdog reset!"));
    }
    LOG(PSTR(" << ***\n"));

    LOG(PSTR("Stack: initial %d, now %d\n"), initial_stack_size(), stack_size());

    ///////////////////////////////////////////////
    //initialize the bus and sensors
    while (!_i2c_init())
    {
        LOG(PSTR("I2C failed\n"));
        blink_led(Blink_Led::Red, 4, chrono::millis(200));
        while (true)
        {
            printf_P(PSTR("%d, %d\n"), digitalReadFast(18), digitalReadFast(19));
            chrono::delay(chrono::millis(20));
        }
        sleep(chrono::seconds(3), false);
    }
    sleep_exact(50);

    init_adc();

    //init sensor
    while (true)
    {
        float t;
        float h;
        s_sht.GetHumidity(h);
        s_sht.GetTemperature(t);
        float vcc = read_vcc();      
        LOG(PSTR("VCC: %d.%dV\n"), (int)vcc, int(vcc * 100.f) % 100);
        LOG(PSTR("T: %d.%d'C\n"), (int)t, int(t * 100.f) % 100);
        LOG(PSTR("H: %d.%d%%\n"), (int)h, int(h * 100.f) % 100);
        if (t != 0.f && h > 0.1f && vcc > 1.5f)
        {
          break;
        }
        LOG(PSTR("Sensors failed\n"));
        blink_led(Blink_Led::Red, 5, chrono::millis(200));
        sleep(chrono::seconds(3), false);
    }
    ///////////////////////////////////////////////

        
/*
    for (uint32_t i = 100; i < 100000; i += 100)
    {
        LOG(PSTR("Sleeping %ldms: "), i);
        Serial.flush();
        chrono::micros us = sleep(chrono::micros(i * 1000), true);
        LOG(PSTR("done. left: %ldus\n"), (uint32_t)us.count);
    }
*/
    ///////////////////////////////////////////////
    //seed the RNG
    uint32_t rnd_seed = 0;
    while (true)
    {
        float t;
        float h;
        for (uint8_t i = 0; i < 20; i++)
        {
            s_sht.GetHumidity(h);
            s_sht.GetTemperature(t);
            float vcc = read_vcc();
            hash_combine(rnd_seed, *(uint32_t*)&vcc);
            hash_combine(rnd_seed, *(uint32_t*)&t);
            hash_combine(rnd_seed, *(uint32_t*)&h);
            sleep_exact(10);
        }
        if (rnd_seed != 0)
        {
            srandom(rnd_seed);
            LOG(PSTR("Random seed: %lu\n"), rnd_seed);
            break;
        }

        LOG(PSTR("Random seed failed\n"));
        blink_led(Blink_Led::Red, 6, chrono::millis(200));
        sleep(chrono::seconds(3), false);
    }

    ///////////////////////////////////////////////
    // Start the RF
    while (!s_comms.init(5, 0))
    {
        LOG(PSTR("RF failed\n"));
        blink_led(Blink_Led::Red, 7, chrono::millis(200));
        sleep(chrono::seconds(3), false);
    }
    s_comms.sleep_mode();

    /*
    //send some test garbage for frequency measurements
    {
        uint8_t raw_packet_data[packet_raw_size(Sensor_Comms::MAX_USER_DATA_SIZE)];
        s_comms.set_destination_address(Sensor_Comms::BROADCAST_ADDRESS);
        s_comms.begin_packet(raw_packet_data, 0);
        s_comms.send_packet(raw_packet_data, 2);
    }
    */

    ///////////////////////////////////////////////
    //done
    blink_led(Blink_Led::Green, 5, chrono::millis(50));

    ///////////////////////////////////////////////
    //settings
    {
        uint32_t address = 0;
        bool ok = load_settings(s_serial_number, address, s_calibration);

        if (s_serial_number == 0)
        {
            s_serial_number = rnd_seed;
            save_settings(s_serial_number, address, s_calibration);
        }

        if (ok && address >= Sensor_Comms::SLAVE_ADDRESS_BEGIN)
        {
            s_comms.set_address(address);
            s_state = State::FIRST_CONFIG;
        }
        else
        {
            s_state = State::PAIR;
        }
    }

    LOG(PSTR("Sensor Type: %d\n"), SENSOR_TYPE);
    LOG(PSTR("Hardware Version: %d\n"), HARDWARE_VERSION);
    LOG(PSTR("Software Version: %d\n"), SOFTWARE_VERSION);
    LOG(PSTR("Serial Number: %lx\n"), s_serial_number);
    LOG(PSTR("Address: %lu\n"), s_comms.get_address());
}

//////////////////////////////////////////////////////////////////////////////////////////

static bool apply_config(const data::sensor::Config_Response& config)
{
    if (config.measurement_period.count == 0 || config.comms_period.count == 0)
    {
        LOG(PSTR("Bag config received!!!\n"));
        return false;
    }

    s_baseline_measurement_index = config.baseline_measurement_index;

    data::sensor::Calibration calibration = s_calibration;
    calibration.temperature_bias += config.calibration_change.temperature_bias;
    calibration.humidity_bias += config.calibration_change.humidity_bias;
    if (calibration.temperature_bias != s_calibration.temperature_bias || calibration.humidity_bias != s_calibration.humidity_bias)
    {
        s_calibration = calibration;
        save_settings(s_serial_number, s_comms.get_address(), s_calibration);
    }

    s_comms_period = chrono::millis(config.comms_period.count * 1000LL);
    s_measurement_period = chrono::millis(config.measurement_period.count * 1000LL);

    chrono::time_ms now = chrono::now();
    if (config.next_comms_delay.count < 0 && (now + chrono::millis(config.next_comms_delay)) > now) //overflow?
    {
        s_next_comms_time_point = chrono::time_ms(0);
    }
    else
    {
        s_next_comms_time_point = now + chrono::millis(config.next_comms_delay);
    }

    if (config.next_measurement_delay.count < 0 && (now + chrono::millis(config.next_measurement_delay)) > now) //overflow?
    {
        s_next_comms_time_point = chrono::time_ms(0);
    }
    else
    {
        s_next_measurement_time_point = now + chrono::millis(config.next_measurement_delay);
    }

    //remove confirmed measurements
    while (s_first_stored_measurement_index <= config.last_confirmed_measurement_index)
    {
        if (!s_storage.pop_front())
        {
            LOG(PSTR("Failed to pop storage data. Left: %ld\n"), config.last_confirmed_measurement_index - s_first_stored_measurement_index);
            s_first_stored_measurement_index = config.last_confirmed_measurement_index + 1;
            break;
        }
        s_first_stored_measurement_index++;
    };

    uint32_t measurement_count = s_storage.get_data_count();
    s_sensor_sleeping = config.sleeping;

    s_comms.set_transmission_power(config.power);

    LOG(PSTR("temp bias: %d\n"), (int)(s_calibration.temperature_bias));
    LOG(PSTR("humidity bias: %d\n"), (int)(s_calibration.humidity_bias));
    LOG(PSTR("next comms delay: %ld\n"), config.next_comms_delay.count);
    LOG(PSTR("comms period: %ld\n"), config.comms_period.count);
    LOG(PSTR("next measurement delay: %ld\n"), config.next_measurement_delay.count);
    LOG(PSTR("measurement period: %ld\n"), config.measurement_period.count);
    LOG(PSTR("baseline index: %lu\n"), config.baseline_measurement_index);
    LOG(PSTR("last confirmed index: %lu\n"), config.last_confirmed_measurement_index);
    LOG(PSTR("first stored index: %lu\n"), s_first_stored_measurement_index);
    LOG(PSTR("count: %lu\n"), measurement_count);
    LOG(PSTR("sleeping: %d\n"), s_sensor_sleeping ? 1 : 0);
    LOG(PSTR("power: %d\n"), (int)config.power);

    return true;
}

//////////////////////////////////////////////////////////////////////////////////////////

static bool request_config()
{
    bool send_successful = false;
    {
        uint8_t raw_buffer[packet_raw_size(sizeof(data::sensor::Config_Request))];
        s_comms.begin_packet(raw_buffer, static_cast<uint8_t>(data::sensor::Type::CONFIG_REQUEST), true);
        data::sensor::Config_Request req;
        req.first_measurement_index = s_first_stored_measurement_index;
        req.measurement_count = s_storage.get_data_count();
        req.b2s_input_dBm = s_last_input_dBm;
        req.calibration = s_calibration;
        req.sleeping = s_sensor_sleeping;
        s_comms.pack(raw_buffer, &req, sizeof(req));
        send_successful = s_comms.send_packet(raw_buffer, 2);
    }

    if (send_successful)
    {
        uint8_t size = sizeof(data::sensor::Config_Response);
        uint8_t raw_buffer[packet_raw_size(size)];
        uint8_t* buffer = s_comms.receive_packet(raw_buffer, size, 500);
        if (buffer)
        {
            data::sensor::Type type = static_cast<data::sensor::Type>(s_comms.get_rx_packet_type(buffer));
            if (type == data::sensor::Type::CONFIG_RESPONSE && size == sizeof(data::sensor::Config_Response))
            {
                s_last_input_dBm = s_comms.get_input_dBm();
                const data::sensor::Config_Response* ptr = reinterpret_cast<const data::sensor::Config_Response*>(s_comms.get_rx_packet_payload(buffer));
                return apply_config(*ptr);
            }
        }
    }
    return false;
}

//////////////////////////////////////////////////////////////////////////////////////////

static void pair_state()
{
    LOG(PSTR(">>> Entering pair state\n"));
    
    uint32_t addr = Sensor_Comms::PAIR_ADDRESS_BEGIN + random() % (Sensor_Comms::PAIR_ADDRESS_END - Sensor_Comms::PAIR_ADDRESS_BEGIN);
    s_comms.set_address(addr);
    s_comms.set_destination_address(Sensor_Comms::BASE_ADDRESS);

    LOG(PSTR("Starting pairing\n"));

    s_comms.sleep_mode();

    wait_for_release(Button::BUTTON1);
    bool done = false;
    while (!done)
    {
        //wait actively for a while for the user to press the button
        uint8_t tries = 0;
        while (tries++ < 10)
        {
            if (is_pressed(Button::BUTTON1))
            {
                tries = 0;
                blink_led(Blink_Led::Yellow, 3, chrono::millis(500));

                //wait for the user to release the button
                wait_for_release(Button::BUTTON1);

                LOG(PSTR("Sending request..."));

                bool send_successful = false;
                {
                    uint8_t raw_buffer[packet_raw_size(sizeof(data::sensor::Pair_Request))];
                    s_comms.begin_packet(raw_buffer, static_cast<uint8_t>(data::sensor::Type::PAIR_REQUEST), true);
                    data::sensor::Pair_Request request;
                    request.descriptor.sensor_type = SENSOR_TYPE;
                    request.descriptor.hardware_version = HARDWARE_VERSION;
                    request.descriptor.software_version = SOFTWARE_VERSION;
                    request.descriptor.serial_number = s_serial_number;
                    request.calibration = s_calibration;
                    s_comms.pack(raw_buffer, &request, sizeof(request));
                    send_successful = s_comms.send_packet(raw_buffer, 2);
                }

                if (send_successful)
                {
                    LOG(PSTR("done.\nWaiting for response..."));

                    uint8_t size = sizeof(data::sensor::Pair_Response);
                    uint8_t raw_buffer[packet_raw_size(size)];
                    uint8_t* buffer = s_comms.receive_packet(raw_buffer, size, 2000);
                    if (buffer)
                    {
                        LOG(PSTR("Received packet type %d of %d bytes\n"), (int)s_comms.get_rx_packet_type(buffer), (int)size);
                    }

                    if (buffer && size == sizeof(data::sensor::Pair_Response) && 
                          s_comms.get_rx_packet_type(buffer) == static_cast<uint8_t>(data::sensor::Type::PAIR_RESPONSE))
                    {
                        s_comms.sleep_mode();
                        s_last_input_dBm = s_comms.get_input_dBm();

                        const data::sensor::Pair_Response* response_ptr = reinterpret_cast<const data::sensor::Pair_Response*>(s_comms.get_rx_packet_payload(buffer));
                        s_comms.set_address(response_ptr->address);

                        LOG(PSTR("done. Addr: %lu\n"), response_ptr->address);

                        save_settings(s_serial_number, response_ptr->address, s_calibration);

                        blink_led(Blink_Led::Green, 3, chrono::millis(500));

                        done = true;
                        break;
                    }
                    else
                    {
                        s_comms.sleep_mode();
                        LOG(PSTR("timeout\n"));
                    }
                }
                else
                {
                    LOG(PSTR("failed.\n"));
                }
                blink_led(Blink_Led::Red, 3, chrono::millis(500));
            }

            s_comms.sleep_mode();

            blink_led(Blink_Led::Yellow, 3, chrono::millis(100));
            sleep(chrono::millis(2000), true);
        }

        if (!done)
        {
            LOG(PSTR("Going to sleep..."));

            //fade_out_leds(YELLOW_LED, chrono::millis(1000));

            //the user didn't press it - sleep for a loooong time
            sleep(chrono::seconds(3600ULL * 24ULL), true);

            LOG(PSTR("woke up.\n"));

            //fade_in_leds(YELLOW_LED, chrono::millis(1000));

            //we probably woken up because the user pressed the button - wait for him to release it
            wait_for_release(Button::BUTTON1);
        }
    }

    s_comms.sleep_mode();

    blink_led(Blink_Led::Green, 3, chrono::millis(500));

    //wait for the user to release the button
    wait_for_release(Button::BUTTON1);

    s_state = State::FIRST_CONFIG;

    //sleep a bit
    sleep(chrono::millis(1000 * 1), false);

    LOG(PSTR("<<< Exiting pair state\n"));
}

//////////////////////////////////////////////////////////////////////////////////////////

static constexpr float MIN_VALID_HUMIDITY = 5.f;

static void do_measurement()
{
    Storage::Data data;

    // Reading temperature or humidity takes about 250 milliseconds!
    // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
    constexpr int8_t max_tries = 10;
    uint8_t tries = 0;
    do
    {
        LOG(PSTR("Measuring %d..."), tries);
        if (s_sht.GetTemperature(data.temperature) && s_sht.GetHumidity(data.humidity))
        {
            data.temperature += static_cast<float>(s_calibration.temperature_bias) * 0.01f;
            data.humidity += static_cast<float>(s_calibration.humidity_bias) * 0.01f;
            break;
        }
    } while (++tries < max_tries);

    if (tries >= max_tries)
    {
        //the way to indicate error!
        data.humidity = 0;
        data.temperature = 0;
    }
    else
    {
        data.humidity = max(data.humidity, MIN_VALID_HUMIDITY);
    }

    //push back and make room if it fails
    LOG(PSTR("Storing..."));
    while (s_storage.push_back(data) == false)
    {
        LOG(PSTR("*"));
        s_storage.pop_front();
        s_first_stored_measurement_index++;
    }
    LOG(PSTR("done\n"));
}

//////////////////////////////////////////////////////////////////////////////////////////

static void do_comms()
{
    size_t count = s_storage.get_data_count();
    if (count == 0)
    {
        LOG(PSTR("No data to send!!!!\n"));
    }
    else
    {
        LOG(PSTR("%d items to send\n"), count);
    }

    //send measurements
    if (count > 0)
    {
        const chrono::millis COMMS_SLOT_DURATION = chrono::millis(7000);
        chrono::time_ms start_tp = chrono::now();

        uint8_t raw_buffer[packet_raw_size(sizeof(data::sensor::Measurement_Batch_Request))];
        data::sensor::Measurement_Batch_Request& batch = *(data::sensor::Measurement_Batch_Request*)s_comms.get_tx_packet_payload(raw_buffer);

        batch.start_index = s_first_stored_measurement_index;
        batch.count = 0;

        bool done = false;
        Storage::iterator it;
        do
        {
            bool send = false;
            if (s_storage.unpack_next(it) == false)
            {
                LOG(PSTR("Finished the data.\n"));
                done = true;
                send = true;
            }
            else
            {
                float vcc = read_vcc();
                data::sensor::Measurement& item = batch.measurements[batch.count++];
                item.flags = s_error_flags;
                if (it.data.humidity < MIN_VALID_HUMIDITY)
                {
                    item.flags |= static_cast<uint8_t>(data::sensor::Measurement::Flag::SENSOR_ERROR);
                    item.pack(0.f, 0.f);
                    batch.pack(vcc);
                }
                else
                {
                    item.pack(it.data.humidity, it.data.temperature);
                    batch.pack(vcc);
                }
            }

            if (batch.count >= data::sensor::Measurement_Batch_Request::MAX_COUNT)
            {
                send = true;
            }

            chrono::time_ms now = chrono::now();
            if (now < start_tp || now - start_tp >= COMMS_SLOT_DURATION)
            {
                LOG(PSTR("Comms time slot expired\n"));
                done = true;
            }

            if (send && batch.count > 0)
            {
                batch.last_batch = done ? 1 : 0;
                LOG(PSTR("Sending measurement batch of %d..."), (int)batch.count);
                s_comms.begin_packet(raw_buffer, static_cast<uint8_t>(data::sensor::Type::MEASUREMENT_BATCH_REQUEST), false);
                if (s_comms.send_packet(raw_buffer, sizeof(data::sensor::Measurement_Batch_Request), 1) == true)
                {
                    s_error_flags = 0;
                    LOG(PSTR("done.\n"));
                }
                else
                {
                    s_error_flags |= static_cast<uint8_t>(data::sensor::Measurement::Flag::COMMS_ERROR);
                    LOG(PSTR("failed: comms\n"));
                    break;
                }

                batch.start_index += batch.count;
                batch.count = 0;

                sleep(chrono::millis(200ULL), true);
            }
        } while (!done);
    }

    LOG(PSTR("done with batches\n"));
    request_config();

    s_comms.sleep_mode();
}

//////////////////////////////////////////////////////////////////////////////////////////

static bool apply_first_config(const data::sensor::First_Config_Response& first_config)
{
    s_storage.clear();

    if (!apply_config(first_config))
    {
        return false;
    }
    s_first_stored_measurement_index = first_config.first_measurement_index;

    return true;
}

//////////////////////////////////////////////////////////////////////////////////////////

static bool request_first_config()
{
    bool send_successful = false;
    {
        uint8_t raw_buffer[packet_raw_size(sizeof(data::sensor::First_Config_Request))];
        s_comms.begin_packet(raw_buffer, static_cast<uint8_t>(data::sensor::Type::FIRST_CONFIG_REQUEST), true);
        data::sensor::First_Config_Request request;
        request.descriptor.sensor_type = SENSOR_TYPE;
        request.descriptor.hardware_version = HARDWARE_VERSION;
        request.descriptor.software_version = SOFTWARE_VERSION;
        request.descriptor.serial_number = s_serial_number;
        s_comms.pack(raw_buffer, &request, sizeof(request));
        send_successful = s_comms.send_packet(raw_buffer, 2);
    }

    if (send_successful)
    {
        uint8_t size = sizeof(data::sensor::First_Config_Response);
        uint8_t raw_buffer[packet_raw_size(size)];
        uint8_t* buffer = s_comms.receive_packet(raw_buffer, size, 500);
        if (buffer)
        {
            data::sensor::Type type = static_cast<data::sensor::Type>(s_comms.get_rx_packet_type(buffer));
            if (type == data::sensor::Type::FIRST_CONFIG_RESPONSE && size == sizeof(data::sensor::First_Config_Response))
            {
                s_last_input_dBm = s_comms.get_input_dBm();
                const data::sensor::First_Config_Response* ptr = reinterpret_cast<const data::sensor::First_Config_Response*>(s_comms.get_rx_packet_payload(buffer));
                return apply_first_config(*ptr);
            }
        }
    }
    return false;
}

//////////////////////////////////////////////////////////////////////////////////////////

static void first_config_state()
{
    LOG(PSTR(">>> Entering first config state\n"));
    
    while (s_state == State::FIRST_CONFIG)
    {
        if (is_pressed(Button::BUTTON1))
        {
            LOG(PSTR("Hold for 1 second to pair again..."));
            chrono::time_ms start_tp = chrono::now();
            while (is_pressed(Button::BUTTON1))
            {
                if (chrono::now() - start_tp > chrono::millis(1000))
                {
                    LOG(PSTR("...repairing.\n"));
                    s_state = State::PAIR;
                    break;
                }
            }
            if (s_state == State::FIRST_CONFIG)
            {
                LOG(PSTR("...cancelled.\n"));
            }
            continue;
        }      

        set_led(Led::Yellow);
        sleep_exact(chrono::millis(100));
        set_led(Led::None);

        bool got_it = request_first_config();
        s_comms.sleep_mode();

        if (got_it)
        {
            LOG(PSTR("Received first config. Starting measuring.\n"));
            set_led(Led::Green);
            sleep_exact(chrono::millis(100));
            set_led(Led::None);
            s_state = State::MEASUREMENT_LOOP;
        }
        else
        {
            set_led(Led::Red);
            sleep_exact(chrono::millis(100));
            set_led(Led::None);
            sleep(chrono::millis(10000), true);
        }
    }
    LOG(PSTR("<<< Exiting first config state\n"));
}

//////////////////////////////////////////////////////////////////////////////////////////

static void measurement_loop_state()
{
    LOG(PSTR(">>> Entering measuring loop state\n"));
    while (true)
    { 
        bool force_comms = false;
        if (is_pressed(Button::BUTTON1))
        {
            LOG(PSTR("Hold for 10 seconds to reset..."));
            chrono::time_ms start_tp = chrono::now();
            while (is_pressed(Button::BUTTON1))
            {
                if (chrono::now() - start_tp > chrono::millis(10000))
                {
                    LOG(PSTR("...resetting.\n"));
                    reset_settings();
                    soft_reset();
                }
            }
            LOG(PSTR("...cancelled.\n"));
            force_comms = true;
        }

        chrono::time_ms now = chrono::now();

        if (now >= s_next_measurement_time_point)
        {
            //check if we have to skip indices
            //we can do this only with an empty storage
            //so in case the storage is not empty, skip measurements.
            bool measure = true;
            if (s_first_stored_measurement_index < s_baseline_measurement_index)
            {
                if (s_storage.empty())
                {
                    LOG(PSTR("Skipping indices\n"));
                    s_first_stored_measurement_index = s_baseline_measurement_index;
                }
                else
                {
                    measure = false;
                    LOG(PSTR("Skipping measurement until storage empty due to index skipping!\n"));
                }
            }

            s_next_measurement_time_point += s_measurement_period;

            if (measure)
            {
                set_led(Led::Green);
                do_measurement();
                set_led(Led::None);
            }
        }

        if (force_comms || now >= s_next_comms_time_point)
        {
            if (!force_comms)
            {
                s_next_comms_time_point += s_comms_period;
            }

            set_led(Led::Yellow);
            sleep_exact(chrono::millis(100));
            set_led(Led::None);
            
            do_comms();
        }

        if (s_sensor_sleeping == true)
        {
            LOG(PSTR("Going to sleep!\n"));
            s_state = State::SLEEPING_LOOP;
            blink_led(Blink_Led::Green, 8, chrono::millis(50));
            break;
        }

        chrono::time_ms next_time_point = min(s_next_measurement_time_point, s_next_comms_time_point);
        chrono::millis sleep_duration(1000);

        now = chrono::now();
        if (next_time_point >= now)
        {
            sleep_duration = next_time_point - now;
        }
        else
        {
            LOG(PSTR("Timing error!\n"));
        }

        LOG(PSTR("tp: %lu, nm: %lu, nc: %lu\n"), uint32_t(now.ticks), uint32_t(s_next_measurement_time_point.ticks), uint32_t(s_next_comms_time_point.ticks));
        LOG(PSTR("Sleeping for %lu ms\n"), sleep_duration.count);

        chrono::micros remaining = sleep(sleep_duration, true);
        LOG(PSTR("Woken up, remaining %lu us\n"), remaining.count);
    }

    LOG(PSTR("<<< Exiting measuring loop state\n"));
}

//////////////////////////////////////////////////////////////////////////////////////////

static void sleep_loop_state()
{
    LOG(PSTR(">>> Entering sleep loop state\n"));
    //request the config again - this also informs the BS that we're sleeping
    request_config();
  
    while (true)
    {
        bool force_comms = false;
        if (is_pressed(Button::BUTTON1))
        {
            chrono::time_ms start_tp = chrono::now();
            while (is_pressed(Button::BUTTON1))
            {
                if (chrono::now() - start_tp > chrono::millis(10000))
                {
                    reset_settings();
                    soft_reset();
                }
            }
            force_comms = true;
        }

        chrono::time_us now = chrono::now_us();

        if (force_comms)
        {
            set_led(Led::Yellow);
            sleep_exact(chrono::millis(100));
            set_led(Led::None);
            
            request_config();
        }

        if (s_sensor_sleeping == false)
        {
            LOG(PSTR("Going to measurement!\n"));
            s_state = State::MEASUREMENT_LOOP;
            blink_led(Blink_Led::Yellow, 8, chrono::millis(50));
            request_config(); //report back the sleeping status
            break;
        }

        chrono::millis sleep_duration(1000ULL * 3600 * 24);
        LOG(PSTR("Sleeping for %lu ms\n"), sleep_duration.count);

        chrono::millis remaining = chrono::millis(sleep(sleep_duration, true));
        LOG(PSTR("Woken up, remaining %lu ms\n"), remaining.count);
    }

    LOG(PSTR("<<< Exiting sleep loop state\n"));
}

//////////////////////////////////////////////////////////////////////////////////////////

int main()
{
    setup();
    
    while (true)
    {
        if (s_state == State::PAIR)
        {
            pair_state();
        }
        else if (s_state == State::FIRST_CONFIG)
        {
            first_config_state();
        }
        else if (s_state == State::MEASUREMENT_LOOP)
        {
            measurement_loop_state();
        }
        else if (s_state == State::SLEEPING_LOOP)
        {
            sleep_loop_state();
        }
        else
        {
            blink_led(Blink_Led::Red, 2, chrono::millis(50));
            sleep(chrono::millis(1000), true);
        }      
    }
}

