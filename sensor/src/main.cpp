#include <Arduino.h>
#include <avr/eeprom.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/wdt.h>

#include "SPI.h"
#include "Low_Power.h"
#include "DHT.h"
#include "LEDs.h"
#include "Buttons.h"
#include "Battery.h"
#include "Comms.h"
#include "Data_Defs.h"
#include "avr_stdio.h"
#include "deque"
#include "Storage.h"

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

enum class State
{
    PAIR,
    FIRST_CONFIG,
    MAIN_LOOP
};
State s_state = State::PAIR;

constexpr uint8_t DHT_DATA_PIN1 = 5;
constexpr uint8_t DHT_DATA_PIN2 = 6;
constexpr uint8_t DHT_DATA_PIN3 = 7;
constexpr uint8_t DHT_DATA_PIN4 = 8;
DHT s_dht(DHT_DATA_PIN1, DHT22);

Comms s_comms;

FILE s_uart_output;

//constexpr uint8_t MAX_MEASUREMENTS = 2;
//std::deque<data::Measurement> s_measurements;
uint32_t s_first_measurement_index = 0;

namespace chrono
{
    time_ms s_time_point;
}

chrono::time_s s_next_measurement_time_point;
chrono::seconds s_measurement_period;

chrono::time_s s_next_comms_time_point;
chrono::seconds s_comms_period;

Storage s_storage;

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
    LOG(PSTR("resetting..."));
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

    if (mcusr_value & (1<<PORF )) LOG(PSTR("Power-on reset.\n"));
    if (mcusr_value & (1<<EXTRF)) LOG(PSTR("External reset!\n"));
    if (mcusr_value & (1<<BORF )) LOG(PSTR("Brownout reset!\n"));
    if (mcusr_value & (1<<WDRF )) LOG(PSTR("Watchdog reset!\n"));

    ///////////////////////////////////////////////
    //notify about startup
    blink_leds(GREEN_LED, 2, chrono::millis(100));

    ///////////////////////////////////////////////
    //setup buttons
    pinMode(static_cast<uint8_t>(Button::BUTTON1), INPUT);
    digitalWrite(static_cast<uint8_t>(Button::BUTTON1), HIGH);

    ///////////////////////////////////////////////
    //initialize the sensor
    s_dht.begin();
    delay(200);

    ///////////////////////////////////////////////
    //seed the RNG
    {
        float t, h;
        s_dht.read(t, h);
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
    
    while (!s_comms.init(5))
    {
        LOG(PSTR("failed\n"));
        blink_leds(RED_LED, 3, chrono::millis(100));
        Low_Power::power_down(chrono::millis(5000));
    }
    LOG(PSTR("done\n"));

    ///////////////////////////////////////////////
    //done
    blink_leds(GREEN_LED, 5, chrono::millis(50));

    //sleep a bit
    Low_Power::power_down(chrono::millis(1000));
}

//////////////////////////////////////////////////////////////////////////////////////////

bool apply_config(const data::Config& config)
{
    if (config.measurement_period.count == 0 ||
            config.comms_period.count == 0 ||
            config.next_comms_time_point <= config.base_time_point ||
            config.next_measurement_time_point <= config.base_time_point)
    {
        LOG(PSTR("Bag config received!!!\n"));
        return false;
    }

    chrono::s_time_point = chrono::time_ms(config.base_time_point.ticks * 1000ULL);

    s_next_comms_time_point = config.next_comms_time_point;
    s_comms_period = config.comms_period;

    s_next_measurement_time_point = config.next_measurement_time_point;
    s_measurement_period = config.measurement_period;

    //remove confirmed measurements
    while (s_first_measurement_index <= config.last_confirmed_measurement_index)
    {
        if (!s_storage.pop_front())
        {
            LOG(PSTR("Failed to pop storage data. Left: %lu\n"), config.last_confirmed_measurement_index - s_first_measurement_index);
            s_first_measurement_index = config.last_confirmed_measurement_index + 1;
            break;
        }
        s_first_measurement_index++;
    };

    uint32_t measurement_count = s_storage.get_data_count();

    LOG(PSTR("base time: %lu\n"), config.base_time_point.ticks);
    LOG(PSTR("next comms: %lu\n"), config.next_comms_time_point.ticks);
    LOG(PSTR("comms period: %lu\n"), config.comms_period.count);
    LOG(PSTR("next measurement: %lu\n"), config.next_measurement_time_point.ticks);
    LOG(PSTR("measurement period: %lu\n"), config.measurement_period.count);
    LOG(PSTR("last confirmed index: %lu\n"), config.last_confirmed_measurement_index);
    LOG(PSTR("first index: %lu\n"), s_first_measurement_index);
    LOG(PSTR("count: %lu\n"), measurement_count);

    return true;
}

