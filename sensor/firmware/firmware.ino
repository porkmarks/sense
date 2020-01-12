//#include <Arduino.h>
//#include <HardwareSerial.h>
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
#include "Radio.h"
#include "Data_Defs.h"
#include "avr_stdio.h"
#include "Storage.h"
#include "Sleep.h"
#include "digitalWriteFast.h"
#include "i2c.h"
#include "Util.h"
#include "Settings.h"

#include "SHT2x.h"

__extension__ typedef int __guard __attribute__((mode (__DI__)));

//extern "C" int __cxa_guard_acquire(__guard *g) {return !*(char *)(g);};
//extern "C" void __cxa_guard_release (__guard *g) {*(char *)g = 1;};
//extern "C" void __cxa_guard_abort (__guard *) {};
//extern "C" void __cxa_pure_virtual() { while (1); }

#include <stdlib.h>
#include "Log.h"
/*
inline void* operator new(size_t size) { return malloc(size); }
inline void* operator new[](size_t size) { return malloc(size); }
inline void operator delete(void* p) { free(p); }
inline void operator delete[](void* p) { free(p); }
inline void* operator new(size_t size_, void *ptr_) { return ptr_; }
*/


// TIMER0 - LEDs
// TIMER1 - Clock
// TIMER2 - Sleep
// TIMER3 - Battery


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
#define SOFTWARE_VERSION  4

static uint8_t s_reboot_flags = 0;
static uint8_t s_comms_errors = 0;

static SHT2x s_sht;
static Radio s_radio;
static FILE s_uart_output;

static uint32_t s_first_stored_measurement_index = 0; //the index of the first measurement in storage

static chrono::time_ms s_next_measurement_time_point;
static chrono::millis s_measurement_period;

static chrono::time_ms s_next_comms_time_point;
static chrono::millis s_comms_period;

static int16_t s_last_input_dBm = 0;
static float s_last_batery_vcc_during_comms = 0;

static Storage s_storage;
static bool s_sensor_sleeping = false;
static Settings s_settings;
static Stable_Settings s_stable_settings;

constexpr float k_radio_min_battery_vcc = 2.0f; //radio works down to 1.8V
constexpr float k_sensor_min_battery_vcc = 2.3f; //sensor works down to 2.1V

//////////////////////////////////////////////////////////////////////////////////////////

ISR(BADISR_vect)
{
    while (true)
    {
      blink_led(Blink_Led::Red, 255, chrono::millis(50));
    }
    //soft_reset();
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
        *p = STACK_CANARY;
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

    while(*p == STACK_CANARY && p < (uint8_t*)&p)
    {
        p++;
        c++;
    }

    return c;
}

//////////////////////////////////////////////////////////////////////////////////////////

void init_gpio()
{
    //set ping to input pull high to make sure they don't float in sleep mode

    //PB
    pinModeFast(8, INPUT); digitalWriteFast(8, HIGH);  //PB0
    pinModeFast(9, INPUT); digitalWriteFast(9, HIGH);  //PB1
    pinModeFast(10, INPUT); digitalWriteFast(10, HIGH);  //PB2
    pinModeFast(11, INPUT); digitalWriteFast(11, HIGH);  //PB3
    pinModeFast(12, INPUT); digitalWriteFast(12, HIGH);  //PB4
    pinModeFast(13, INPUT); digitalWriteFast(13, HIGH);  //PB5

    //PC
    pinModeFast(14, INPUT); digitalWriteFast(14, HIGH);  //PC0
    pinModeFast(15, INPUT); digitalWriteFast(15, HIGH);  //PC1
    pinModeFast(17, INPUT); digitalWriteFast(17, HIGH);  //PC3

    //PD
    pinModeFast(7, INPUT); digitalWriteFast(7, HIGH);  //PD7

    //PE
    pinModeFast(25, INPUT); digitalWriteFast(25, HIGH);  //PE2 / ADC6
    pinModeFast(26, INPUT); digitalWriteFast(26, HIGH);  //PE3 / ADC7
}

