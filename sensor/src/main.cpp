#include <Arduino.h>
#include "Wire.h"
#include <avr/eeprom.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/wdt.h>

#include "SPI.h"
#include "Low_Power.h"
#include "LEDs.h"
#include "Buttons.h"
#include "Battery.h"
#include "Sensor_Comms.h"
#include "Data_Defs.h"
#include "avr_stdio.h"
#include "Storage.h"

#define SENSOR_DHT22 1
#define SENSOR_SHT21 2

#define SENSOR_TYPE SENSOR_SHT21

#if SENSOR_TYPE == SENSOR_DHT22
#   include "DHT.h"
#elif SENSOR_TYPE == SENSOR_SHT21
#   include "SHT2x.h"
#endif


__extension__ typedef int __guard __attribute__((mode (__DI__)));

extern "C" int __cxa_guard_acquire(__guard *g) {return !*(char *)(g);};
extern "C" void __cxa_guard_release (__guard *g) {*(char *)g = 1;};
extern "C" void __cxa_guard_abort (__guard *) {};
//extern "C" void __cxa_pure_virtual() { while (1); }

#include <stdlib.h>
inline void* operator new(size_t size) { return malloc(size); }
inline void* operator new[](size_t size) { return malloc(size); }
inline void operator delete(void* p) { free(p); }
inline void operator delete[](void* p) { free(p); }
inline void* operator new(size_t size_, void *ptr_) { return ptr_; }


//////////////////////////////////////////////////////////////////////////////////////////

template<typename... Args>
void log(PGM_P fmt, Args... args)
{
    printf_P(fmt, args...);
}

#define LOG log
//#define LOG(...)

//////////////////////////////////////////////////////////////////////////////////////////



//////////////////////////////////////////////////////////////////////////////////////////

enum class State : uint8_t
{
    PAIR,
    FIRST_CONFIG,
    MEASUREMENT_LOOP
};
State s_state = State::PAIR;

uint8_t s_error_flags = 0;

#if SENSOR_TYPE == SENSOR_DHT22
//constexpr uint8_t DHT_DATA_PIN1 = 5;
//constexpr uint8_t DHT_DATA_PIN2 = 6;
//constexpr uint8_t DHT_DATA_PIN3 = 7;
//constexpr uint8_t DHT_DATA_PIN4 = 8;
//DHT s_dht(DHT_DATA_PIN1, DHT22);
#else
SHT2xClass s_sht;
#endif

Sensor_Comms s_comms;

FILE s_uart_output;

uint32_t s_first_measurement_index = 0; //the index of the first measurement in storage

namespace chrono
{
    time_ms s_time_point;
}

chrono::time_ms s_next_measurement_time_point;
chrono::millis s_measurement_period;

chrono::time_ms s_next_comms_time_point;
chrono::millis s_comms_period;

Storage s_storage;
data::sensor::Calibration s_calibration;

//////////////////////////////////////////////////////////////////////////////////////////

ISR(BADISR_vect)
{
    digitalWrite(GREEN_LED_PIN, LOW);
    while (true)
    {
        digitalWrite(RED_LED_PIN, HIGH);
        delay(5);
        digitalWrite(RED_LED_PIN, LOW);
        delay(95);
    }
}