//////////////////////////////////////////////////////////////////////////////////////////

bool request_config()
{
    s_comms.begin_packet(data::Type::CONFIG_REQUEST);
    data::Config_Request req;
    req.first_measurement_index = s_first_measurement_index;
    req.measurement_count = s_storage.get_data_count();
    req.b2s_input_dBm = s_comms.get_input_dBm();
    s_comms.pack(req);
    if (s_comms.send_packet(5))
    {
        uint8_t size = s_comms.receive_packet(500);
        data::Type type = s_comms.get_packet_type();
        if (type == data::Type::CONFIG && size == sizeof(data::Config))
        {
            const data::Config* ptr = reinterpret_cast<const data::Config*>(s_comms.get_packet_payload());
            return apply_config(*ptr);
        }
    }
    return false;
}

//////////////////////////////////////////////////////////////////////////////////////////

bool apply_first_config(const data::First_Config& first_config)
{
    if (!apply_config(first_config.config))
    {
        return false;
    }

    s_first_measurement_index = first_config.first_measurement_index;
    s_storage.clear();

    LOG(PSTR("first index: %lu\n"), s_first_measurement_index);

    return true;
}

//////////////////////////////////////////////////////////////////////////////////////////

bool request_first_config()
{
    s_comms.begin_packet(data::Type::FIRST_CONFIG_REQUEST);
    s_comms.pack(data::First_Config_Request());
    if (s_comms.send_packet(5))
    {
        uint8_t size = s_comms.receive_packet(500);
        data::Type type = s_comms.get_packet_type();
        if (type == data::Type::FIRST_CONFIG && size == sizeof(data::First_Config))
        {
            const data::First_Config* ptr = reinterpret_cast<const data::First_Config*>(s_comms.get_packet_payload());
            return apply_first_config(*ptr);
        }
    }
    return false;
}

//////////////////////////////////////////////////////////////////////////////////////////

void pair()
{
    s_comms.set_address(Comms::BASE_ADDRESS);
    s_comms.set_check_address(Comms::BASE_ADDRESS);

    LOG(PSTR("Starting pairing\n"));

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

                s_comms.begin_packet(data::Type::PAIR_REQUEST);
                s_comms.pack(data::Pair_Request());
                if (s_comms.send_packet(5))
                {
                    LOG(PSTR("done.\nWaiting for response..."));

                    set_leds(GREEN_LED);

                    uint8_t size = s_comms.receive_packet(10000);
                    if (size > 0)
                    {
                        LOG(PSTR("Received packet of %d bytes. Type %s\n"), (int)size, (int)s_comms.get_packet_type());
                    }

                    if (size == sizeof(data::Pair_Response) && s_comms.get_packet_type() == data::Type::PAIR_RESPONSE)
                    {
                        const data::Pair_Response* response_ptr = reinterpret_cast<const data::Pair_Response*>(s_comms.get_packet_payload());
                        s_comms.set_address(response_ptr->address);
                        s_comms.set_check_address(response_ptr->address);
                        LOG(PSTR("done. Addr: %d\n"), response_ptr->address);

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
                    LOG(PSTR("failed.\n"));
                }
                blink_leds(RED_LED, 3, chrono::millis(500));
            }

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

    blink_leds(GREEN_LED, 3, chrono::millis(500));

    //wait for the user to release the button
    wait_for_release(Button::BUTTON1);

    s_state = State::FIRST_CONFIG;

    //sleep a bit
    Low_Power::power_down(chrono::millis(1000 * 1));
}

//////////////////////////////////////////////////////////////////////////////////////////

static constexpr float MIN_VALID_HUMIDITY = 5.f;

