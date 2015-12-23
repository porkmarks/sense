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
uint32_t s_next_measurement_index = 0;
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

    //setup serial. 1.2Khz seems to be the best for 8Mhz
    Serial.begin(1200);
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

    s_next_measurement_index = config.next_measurement_index;

    //remove confirmed measurements
    while (s_first_measurement_index < config.last_confirmed_measurement_index)
    {
        if (!s_storage.pop_front())
        {
            LOG(PSTR("Failed to pop storage data. Left: %lu\n"), config.last_confirmed_measurement_index - s_first_measurement_index);
        }
        s_first_measurement_index++;
    };

    LOG(PSTR("base time: %lu\n"), config.base_time_point.ticks);
    LOG(PSTR("next comms: %lu\n"), config.next_comms_time_point.ticks);
    LOG(PSTR("comms period: %lu\n"), config.comms_period.count);
    LOG(PSTR("next measurement: %lu\n"), config.next_measurement_time_point.ticks);
    LOG(PSTR("measurement period: %lu\n"), config.measurement_period.count);
    LOG(PSTR("last confirmed index: %lu\n"), config.last_confirmed_measurement_index);
    LOG(PSTR("first index: %lu\n"), s_first_measurement_index);
    LOG(PSTR("next index: %lu\n"), s_next_measurement_index);

    return true;
}

//////////////////////////////////////////////////////////////////////////////////////////

bool request_config()
{
    s_comms.begin_packet(data::Type::CONFIG_REQUEST);
    s_comms.pack(data::Config_Request());
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

void pair()
{
    uint32_t addr = Comms::PAIR_ADDRESS_BEGIN + random() % (Comms::PAIR_ADDRESS_END - Comms::PAIR_ADDRESS_BEGIN);
    s_comms.set_address(addr);
    s_comms.set_destination_address(Comms::BASE_ADDRESS);

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

                    uint8_t size = s_comms.receive_packet(1000);
//                    if (size > 0)
//                    {
//                        LOG(F("Received packet of "));
//                        LOG(size);
//                        LOG(F(" bytes. Type: "));
//                        LOG_LN((int)s_comms.get_packet_type());
//                    }

                    if (size == sizeof(data::Pair_Response) && s_comms.get_packet_type() == data::Type::PAIR_RESPONSE)
                    {
                        const data::Pair_Response* response_ptr = reinterpret_cast<const data::Pair_Response*>(s_comms.get_packet_payload());
                        s_comms.set_address(response_ptr->address);
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

void do_measurement()
{
    s_next_measurement_index++;

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

    const chrono::millis COMMS_SLOT_DURATION = chrono::millis(3000);
    chrono::time_ms start_tp = chrono::now();
    chrono::time_ms now = chrono::now();
    uint32_t measurement_index = s_first_measurement_index;
    Storage::iterator it;
    do
    {
        if (s_storage.unpack_next(it) == false)
        {
            break;
        }

        data::Measurement item;

        //uint32_t time_point_s = (s_next_measurement_time_point_s / 1000ULL) + 1;

        //item.time_point = time_point_s - (s_next_measurement_index - s_first_measurement_index) * s_measurement_period_s;
        item.index = measurement_index++;
        item.flags = (it.data.humidity == 0) ? static_cast<uint8_t>(data::Measurement::Flag::SENSOR_ERROR) : 0;
        item.pack(it.data.vcc, it.data.humidity, it.data.temperature);

        s_comms.begin_packet(data::Type::MEASUREMENT);
        s_comms.pack(item);
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

        now = chrono::now();
    } while (now >= start_tp && now - start_tp < COMMS_SLOT_DURATION);

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

        if (request_config())
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