void setup()
{
    int mcusr_value = MCUSR;
    MCUSR = 0;

    init_gpio();
    uart_init();

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
    init_sleep();
    chrono::init_clock(921600);

    set_led(Led::Green);
    chrono::calibrate();
    set_led(Led::None);

/*
    LOG(PSTR("0 %d\n"), (int)(EECR & bit(EERIE)));
    LOG(PSTR("1 %d\n"), (int)(XFDCSR & bit(XFDIE)));
    LOG(PSTR("2 %d\n"), (int)(WDTCSR & bit(WDIE)));
    LOG(PSTR("3 %d\n"), (int)(PCICR & bit(PCIE2)));
    LOG(PSTR("4 %d\n"), (int)(PCICR & bit(PCIE1)));
    LOG(PSTR("5 %d\n"), (int)(PCICR & bit(PCIE0)));
    LOG(PSTR("6 %d\n"), (int)(TIMSK0 & bit(OCIE0B)));
    LOG(PSTR("7 %d\n"), (int)(TIMSK0 & bit(OCIE0A)));
    LOG(PSTR("8 %d\n"), (int)(TIMSK0 & bit(TOIE0)));
    LOG(PSTR("9 %d\n"), (int)(TIMSK1 & bit(ICIE1)));
    LOG(PSTR("a %d\n"), (int)(TIMSK1 & bit(OCIE1B)));
    LOG(PSTR("b %d\n"), (int)(TIMSK1 & bit(OCIE1A)));
    LOG(PSTR("c %d\n"), (int)(TIMSK1 & bit(TOIE1)));
    LOG(PSTR("d %d\n"), (int)(TIMSK2 & bit(OCIE2B)));
    LOG(PSTR("e %d\n"), (int)(TIMSK2 & bit(OCIE2A)));
    LOG(PSTR("f %d\n"), (int)(TIMSK2 & bit(TOIE2)));
    LOG(PSTR("g %d\n"), (int)(TIMSK3 & bit(ICIE3)));
    LOG(PSTR("h %d\n"), (int)(TIMSK3 & bit(OCIE3B)));
    LOG(PSTR("i %d\n"), (int)(TIMSK3 & bit(OCIE3A)));
    LOG(PSTR("j %d\n"), (int)(TIMSK3 & bit(TOIE3)));
    LOG(PSTR("k %d\n"), (int)(TIMSK4 & bit(ICIE4)));
    LOG(PSTR("l %d\n"), (int)(TIMSK4 & bit(OCIE4B)));
    LOG(PSTR("m %d\n"), (int)(TIMSK4 & bit(OCIE4A)));
    LOG(PSTR("n %d\n"), (int)(TIMSK4 & bit(TOIE4)));
    LOG(PSTR("o %d\n"), (int)(SPCR0 & bit(SPIE)));
    LOG(PSTR("p %d\n"), (int)(SPCR1 & bit(SPIE)));
    LOG(PSTR("q %d\n"), (int)(UCSR0B & bit(RXCIE0)));
    LOG(PSTR("r %d\n"), (int)(UCSR0B & bit(TXCIE0)));
    LOG(PSTR("s %d\n"), (int)(UCSR0B & bit(UDRIE0)));
    LOG(PSTR("t %d\n"), (int)(UCSR1B & bit(RXCIE1)));
    LOG(PSTR("u %d\n"), (int)(UCSR1B & bit(TXCIE1)));
    LOG(PSTR("v %d\n"), (int)(UCSR1B & bit(UDRIE1)));
    LOG(PSTR("w %d\n"), (int)(TWCR0 & bit(TWIE)));
    LOG(PSTR("w %d\n"), (int)(TWCR1 & bit(TWIE)));
    LOG(PSTR("x %d\n"), (int)(ACSR & bit(ACIE)));
    LOG(PSTR("y %d\n"), (int)(ADCSRA & bit(ADIE)));
    LOG(PSTR("z %d\n"), (int)(SPMCSR & bit(SPMIE)));
    LOG(PSTR("0 %d\n"), (int)(EIMSK & bit(INT0)));
    LOG(PSTR("1 %d\n"), (int)(EIMSK & bit(INT1)));
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
    //while (true)
    {
      //printf_P(PSTR("%d, %d\n"), digitalReadFast(18), digitalReadFast(19));
      //chrono::delay(chrono::millis(20));
      sleep(true);
      for (int i = 0; i < 1; i++)
      {
        LOG(PSTR("%ld: %ld\n  "), loops, chrono::now().ticks);
        chrono::delay(chrono::millis(500));
      }
      loops++;
    }
*/
    ////////////////////////////////////////////

    LOG(PSTR("*** "));
    s_reboot_flags = 0;
    if (mcusr_value & bit(PORF))
    {
        s_reboot_flags |= static_cast<uint8_t>(data::sensor::Reboot_Flag::REBOOT_POWER_ON);
        LOG(PSTR("POR "));
    }
    if (mcusr_value & bit(EXTRF))
    {
        s_reboot_flags |= static_cast<uint8_t>(data::sensor::Reboot_Flag::REBOOT_RESET);
        LOG(PSTR("ER "));
    }
    if (mcusr_value & bit(BORF))
    {
        s_reboot_flags |= static_cast<uint8_t>(data::sensor::Reboot_Flag::REBOOT_BROWNOUT);
        LOG(PSTR("BOR "));
    }
    if (mcusr_value & bit(WDRF))
    {
        s_reboot_flags |= static_cast<uint8_t>(data::sensor::Reboot_Flag::REBOOT_WATCHDOG);
        LOG(PSTR("WR "));
    }
    if (s_reboot_flags == 0)
    {
        s_reboot_flags |= static_cast<uint8_t>(data::sensor::Reboot_Flag::REBOOT_UNKNOWN);
    }
    LOG(PSTR(" ***\n"));

    LOG(PSTR("Stack: %d -> %d\n"), initial_stack_size(), stack_size());

    

    ///////////////////////////////////////////////
    //initialize the bus and sensors
    while (!i2c_init())
    {
        LOG(PSTR("I2C F\n"));
        blink_led(Blink_Led::Red, 4, chrono::millis(200));
        float h;
        //try to read something
        //sometimes the sht21 gets stuck and we just have to read smth for it to release the lines
        s_sht.GetHumidity(h); 
        chrono::delay(chrono::millis(100));
        //reinit
        if (i2c_init())
        {
            break;
        }
        sleep(true);
    }
    chrono::delay(chrono::millis(50));

    init_battery();

    //init sensor
    while (true)
    {
        float t;
        float h;
        s_sht.GetHumidity(h);
        s_sht.GetTemperature(t);
        float vcc = read_battery(s_stable_settings.vref);
        s_last_batery_vcc_during_comms = vcc;
        LOG(PSTR("VCC: %ldmV\n"), (int32_t)(vcc*1000.f));
        LOG(PSTR("T: %ldm'C\n"), (int32_t)(t*1000.f));
        LOG(PSTR("H: %d%%\n"), (int)h);
        if (h > 0.1f && vcc > 1.5f)
        {
          break;
        }
        LOG(PSTR("Sensors F\n"));
        blink_led(Blink_Led::Red, 2, chrono::millis(200));
        sleep(true);
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
            float vcc = read_battery(s_stable_settings.vref);
            hash_combine(rnd_seed, *(uint32_t*)&vcc);
            hash_combine(rnd_seed, *(uint32_t*)&t);
            hash_combine(rnd_seed, *(uint32_t*)&h);
            chrono::delay(chrono::millis(10));
        }
        if (rnd_seed != 0)
        {
            srandom(rnd_seed);
            //LOG(PSTR("Seed: %lu\n"), rnd_seed);
            break;
        }

        LOG(PSTR("Seed F\n"));
        blink_led(Blink_Led::Red, 3, chrono::millis(200));
        sleep(true);
    }

    ///////////////////////////////////////////////
    // Start the RF
    while (!s_radio.init(5, 0))
    {
        LOG(PSTR("RF F\n"));
        blink_led(Blink_Led::Red, 4, chrono::millis(200));
        sleep(true);
    }
    s_radio.set_auto_sleep(true);
    s_radio.sleep();