void do_measurement()
{
    Storage::Data data;

    // Reading temperature or humidity takes about 250 milliseconds!
    // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
    constexpr int8_t max_tries = 10;
    uint8_t tries = 0;
    do
    {
        LOG(PSTR("Measuring %d..."), tries);
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
    } while (++tries < max_tries);

    if (tries >= max_tries)
    {
        //the way to indicate error!
        data.humidity = 0;
        data.temperature = 0;
    }
    else
    {
        data.humidity = std::max(data.humidity, MIN_VALID_HUMIDITY);
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

void do_comms()
{
    size_t count = s_storage.get_data_count();
    if (count == 0)
    {
        LOG(PSTR("No data to send!!!!\n"));
        return;
    }

    LOG(PSTR("%d items to send\n"), count);

    s_comms.idle_mode();

    const chrono::millis COMMS_SLOT_DURATION = chrono::millis(5000);
    chrono::time_ms start_tp = chrono::now();
    chrono::time_ms now = chrono::now();

    data::Measurement_Batch batch;

    uint32_t measurement_index = s_first_measurement_index;
    batch.index = measurement_index;

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
            measurement_index++;
            data::Measurement& item = batch.measurements[batch.count++];

            if (it.data.humidity < MIN_VALID_HUMIDITY)
            {
                item.flags = static_cast<uint8_t>(data::Measurement::Flag::SENSOR_ERROR);
                item.pack(it.data.vcc, 0.f, 0.f);
            }
            else
            {
                item.flags = 0;
                item.pack(it.data.vcc, it.data.humidity, it.data.temperature);
            }
        }

        if (batch.count >= data::Measurement_Batch::MAX_COUNT)
        {
            send = true;
        }


        if (send)
        {
            s_comms.begin_packet(data::Type::MEASUREMENT_BATCH);
            s_comms.pack(&batch, data::MEASUREMENT_BATCH_PACKET_MIN_SIZE + batch.count * sizeof(data::Measurement));
            LOG(PSTR("Sending measurement..."));
            if (s_comms.send_packet(3) == true)
            {
                LOG(PSTR("done.\n"));
            }
            else
            {
                LOG(PSTR("failed: comms\n"));
                break;
            }

            batch.index = measurement_index;
            batch.count = 0;
        }

        now = chrono::now();
        if (now < start_tp || now - start_tp >= COMMS_SLOT_DURATION)
        {
            done = true;
        }

    } while (!done);

    request_config();

    s_comms.stand_by_mode();
}

//////////////////////////////////////////////////////////////////////////////////////////

void first_config()
{
    while (true)
    {
        if (is_pressed(Button::BUTTON1))
        {
            s_state = State::PAIR;
            break;
        }

        if (request_first_config())
        {
            LOG(PSTR("Received config:\ntime_point: %lu\n"), uint32_t(chrono::now().ticks / 1000ULL));
            s_state = State::MAIN_LOOP;
            break;
        }

        Low_Power::power_down_int(chrono::millis(2000));
    }
}

//////////////////////////////////////////////////////////////////////////////////////////

void main_loop()
{
    while (true)
    {
        if (is_pressed(Button::BUTTON1))
        {
            chrono::time_ms start_tp = chrono::now();
            while (is_pressed(Button::BUTTON1))
            {
                if (chrono::now() - start_tp > chrono::millis(10000))
                {
                    soft_reset();
                }
            }
        }

        chrono::time_s time_point = chrono::time_s((chrono::now().ticks / 1000ULL) + 1);

        if (time_point >= s_next_measurement_time_point)
        {
            s_next_measurement_time_point += s_measurement_period;

            set_leds(GREEN_LED);
            do_measurement();
            set_leds(0);
        }

        if (time_point >= s_next_comms_time_point)
        {
            s_next_comms_time_point += s_comms_period;

            set_leds(YELLOW_LED);
            do_comms();
            set_leds(0);
        }

        chrono::time_ms next_time_point = chrono::time_ms(std::min(s_next_measurement_time_point, s_next_comms_time_point).ticks * 1000ULL);
        chrono::millis sleep_duration(1000);

        chrono::time_ms now = chrono::now();
        if (next_time_point > now)
        {
            sleep_duration = next_time_point - now;
        }
        else
        {
            LOG(PSTR("Timing error!\n"));
        }

        //LOG(PSTR("tp: %lu, nm: %lu, nc: %lu\n"), uint32_t(s_time_point / 1000ULL), s_next_measurement_time_point_s, s_next_comms_time_point_s);
        LOG(PSTR("Sleeping for %lu ms"), sleep_duration.count);

        Low_Power::power_down_int(sleep_duration);
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
        else if (s_state == State::MAIN_LOOP)
        {
            main_loop();
        }
        else
        {
            blink_leds(RED_LED, 2, chrono::millis(50));
            Low_Power::power_down_int(chrono::millis(1000));
        }
    }
}