void soft_reset()
{
    LOG(PSTR("********** resetting..."));
    //cli();
    //wdt_enable(WDTO_15MS);
    //while(1);
    void (*resetptr)( void ) = 0x0000;
    resetptr();
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

void stack_paint(void) __attribute__ ((naked)) __attribute__ ((section (".init1")));

void stack_paint(void)
{
#if 0
    uint8_t *p = &_end;

    while(p <= &__stack)
    {
        *p = STACK_CANARY;
        p++;
    }
#else
    __asm volatile ("    ldi r30,lo8(_end)\n"
                    "    ldi r31,hi8(_end)\n"
                    "    ldi r24,lo8(0xc5)\n" /* STACK_CANARY = 0xc5 */
                    "    ldi r25,hi8(__stack)\n"
                    "    rjmp .cmp\n"
                    ".loop:\n"
                    "    st Z+,r24\n"
                    ".cmp:\n"
                    "    cpi r30,lo8(__stack)\n"
                    "    cpc r31,r25\n"
                    "    brlo .loop\n"
                    "    breq .loop"::);
#endif
}

uint16_t initial_stack_size(void)
{
    return &__stack - &_end;
}

uint16_t stack_size(void)
{
    const uint8_t *p = &_end;
    uint16_t       c = 0;

    while(*p == STACK_CANARY && p <= &__stack)
    {
        p++;
        c++;
    }

    return c;
}

//////////////////////////////////////////////////////////////////////////////////////////

static const size_t MAX_NAME_LENGTH = 32;
static const uint8_t SETTINGS_VERSION = 1;

static uint8_t  EEMEM eeprom_version;
static uint32_t EEMEM eeprom_address;
static uint16_t EEMEM eeprom_temperature_bias;
static uint16_t EEMEM eeprom_humidity_bias;

void reset_settings()
{
    eeprom_write_byte(&eeprom_version, 0);
}

void save_settings(uint32_t address, data::sensor::Calibration const& calibration)
{
    eeprom_write_byte(&eeprom_version, SETTINGS_VERSION);
    eeprom_write_dword(&eeprom_address, address);
    eeprom_write_word(&eeprom_temperature_bias, *reinterpret_cast<uint16_t const*>(&calibration.temperature_bias));
    eeprom_write_word(&eeprom_humidity_bias, *reinterpret_cast<uint16_t const*>(&calibration.humidity_bias));
}

bool load_settings(uint32_t& address, data::sensor::Calibration& calibration)
{
    if (eeprom_read_byte(&eeprom_version) == SETTINGS_VERSION)
    {
        address = eeprom_read_dword(&eeprom_address);

        uint16_t v = eeprom_read_word(&eeprom_temperature_bias);
        calibration.temperature_bias = *reinterpret_cast<int16_t const*>(&v);

        v = eeprom_read_word(&eeprom_humidity_bias);
        calibration.humidity_bias = *reinterpret_cast<int16_t const*>(&v);
        return true;
    }
    else
    {
        reset_settings();
        return false;
    }
}

//////////////////////////////////////////////////////////////////////////////////////////

void setup()
{
    int mcusr_value = MCUSR;
    MCUSR = 0;

    //setup the led pins
    pinMode(RED_LED_PIN, OUTPUT);
    digitalWrite(RED_LED_PIN, LOW);
    pinMode(GREEN_LED_PIN, OUTPUT);
    digitalWrite(GREEN_LED_PIN, LOW);

    //setup serial.
    Serial.begin(38400);
    delay(200);

    fdev_setup_stream(&s_uart_output, uart_putchar, NULL, _FDEV_SETUP_WRITE);
    stdout = &s_uart_output;

    LOG(PSTR("******************** >> "));
    s_error_flags = 0;
    if (mcusr_value & (1<<PORF ))
    {
        s_error_flags |= static_cast<uint8_t>(data::sensor::Measurement::Flag::REBOOT_POWER_ON);
        LOG(PSTR("Power-on reset."));
    }
    if (mcusr_value & (1<<EXTRF))
    {
        s_error_flags |= static_cast<uint8_t>(data::sensor::Measurement::Flag::REBOOT_RESET);
        LOG(PSTR("External reset!"));
    }
    if (mcusr_value & (1<<BORF ))
    {
        s_error_flags |= static_cast<uint8_t>(data::sensor::Measurement::Flag::REBOOT_BROWNOUT);
        LOG(PSTR("Brownout reset!"));
    }
    if (mcusr_value & (1<<WDRF ))
    {
        s_error_flags |= static_cast<uint8_t>(data::sensor::Measurement::Flag::REBOOT_WATCHDOG);
        LOG(PSTR("Watchdog reset!"));
    }
    LOG(PSTR(" << ********************\n"));

    LOG(PSTR("Stack: initial %d, now: %d\n"), initial_stack_size(), stack_size());

    ///////////////////////////////////////////////
    //notify about startup
    blink_leds(GREEN_LED, 2, chrono::millis(100));

    ///////////////////////////////////////////////
    //setup buttons
    pinMode(static_cast<uint8_t>(Button::BUTTON1), INPUT);
    digitalWrite(static_cast<uint8_t>(Button::BUTTON1), HIGH);

    ///////////////////////////////////////////////
    //initialize the sensor
#if SENSOR_TYPE == SENSOR_DHT22
    s_dht.begin();
#else
    Wire.begin();
#endif
    delay(200);

    ///////////////////////////////////////////////
    //seed the RNG
    {
        float t, h;
#if SENSOR_TYPE == SENSOR_DHT22
        s_dht.read(t, h);
#else
        t = s_sht.GetTemperature();
        h = s_sht.GetHumidity();
#endif
        LOG(PSTR("Temperature: %d\nHumidity: %d\n"), (int)t, (int)h);
        float vcc = read_vcc();
        float s = vcc + t + h;
        uint32_t seed = reinterpret_cast<uint32_t&>(s);
        srandom(seed);
        LOG(PSTR("Random seed: %lu\n"), seed);
    }

    ///////////////////////////////////////////////
    //setup SPI
    pinMode(SS, OUTPUT);
    digitalWrite(SS, HIGH);
    delay(100);
    
    SPI.setDataMode(SPI_MODE0);
    SPI.setClockDivider(SPI_CLOCK_DIV2);
    SPI.begin();

    ///////////////////////////////////////////////

    LOG(PSTR("Starting rfm22b setup..."));
    
    while (!s_comms.init(5, 1))
    {
        LOG(PSTR("failed\n"));
        blink_leds(RED_LED, 3, chrono::millis(100));
        Low_Power::power_down(chrono::millis(5000));
    }
    s_comms.stand_by_mode();
    LOG(PSTR("done\n"));

    ///////////////////////////////////////////////
    //done
    blink_leds(GREEN_LED, 5, chrono::millis(50));

    //sleep a bit
    Low_Power::power_down(chrono::millis(1000));

    ///////////////////////////////////////////////
    //settings
    {
        uint32_t address = 0;
        if (load_settings(address, s_calibration))
        {
            s_comms.set_address(address);
            LOG(PSTR("Loaded settings. Addr: %lu\n"), address);
            s_state = State::FIRST_CONFIG;
        }
        else
        {
            s_state = State::PAIR;
        }
    }
}

//////////////////////////////////////////////////////////////////////////////////////////

static bool apply_config(const data::sensor::Config& config)
{
    if (config.measurement_period.count == 0 || config.comms_period.count == 0)
    {
        LOG(PSTR("Bag config received!!!\n"));
        return false;
    }

    data::sensor::Calibration calibration = s_calibration;
    calibration.temperature_bias += config.calibration_change.temperature_bias;
    calibration.humidity_bias += config.calibration_change.humidity_bias;
    if (calibration.temperature_bias != s_calibration.temperature_bias || calibration.humidity_bias != s_calibration.humidity_bias)
    {
        s_calibration = calibration;
        save_settings(s_comms.get_address(), s_calibration);
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
    while (s_first_measurement_index <= config.last_confirmed_measurement_index)
    {
        if (!s_storage.pop_front())
        {
            LOG(PSTR("Failed to pop storage data. Left: %ld\n"), config.last_confirmed_measurement_index - s_first_measurement_index);
            s_first_measurement_index = config.last_confirmed_measurement_index + 1;
            break;
        }
        s_first_measurement_index++;
    };

    uint32_t measurement_count = s_storage.get_data_count();

    LOG(PSTR("temp bias: %f\n"), s_calibration.temperature_bias / 100.f);
    LOG(PSTR("humidity bias: %f\n"), s_calibration.humidity_bias / 100.f);
    LOG(PSTR("next comms delay: %ld\n"), config.next_comms_delay.count);
    LOG(PSTR("comms period: %ld\n"), config.comms_period.count);
    LOG(PSTR("next measurement delay: %ld\n"), config.next_measurement_delay.count);
    LOG(PSTR("measurement period: %ld\n"), config.measurement_period.count);
    LOG(PSTR("last confirmed index: %lu\n"), config.last_confirmed_measurement_index);
    LOG(PSTR("first index: %lu\n"), s_first_measurement_index);
    LOG(PSTR("count: %lu\n"), measurement_count);

    return true;
}

//////////////////////////////////////////////////////////////////////////////////////////

static bool request_config()
{
    bool send_successful = false;
    {
        uint8_t raw_buffer[s_comms.get_payload_raw_buffer_size(sizeof(data::sensor::Config_Request))];
        s_comms.begin_packet(raw_buffer, data::sensor::Type::CONFIG_REQUEST);
        data::sensor::Config_Request req;
        req.first_measurement_index = s_first_measurement_index;
        req.measurement_count = s_storage.get_data_count();
        req.b2s_input_dBm = s_comms.get_input_dBm();
        req.calibration = s_calibration;
        s_comms.pack(raw_buffer, req);
        send_successful = s_comms.send_packet(raw_buffer, 5);
    }

    if (send_successful)
    {
        uint8_t size = sizeof(data::sensor::Config);
        uint8_t raw_buffer[s_comms.get_payload_raw_buffer_size(size)];
        uint8_t* buffer = s_comms.receive_packet(raw_buffer, size, 500);
        if (buffer)
        {
            data::sensor::Type type = s_comms.get_rx_packet_type(buffer);
            if (type == data::sensor::Type::CONFIG && size == sizeof(data::sensor::Config))
            {
                const data::sensor::Config* ptr = reinterpret_cast<const data::sensor::Config*>(s_comms.get_rx_packet_payload(buffer));
                return apply_config(*ptr);
            }
        }
    }
    return false;
}

//////////////////////////////////////////////////////////////////////////////////////////

static bool apply_first_config(const data::sensor::First_Config& first_config)
{
    s_first_measurement_index = first_config.first_measurement_index;
    s_storage.clear();

    if (!apply_config(first_config.config))
    {
        return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////////////////////

static bool request_first_config()
{
    bool send_successful = false;
    {
        uint8_t raw_buffer[s_comms.get_payload_raw_buffer_size(sizeof(data::sensor::First_Config_Request))];
        s_comms.begin_packet(raw_buffer, data::sensor::Type::FIRST_CONFIG_REQUEST);
        s_comms.pack(raw_buffer, data::sensor::First_Config_Request());
        send_successful = s_comms.send_packet(raw_buffer, 5);
    }

    if (send_successful)
    {
        uint8_t size = sizeof(data::sensor::First_Config);
        uint8_t raw_buffer[s_comms.get_payload_raw_buffer_size(size)];
        uint8_t* buffer = s_comms.receive_packet(raw_buffer, size, 500);
        if (buffer)
        {
            data::sensor::Type type = s_comms.get_rx_packet_type(buffer);
            if (type == data::sensor::Type::FIRST_CONFIG && size == sizeof(data::sensor::First_Config))
            {
                const data::sensor::First_Config* ptr = reinterpret_cast<const data::sensor::First_Config*>(s_comms.get_rx_packet_payload(buffer));
                return apply_first_config(*ptr);
            }
        }
    }
    return false;
}

//////////////////////////////////////////////////////////////////////////////////////////

static void pair()
{
    uint32_t addr = Sensor_Comms::PAIR_ADDRESS_BEGIN + random() % (Sensor_Comms::PAIR_ADDRESS_END - Sensor_Comms::PAIR_ADDRESS_BEGIN);
    s_comms.set_address(addr);
    s_comms.set_destination_address(Sensor_Comms::BASE_ADDRESS);

    LOG(PSTR("Starting pairing\n"));

    s_comms.stand_by_mode();

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
                set_leds(YELLOW_LED);

                //wait for the user to release the button
                wait_for_release(Button::BUTTON1);

                LOG(PSTR("Sending request..."));

                s_comms.idle_mode();

                bool send_successful = false;
                {
                    uint8_t raw_buffer[s_comms.get_payload_raw_buffer_size(sizeof(data::sensor::Pair_Request))];
                    s_comms.begin_packet(raw_buffer, data::sensor::Type::PAIR_REQUEST);
                    data::sensor::Pair_Request request;
                    request.calibration = s_calibration;
                    s_comms.pack(raw_buffer, request);
                    send_successful = s_comms.send_packet(raw_buffer, 5);
                }

                if (send_successful)
                {
                    LOG(PSTR("done.\nWaiting for response..."));

                    set_leds(GREEN_LED);

                    uint8_t size = sizeof(data::sensor::Pair_Response);
                    uint8_t raw_buffer[s_comms.get_payload_raw_buffer_size(size)];
                    uint8_t* buffer = s_comms.receive_packet(raw_buffer, size, 10000);
                    if (buffer)
                    {
                        LOG(PSTR("Received packet of %d bytes. Type %s\n"), (int)size, (int)s_comms.get_rx_packet_type(buffer));
                    }

                    if (buffer && size == sizeof(data::sensor::Pair_Response) && s_comms.get_rx_packet_type(buffer) == data::sensor::Type::PAIR_RESPONSE)
                    {
                        s_comms.stand_by_mode();

                        const data::sensor::Pair_Response* response_ptr = reinterpret_cast<const data::sensor::Pair_Response*>(s_comms.get_rx_packet_payload(buffer));
                        s_comms.set_address(response_ptr->address);
                        LOG(PSTR("done. Addr: %lu\n"), response_ptr->address);

                        save_settings(response_ptr->address, s_calibration);

                        done = true;
                        break;
                    }
                    else
                    {
                        s_comms.stand_by_mode();
                        LOG(PSTR("timeout\n"));
                    }
                }
                else
                {
                    LOG(PSTR("failed.\n"));
                }
                blink_leds(RED_LED, 3, chrono::millis(500));
            }

            s_comms.stand_by_mode();

            blink_leds(YELLOW_LED, 3, chrono::millis(100));
            Low_Power::power_down_int(chrono::millis(2000));
        }

        if (!done)
        {
            LOG(PSTR("Going to sleep..."));

            fade_out_leds(YELLOW_LED, chrono::millis(1000));

            //the user didn't press it - sleep for a loooong time
            Low_Power::power_down_int(chrono::millis(1000ULL * 3600 * 24));

            LOG(PSTR("woke up.\n"));

            fade_in_leds(YELLOW_LED, chrono::millis(1000));

            //we probably woken up because the user pressed the button - wait for him to release it
            wait_for_release(Button::BUTTON1);
        }
    }

    s_comms.stand_by_mode();

    blink_leds(GREEN_LED, 3, chrono::millis(500));

    //wait for the user to release the button
    wait_for_release(Button::BUTTON1);

    s_state = State::FIRST_CONFIG;

    //sleep a bit
    Low_Power::power_down(chrono::millis(1000 * 1));
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
#if SENSOR_TYPE == SENSOR_DHT22
        bool ok = s_dht.read(data.temperature, data.humidity);
        if (!ok)
        {
            LOG(PSTR("failed: DHT sensor!\n"));
        }
        else
        {
            LOG(PSTR("done\n"));
            break;
        }
#else
        data.temperature = s_sht.GetTemperature() + static_cast<float>(s_calibration.temperature_bias) / 100.f;
        data.humidity = s_sht.GetHumidity() + static_cast<float>(s_calibration.humidity_bias) / 100.f;
        break;
#endif
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

    data.vcc = read_vcc();

    //push back and make room if it fails
    LOG(PSTR("Storing..."));
    while (s_storage.push_back(data) == false)
    {
        LOG(PSTR("*"));
        s_storage.pop_front();
        s_first_measurement_index++;
    }
    LOG(PSTR("done\n"));
}

//////////////////////////////////////////////////////////////////////////////////////////

static void do_comms()
{
    {
        size_t count = s_storage.get_data_count();
        if (count == 0)
        {
            LOG(PSTR("No data to send!!!!\n"));
            return;
        }
        LOG(PSTR("%d items to send\n"), count);
    }

    s_comms.idle_mode();

    //send measurements
    {
        const chrono::millis COMMS_SLOT_DURATION = chrono::millis(5000);
        chrono::time_ms start_tp = chrono::now();

        uint8_t raw_buffer[s_comms.get_payload_raw_buffer_size(sizeof(data::sensor::Measurement_Batch))];
        data::sensor::Measurement_Batch& batch = *(data::sensor::Measurement_Batch*)s_comms.get_tx_packet_payload(raw_buffer);

        batch.start_index = s_first_measurement_index;
        batch.count = 0;

        bool done = false;
        Storage::iterator it;
        do
        {
            bool send = false;
            if (s_storage.unpack_next(it) == false)
            {
                done = true;
                send = true;
            }
            else
            {
                data::sensor::Measurement& item = batch.measurements[batch.count++];
                item.flags = s_error_flags;
                if (it.data.humidity < MIN_VALID_HUMIDITY)
                {
                    item.flags |= static_cast<uint8_t>(data::sensor::Measurement::Flag::SENSOR_ERROR);
                    item.pack(0.f, 0.f);
                    batch.pack(it.data.vcc);
                }
                else
                {
                    item.pack(it.data.humidity, it.data.temperature);
                    batch.pack(it.data.vcc);
                }
            }

            if (batch.count >= data::sensor::Measurement_Batch::MAX_COUNT)
            {
                send = true;
            }

            if (send && batch.count > 0)
            {
                LOG(PSTR("Sending measurement batch of %d..."), (int)batch.count);
                s_comms.begin_packet(raw_buffer, data::sensor::Type::MEASUREMENT_BATCH);
                if (s_comms.send_packet(raw_buffer, sizeof(data::sensor::Measurement_Batch), 3) == true)
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
            }

            chrono::time_ms now = chrono::now();
            if (now < start_tp || now - start_tp >= COMMS_SLOT_DURATION)
            {
                done = true;
            }

        } while (!done);
    }

    request_config();

    s_comms.stand_by_mode();
}

//////////////////////////////////////////////////////////////////////////////////////////

static void first_config()
{
    while (true)
    {
        if (is_pressed(Button::BUTTON1))
        {
            s_state = State::PAIR;
            break;
        }

        s_comms.idle_mode();

        if (request_first_config())
        {
            LOG(PSTR("Received first config. Starting measuring.\n"));
            s_state = State::MEASUREMENT_LOOP;
            break;
        }

        s_comms.stand_by_mode();
        Low_Power::power_down_int(chrono::millis(2000));
    }

    s_comms.stand_by_mode();
}

//////////////////////////////////////////////////////////////////////////////////////////

static void measurement_loop()
{
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

        chrono::time_ms now = chrono::now();

        if (now >= s_next_measurement_time_point)
        {
            s_next_measurement_time_point += s_measurement_period;

            set_leds(GREEN_LED);
            do_measurement();
            set_leds(0);
        }

        if (force_comms || now >= s_next_comms_time_point)
        {
            if (!force_comms)
            {
                s_next_comms_time_point += s_comms_period;
            }

            set_leds(YELLOW_LED);
            do_comms();
            set_leds(0);
        }

        chrono::time_ms next_time_point = min(s_next_measurement_time_point, s_next_comms_time_point);
        chrono::millis sleep_duration(1000);

        now = chrono::now();
        if (next_time_point > now)
        {
            sleep_duration = next_time_point - now;
        }
        else
        {
            LOG(PSTR("Timing error!\n"));
        }

        LOG(PSTR("tp: %ul, nm: %ul, nc: %ul\n"), uint32_t(now.ticks), uint32_t(s_next_measurement_time_point.ticks), uint32_t(s_next_comms_time_point.ticks));
        LOG(PSTR("Sleeping for %lu ms\n"), sleep_duration.count);

        chrono::millis remaining = Low_Power::power_down_int(sleep_duration);
        LOG(PSTR("Woken up, remaining %lu ms\n"), remaining.count);
    }
}

//////////////////////////////////////////////////////////////////////////////////////////

int main()
{
    init();
    setup();

    while (true)
    {
        if (s_state == State::PAIR)
        {
            pair();
        }
        else if (s_state == State::FIRST_CONFIG)
        {
            first_config();
        }
        else if (s_state == State::MEASUREMENT_LOOP)
        {
            measurement_loop();
        }
        else
        {
            blink_leds(RED_LED, 2, chrono::millis(50));
            Low_Power::power_down_int(chrono::millis(1000));
        }
    }
}