/* 
    //comms testing
    {
        uint8_t raw_packet_data[packet_raw_size(Radio::MAX_USER_DATA_SIZE)];
        s_radio.set_destination_address(Radio::BROADCAST_ADDRESS);
        char data[20];
        memset(data, 0, 20);
        LOG(PSTR("Stack: %d -> %d\n"), initial_stack_size(), stack_size());

        while (true)
        {
            LOG(PSTR("Receiving...\n"));
            uint8_t size = Radio::MAX_USER_DATA_SIZE;
            uint8_t* packet_data = s_radio.receive_packet(raw_packet_data, size, chrono::millis(1000000ULL));
            if (packet_data)
            {
                uint32_t rcounter = *((uint32_t*)s_radio.get_rx_packet_payload(packet_data));
                LOG(PSTR("\t\tdone: %ld. Sending...\n"), rcounter);
                s_radio.begin_packet(raw_packet_data, 1, true);
                s_radio.pack(raw_packet_data, &rcounter, sizeof(rcounter));
                s_radio.pack(raw_packet_data, data, 20);
                bool ok = s_radio.send_packed_packet(raw_packet_data, true);
                LOG(PSTR("\t\t\t%s.\n"), (ok ? "done" : "failed"));
            }
            else
            {
                LOG(PSTR("\t\ttimeout\n"));
            }
        }
    }
//*/
    
    /*
    //send some test garbage for frequency measurements
    {
        uint8_t raw_packet_data[packet_raw_size(Radio::MAX_USER_DATA_SIZE)];
        s_radio.set_destination_address(Radio::BROADCAST_ADDRESS);
        while (true)
        {
          s_radio.begin_packet(raw_packet_data, 0, false);
          s_radio.send_packet(raw_packet_data, Radio::MAX_USER_DATA_SIZE, false);
        }
    }
    */

    ///////////////////////////////////////////////
    //done
    blink_led(Blink_Led::Green, 5, chrono::millis(50));

    ///////////////////////////////////////////////
    //settings
    {
        bool ok = load_stable_settings(s_stable_settings) & load_settings(s_settings);

        if (s_stable_settings.serial_number == 0)
        {
            s_stable_settings.serial_number = rnd_seed;
            save_stable_settings(s_stable_settings);
        }

        if (ok && s_settings.address >= Radio::SLAVE_ADDRESS_BEGIN)
        {
            s_radio.set_address(s_settings.address);
            s_state = State::FIRST_CONFIG;
        }
        else
        {
            s_state = State::PAIR;
        }
    }

    ///////////////////////////////////////////////
    //calibrate VCC
    if (is_pressed(Button::BUTTON1))
    {
      LOG(PSTR("Cal vref...\n"));
      bool calibration_done = false;
      float vref = s_stable_settings.vref / 100.f;
      float new_vref = vref;
      do
      {
        constexpr float k_calibration_voltage = 3.5f;
        float vcc = read_battery(vref);
        if (vcc >= k_calibration_voltage - 0.2f)
        {
            float x = vref * 1024.f / vcc;
            new_vref = k_calibration_voltage * x / 1024.f;
            LOG(PSTR("vcc %dmV vref %dmV->%dmV\n"), (int)(vcc*1000.f), (int)(vref*1000.f), (int)(new_vref*1000.f));
            calibration_done = true;
            vref = new_vref;
        }
        else
        {
          LOG(PSTR("vref cal skipped, vcc f: %dmV\n"), (int)(vcc*1000.f));
        }
      }
      while (is_pressed(Button::BUTTON1));
      
      uint8_t new_ivref = uint8_t(new_vref * 100.f + 0.5f);
      if (calibration_done && s_stable_settings.vref != new_ivref)
      {
        s_stable_settings.vref = new_ivref;
        save_stable_settings(s_stable_settings);
      }
    }

    ///////////////////////////////////////////////
    //battery guard
    battery_guard_auto(s_stable_settings.vref, k_radio_min_battery_vcc);

    LOG(PSTR("Type: %d\n"), SENSOR_TYPE);
    LOG(PSTR("HW Ver: %d\n"), HARDWARE_VERSION);
    LOG(PSTR("SW Ver: %d\n"), SOFTWARE_VERSION);
    LOG(PSTR("SN: %lx\n"), s_stable_settings.serial_number);
    LOG(PSTR("Addr: %u\n"), s_settings.address);
}

//////////////////////////////////////////////////////////////////////////////////////////

static bool apply_config(const data::sensor::Config_Response& config)
{
    if (config.measurement_period.count == 0 || config.comms_period.count == 0)
    {
        LOG(PSTR("Bag config recv!\n"));
        return false;
    }

    if (config.calibration.temperature_bias != s_stable_settings.calibration.temperature_bias || 
        config.calibration.humidity_bias != s_stable_settings.calibration.humidity_bias)
    {
        s_stable_settings.calibration = config.calibration;
        save_stable_settings(s_stable_settings);
    }

    s_comms_period = chrono::millis(config.comms_period.count * 1000LL);
    s_measurement_period = chrono::millis(config.measurement_period.count * 1000LL);

    chrono::time_ms now = chrono::now();
    s_next_comms_time_point = now + chrono::millis(config.next_comms_delay);
    s_next_measurement_time_point = now + chrono::millis(config.next_measurement_delay);

    //remove confirmed measurements
    while (s_first_stored_measurement_index <= config.last_confirmed_measurement_index)
    {
        if (!s_storage.pop_front())
        {
            LOG(PSTR("Storage pop failed. Left: %ld\n"), config.last_confirmed_measurement_index - s_first_stored_measurement_index);
            s_first_stored_measurement_index = config.last_confirmed_measurement_index + 1;
            break;
        }
        s_first_stored_measurement_index++;
    };

    uint32_t measurement_count = s_storage.get_data_count();
    s_sensor_sleeping = config.sleeping;

    s_radio.set_transmission_power(config.power);

    LOG(PSTR("T bias: %d\n"), (int)(s_stable_settings.calibration.temperature_bias));
    LOG(PSTR("H bias: %d\n"), (int)(s_stable_settings.calibration.humidity_bias));
    LOG(PSTR("next comms dt: %ld\n"), config.next_comms_delay.count);
    LOG(PSTR("comms per: %ld\n"), config.comms_period.count);
    LOG(PSTR("next meas dt: %ld\n"), config.next_measurement_delay.count);
    LOG(PSTR("meas per: %ld\n"), config.measurement_period.count);
    LOG(PSTR("last conf idx: %lu\n"), config.last_confirmed_measurement_index);
    LOG(PSTR("first stored idx: %lu\n"), s_first_stored_measurement_index);
    LOG(PSTR("cnt: %lu\n"), measurement_count);
    LOG(PSTR("sleep: %d\n"), s_sensor_sleeping ? 1 : 0);
    LOG(PSTR("pwr: %d\n"), (int)config.power);

    return true;
}

//////////////////////////////////////////////////////////////////////////////////////////

static void fill_config_request(data::sensor::Config_Request& request)
{
    request.reboot_flags = s_reboot_flags;
    request.comms_errors = s_comms_errors;
    request.first_measurement_index = s_first_stored_measurement_index;
    request.measurement_count = s_storage.get_data_count();
    request.b2s_qss = data::sensor::pack_qss(s_last_input_dBm);
    request.calibration = s_stable_settings.calibration;
    request.sleeping = s_sensor_sleeping;
    
    Storage::iterator it;
    if (s_storage.unpack_next(it) == true)
    {
        request.measurement.pack(it.data.humidity, it.data.temperature);
    }
    else
    {
        Battery_Monitor bm(s_stable_settings.vref, k_sensor_min_battery_vcc);
        float temperature = 0;
        float humidity = 0;
        if (s_sht.GetTemperature(temperature) && s_sht.GetHumidity(humidity))
        {
            temperature += static_cast<float>(s_stable_settings.calibration.temperature_bias) * 0.01f;
            humidity += static_cast<float>(s_stable_settings.calibration.humidity_bias) * 0.01f;
        }
        request.measurement.pack(humidity, temperature);
    }
    request.qvcc = data::sensor::pack_qvcc(s_last_batery_vcc_during_comms);
}

//////////////////////////////////////////////////////////////////////////////////////////

static bool request_config()
{
    Battery_Monitor bm(s_stable_settings.vref, s_last_batery_vcc_during_comms, k_radio_min_battery_vcc);
  
    bool send_successful = false;
    {
        uint8_t raw_buffer[packet_raw_size(sizeof(data::sensor::Config_Request))];
        s_radio.begin_packet(raw_buffer, static_cast<uint8_t>(data::sensor::Type::CONFIG_REQUEST), true);
        data::sensor::Config_Request request;
        fill_config_request(request);
        s_radio.pack(raw_buffer, &request, sizeof(request));
        send_successful = s_radio.send_packed_packet(raw_buffer, true);
    }

    if (send_successful)
    {
        s_reboot_flags = 0;
        s_comms_errors = 0;
      
        uint8_t size = sizeof(data::sensor::Config_Response);
        uint8_t raw_buffer[packet_raw_size(size)];
        uint8_t* buffer = s_radio.receive_packet(raw_buffer, size, chrono::millis(2000));
        if (buffer)
        {
            data::sensor::Type type = static_cast<data::sensor::Type>(s_radio.get_rx_packet_type(buffer));
            if (type == data::sensor::Type::CONFIG_RESPONSE && size == sizeof(data::sensor::Config_Response))
            {
                s_last_input_dBm = s_radio.get_last_input_dBm();
                const data::sensor::Config_Response* ptr = reinterpret_cast<const data::sensor::Config_Response*>(s_radio.get_rx_packet_payload(buffer));
                return apply_config(*ptr);
            }
        }
        else
        {
            s_comms_errors++;
        }
    }
    else
    {
        s_comms_errors++;
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////////////////////

static void pair_state()
{
    LOG(PSTR(">>> pair\n"));
    
    Radio::Address addr = Radio::PAIR_ADDRESS_BEGIN + random() % (Radio::PAIR_ADDRESS_END - Radio::PAIR_ADDRESS_BEGIN);
    s_radio.set_address(addr);
    s_radio.set_destination_address(Radio::BASE_ADDRESS);
    s_radio.set_frequency(869.f);

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

                Battery_Monitor bm(s_stable_settings.vref, s_last_batery_vcc_during_comms, k_radio_min_battery_vcc);
                
                LOG(PSTR("Send req..."));

                bool send_successful = false;
                {
                    uint8_t raw_buffer[packet_raw_size(sizeof(data::sensor::Pair_Request))];
                    s_radio.begin_packet(raw_buffer, static_cast<uint8_t>(data::sensor::Type::PAIR_REQUEST), true);
                    data::sensor::Pair_Request request;
                    request.descriptor.sensor_type = SENSOR_TYPE;
                    request.descriptor.hardware_version = HARDWARE_VERSION;
                    request.descriptor.software_version = SOFTWARE_VERSION;
                    request.descriptor.serial_number = s_stable_settings.serial_number;
                    request.calibration = s_stable_settings.calibration;
                    s_radio.pack(raw_buffer, &request, sizeof(request));
                    send_successful = s_radio.send_packed_packet(raw_buffer, true);
                }

                if (send_successful)
                {
                    LOG(PSTR("done.\nWait for resp..."));

                    uint8_t size = sizeof(data::sensor::Pair_Response);
                    uint8_t raw_buffer[packet_raw_size(size)];
                    uint8_t* buffer = s_radio.receive_packet(raw_buffer, size, chrono::millis(2000));

                    if (buffer && size == sizeof(data::sensor::Pair_Response) && 
                          s_radio.get_rx_packet_type(buffer) == static_cast<uint8_t>(data::sensor::Type::PAIR_RESPONSE))
                    {
                        s_last_input_dBm = s_radio.get_last_input_dBm();

                        const data::sensor::Pair_Response* response_ptr = reinterpret_cast<const data::sensor::Pair_Response*>(s_radio.get_rx_packet_payload(buffer));
                        s_radio.set_address(response_ptr->address);

                        LOG(PSTR("done. Addr %u\n"), response_ptr->address);

                        s_settings.address = response_ptr->address;
                        save_settings(s_settings);

                        blink_led(Blink_Led::Green, 3, chrono::millis(500));

                        done = true;
                        break;
                    }
                    else
                    {
                        LOG(PSTR("timeout\n"));
                    }
                }
                else
                {
                    LOG(PSTR("failed\n"));
                }
                blink_led(Blink_Led::Red, 3, chrono::millis(500));
            }

            blink_led(Blink_Led::Yellow, 3, chrono::millis(100));
            chrono::delay(chrono::millis(1000));
        }

        if (!done)
        {
            LOG(PSTR("Sleeping..."));

            //fade_out_leds(YELLOW_LED, chrono::millis(1000));

            //the user didn't press it - sleep for a loooong time
            do
            {
              sleep(true);
            } while (!is_pressed(Button::BUTTON1));

            chrono::calibrate();
            LOG(PSTR("done\n"));

            //fade_in_leds(YELLOW_LED, chrono::millis(1000));

            //we probably woken up because the user pressed the button - wait for him to release it
            wait_for_release(Button::BUTTON1);

            battery_guard_auto(s_stable_settings.vref, k_radio_min_battery_vcc);
        }
    }

    s_radio.set_frequency(868.f);

    blink_led(Blink_Led::Green, 3, chrono::millis(500));

    //wait for the user to release the button
    wait_for_release(Button::BUTTON1);

    s_state = State::FIRST_CONFIG;

    //sleep a bit
    chrono::delay(chrono::millis(500));

    LOG(PSTR("<<< pair\n"));
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
        LOG(PSTR("Meas %d..."), tries);
        Battery_Monitor bm(s_stable_settings.vref, k_sensor_min_battery_vcc);
        if (s_sht.GetTemperature(data.temperature) && s_sht.GetHumidity(data.humidity))
        {
            data.temperature += static_cast<float>(s_stable_settings.calibration.temperature_bias) * 0.01f;
            data.humidity += static_cast<float>(s_stable_settings.calibration.humidity_bias) * 0.01f;
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
    LOG(PSTR("store %ldm'C, %d%%..."), (int32_t)(data.temperature*1000.f), (int)data.humidity);
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
    //send measurements
    if (!s_storage.empty())
    {
        int8_t allowed_failures = 3;
        const chrono::millis COMMS_SLOT_DURATION = chrono::millis(7000);
        chrono::time_ms start_tp = chrono::now();

        uint8_t raw_buffer_size = packet_raw_size(sizeof(data::sensor::Measurement_Batch_Request));
        uint8_t raw_buffer[raw_buffer_size];
        memset(raw_buffer, 0, raw_buffer_size);
        data::sensor::Measurement_Batch_Request& batch = *(data::sensor::Measurement_Batch_Request*)s_radio.get_tx_packet_payload(raw_buffer);

        batch.start_index = s_first_stored_measurement_index;
        batch.count = 0;

        bool done = false;
        Storage::iterator it;
        do
        {
            bool send = false;
            if (s_storage.unpack_next(it))
            {
                data::sensor::Measurement& item = batch.measurements[batch.count++];
                item.pack(it.data.humidity, it.data.temperature);
                batch.qvcc = data::sensor::pack_qvcc(s_last_batery_vcc_during_comms);
                
                send = batch.count >= data::sensor::Measurement_Batch_Request::MAX_COUNT;
            }
            else
            {
                done = true;
                send = true;
            }

            chrono::time_ms now = chrono::now();
            if (now < start_tp || now - start_tp >= COMMS_SLOT_DURATION)
            {
                LOG(PSTR("Slot expired\n"));
                done = true;
            }

            if (send && batch.count > 0)
            {
                Battery_Monitor bm(s_stable_settings.vref, s_last_batery_vcc_during_comms, k_radio_min_battery_vcc);
                
                batch.last_batch = done ? 1 : 0;
                LOG(PSTR("Send batch of %d..."), (int)batch.count);
                s_radio.begin_packet(raw_buffer, static_cast<uint8_t>(data::sensor::Type::MEASUREMENT_BATCH_REQUEST), false);
                if (s_radio.send_packet(raw_buffer, sizeof(data::sensor::Measurement_Batch_Request), true) == true)
                {
                    LOG(PSTR("done.\n"));
                }
                else
                {
                    s_comms_errors++;
                    LOG(PSTR("failed: comms\n"));
                    if (allowed_failures-- <= 0)
                    {
                      break;
                    }
                }

                batch.start_index += batch.count;
                batch.count = 0;

                chrono::delay(chrono::millis(10));
            }
        } while (!done);
    }

    LOG(PSTR("done sending\n"));
    chrono::delay(chrono::millis(200));
    request_config();
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
    Battery_Monitor bm(s_stable_settings.vref, s_last_batery_vcc_during_comms, k_radio_min_battery_vcc);
    
    bool send_successful = false;
    {
        uint8_t raw_buffer[packet_raw_size(sizeof(data::sensor::First_Config_Request))];
        s_radio.begin_packet(raw_buffer, static_cast<uint8_t>(data::sensor::Type::FIRST_CONFIG_REQUEST), true);
        data::sensor::First_Config_Request request;
        fill_config_request(request);
        
        request.descriptor.sensor_type = SENSOR_TYPE;
        request.descriptor.hardware_version = HARDWARE_VERSION;
        request.descriptor.software_version = SOFTWARE_VERSION;
        request.descriptor.serial_number = s_stable_settings.serial_number;
        
        s_radio.pack(raw_buffer, &request, sizeof(request));
        send_successful = s_radio.send_packed_packet(raw_buffer, true);
    }

    if (send_successful)
    {
        uint8_t size = sizeof(data::sensor::First_Config_Response);
        uint8_t raw_buffer[packet_raw_size(size)];
        uint8_t* buffer = s_radio.receive_packet(raw_buffer, size, chrono::millis(2000));
        if (buffer)
        {
            data::sensor::Type type = static_cast<data::sensor::Type>(s_radio.get_rx_packet_type(buffer));
            if (type == data::sensor::Type::FIRST_CONFIG_RESPONSE && size == sizeof(data::sensor::First_Config_Response))
            {
                s_last_input_dBm = s_radio.get_last_input_dBm();
                const data::sensor::First_Config_Response* ptr = reinterpret_cast<const data::sensor::First_Config_Response*>(s_radio.get_rx_packet_payload(buffer));
                return apply_first_config(*ptr);
            }
        }
    }
    return false;
}

//////////////////////////////////////////////////////////////////////////////////////////

static void first_config_state()
{
    LOG(PSTR(">>> first config\n"));
    
    while (s_state == State::FIRST_CONFIG)
    {
        if (is_pressed(Button::BUTTON1))
        {
            LOG(PSTR("Hold for 1 second to pair..."));
            chrono::time_ms start_tp = chrono::now();
            while (is_pressed(Button::BUTTON1))
            {
                if (chrono::now() - start_tp > chrono::millis(1000))
                {
                    LOG(PSTR("repairing.\n"));
                    s_state = State::PAIR;
                    break;
                }
            }
            if (s_state == State::FIRST_CONFIG)
            {
                LOG(PSTR("cancelled.\n"));
            }
            continue;
        }      

        set_led(Led::Yellow);
        chrono::delay(chrono::millis(100));
        set_led(Led::None);

        bool got_it = request_first_config();
        if (got_it)
        {
            LOG(PSTR("First config done. Starting meas\n"));
            set_led(Led::Green);
            chrono::delay(chrono::millis(100));
            set_led(Led::None);
            s_state = State::MEASUREMENT_LOOP;
        }
        else
        {
            set_led(Led::Red);
            chrono::delay(chrono::millis(100));
            set_led(Led::None);
            sleep(true);
            chrono::calibrate();
        }
    }
    LOG(PSTR("<<< first config\n"));
}

//////////////////////////////////////////////////////////////////////////////////////////

static void measurement_loop_state()
{
    LOG(PSTR(">>> meas loop\n"));
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
                    LOG(PSTR("resetting.\n"));
                    reset_settings();
                    soft_reset();
                }
            }
            LOG(PSTR("cancelled.\n"));
            force_comms = true;
        }

        if (chrono::now() >= s_next_measurement_time_point)
        {
            s_next_measurement_time_point += s_measurement_period;

            set_led(Led::Green);
            do_measurement();
            set_led(Led::None);
        }

        if (force_comms || chrono::now() >= s_next_comms_time_point)
        {
            if (!force_comms)
            {
                s_next_comms_time_point += s_comms_period;
            }

            set_led(Led::Yellow);
            chrono::delay(chrono::millis(100));
            set_led(Led::None);
            
            do_comms();
        }

        if (s_sensor_sleeping == true)
        {
            s_state = State::SLEEPING_LOOP;
            blink_led(Blink_Led::Green, 8, chrono::millis(50));
            break;
        }

        chrono::time_ms now = chrono::now();
        LOG(PSTR("tp: %ld, nm: %ld, nc: %ld\n"), int32_t(now.ticks / 1000), int32_t(s_next_measurement_time_point.ticks / 1000), int32_t(s_next_comms_time_point.ticks / 1000));

        if (get_next_wakeup_time_point() > s_next_comms_time_point + chrono::millis(2000))
        {
            chrono::time_ms next = min(s_next_measurement_time_point, s_next_comms_time_point);
            chrono::millis dt = next - chrono::now();
            LOG(PSTR("Delay %ld ms...\n"), (int32_t)dt.count);
            if (dt.count > 0)
            {
                chrono::delay(dt);
            }
        }
        else
        {
            LOG(PSTR("Sleep..."));
            sleep(true);
        }
        
        chrono::calibrate();
        LOG(PSTR("done\n"));
    }

    LOG(PSTR("<<< measuring loop\n"));
}

//////////////////////////////////////////////////////////////////////////////////////////

static void sleep_loop_state()
{
    LOG(PSTR(">>> sleep loop\n"));
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

        if (force_comms)
        {
            set_led(Led::Yellow);
            chrono::delay(chrono::millis(100));
            set_led(Led::None);

            request_config();
        }

        if (s_sensor_sleeping == false)
        {
            s_state = State::MEASUREMENT_LOOP;

            blink_led(Blink_Led::Yellow, 8, chrono::millis(50));
            request_config(); //report back the sleeping status
            break;
        }

        LOG(PSTR("Sleeping..."));
        sleep(true);
        battery_guard_manual(s_stable_settings.vref, k_radio_min_battery_vcc);
        chrono::calibrate();
        LOG(PSTR("done\n"));
    }

    LOG(PSTR("<<< sleep loop\n"));
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
            sleep(true);
        }      
    }
}
