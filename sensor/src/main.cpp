#include <Arduino.h>
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

////////////////////////////////////////////////////////////////////////////////////////////////////////

enum class State
{
    PAIR,
    READ_DATA
};
State s_state = State::PAIR;

constexpr uint8_t DHT_DATA_PIN1 = 5;
constexpr uint8_t DHT_DATA_PIN2 = 6;
constexpr uint8_t DHT_DATA_PIN3 = 7;
constexpr uint8_t DHT_DATA_PIN4 = 8;
DHT s_dht(DHT_DATA_PIN1, DHT22);

Comms s_comms;

constexpr uint8_t MAX_MEASUREMENTS = 32;
std::deque<data::Measurement> s_measurements;
uint32_t s_measurement_index = 0;
uint64_t s_timestamp = 0;
uint32_t s_sleep_duration = 10000;




void setup()
{
    pinMode(RED_LED_PIN, OUTPUT);
    digitalWrite(RED_LED_PIN, LOW);
    pinMode(GREEN_LED_PIN, OUTPUT);
    digitalWrite(GREEN_LED_PIN, LOW);

    pinMode(static_cast<uint8_t>(Button::BUTTON1), INPUT);
    digitalWrite(static_cast<uint8_t>(Button::BUTTON1), HIGH);

    //s_measurements.reserve(MAX_MEASUREMENTS);

    // put your setup code here, to run once:
    Serial.begin(2400);
    delay(500);

    FILE uart_output;
    fdev_setup_stream(&uart_output, uart_putchar, NULL, _FDEV_SETUP_WRITE);
    stdout = &uart_output;
    
    s_dht.begin();
    delay(500);

    //seed the RNG
    {
        float t, h;
        s_dht.read(t, h);
        float vcc = read_vcc();
        float s = vcc + t + h;
        uint32_t seed = *reinterpret_cast<uint32_t*>(&s);
        srandom(seed);
        Serial.print("Random seed: ");
        Serial.println(seed);
    }

    //  pinMode(SS, OUTPUT);
    //  SPI.begin();
    //  delay(100);
    
    //  SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
    //  delay(100);
    
    pinMode(SS, OUTPUT);
    digitalWrite(SS, HIGH);
    delay(100);
    
    SPI.setDataMode(SPI_MODE0);
    SPI.setClockDivider(SPI_CLOCK_DIV2);
    SPI.begin();
    
    Serial.println(F("Starting rfm22b setup..."));
    
    while (!s_comms.init(5))
    {
        Serial.println(F("init failed"));
        blink_leds(RED_LED, 3, 100);
        Low_Power::power_down(5000);
    }
    
    blink_leds(GREEN_LED, 5, 50);
    Low_Power::power_down(1000);
}

void pair()
{
    uint32_t addr = Comms::PAIR_ADDRESS_BEGIN + random() % (Comms::PAIR_ADDRESS_END - Comms::PAIR_ADDRESS_BEGIN);
    s_comms.set_address(addr);
    s_comms.set_destination_address(Comms::SERVER_ADDRESS);

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

                s_comms.begin_packet(data::Type::PAIR_REQUEST);
                s_comms.pack(data::Pair_Request());
                if (s_comms.send_packet(5))
                {
                    set_leds(GREEN_LED);

                    uint8_t size = s_comms.receive_packet(1000);
                    if (size == sizeof(data::Pair_Response) &&
                        s_comms.get_packet_type() == data::Type::PAIR_RESPONSE)
                    {
                        const data::Pair_Response* response_ptr = reinterpret_cast<const data::Pair_Response*>(s_comms.get_packet_payload());
                        s_comms.set_address(response_ptr->address);
                        //s_clock.set_timestamp(response.timestamp);
                        s_timestamp = response_ptr->server_timestamp * 1000ULL;
                        done = true;
                        break;
                    }
                }
                blink_leds(RED_LED, 3, 500);
            }

            blink_leds(YELLOW_LED, 3, 100);
            Low_Power::power_down_int(2000);
        }

        if (!done)
        {
            fade_out_leds(YELLOW_LED, 1000);

            //the user didn't press it - sleep for a loooong time
            Low_Power::power_down_int(1000ULL * 3600 * 24 * 30);

            fade_in_leds(YELLOW_LED, 1000);

            //we probably woken up because the user pressed the button - wait for him to release it
            wait_for_release(Button::BUTTON1);
        }
    }

    blink_leds(GREEN_LED, 3, 500);

    //wait for the user to release the button
    wait_for_release(Button::BUTTON1);

    s_state = State::READ_DATA;

    //sleep a bit
    Low_Power::power_down(1000 * 1);
}

void listen(uint32_t timeout)
{
    uint8_t size = s_comms.receive_packet(timeout);
    if (size == 0)
    {
        return;
    }

    data::Type type = s_comms.get_packet_type();
    if (type == data::Type::CONFIG && size == sizeof(data::Config))
    {
        const data::Config* ptr = reinterpret_cast<const data::Config*>(s_comms.get_packet_payload());
        s_timestamp = ptr->server_timestamp * 1000ULL;
        if (ptr->scheduled_timestamp > ptr->server_timestamp)
        {
            s_sleep_duration = ptr->scheduled_timestamp - ptr->server_timestamp;
        }
    }
}

void request_config()
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
            s_timestamp = ptr->server_timestamp * 1000ULL;
            if (ptr->scheduled_timestamp > ptr->server_timestamp)
            {
                s_sleep_duration = ptr->scheduled_timestamp - ptr->server_timestamp;
            }
        }
    }
}

void read_data()
{
    while (true)
    {
        uint32_t start_tp = millis();
        uint64_t start_sleep_tp = Low_Power::s_sleep_time;

        if (is_pressed(Button::BUTTON1))
        {
            s_state = State::PAIR;
            return;
        }

        uint8_t flags = 0;
        s_measurement_index++;

        //digitalWrite(DHT_POWER_PIN, HIGH);
        digitalWrite(RED_LED_PIN, HIGH);   // turn the LED on (HIGH is the voltage level)

        Serial.print(F("Awake for..."));
        uint32_t lastTP = millis();

        // Reading temperature or humidity takes about 250 milliseconds!
        // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
        float t, h;
        if (!s_dht.read(t, h))
        {
            flags |= data::Measurement::FLAG_SENSOR_ERROR;
            Serial.println(F("Failed to read from DHT sensor!"));
        }

        float vcc = read_vcc();

        uint32_t readingD = millis() - lastTP;
        lastTP = millis();

        //Serial.print(F("Humidity: "));
        //Serial.print(h);
        //Serial.print(F("% Temperature: "));
        //Serial.println(t);

        //Serial.println(F("Sending..."));

        s_comms.idle_mode();
        data::Measurement item(s_timestamp / 1000ULL, s_measurement_index, flags, vcc, h, t);

        //add it to the ring. IF needed, pop old elements
        if (s_measurements.size() + 1 > MAX_MEASUREMENTS)
        {
            s_measurements.pop_front();
        }
        s_measurements.push_back(item);

        for (uint8_t i = 0; i < s_measurements.size();)
        {
            data::Measurement& item = s_measurements[i];
            s_comms.begin_packet(data::Type::MEASUREMENT);
            s_comms.pack(item);
            if (s_comms.send_packet(3))
            {
                s_measurements.erase(s_measurements.begin() + i);
            }
            else
            {
                item.flags |= data::Measurement::FLAG_COMMS_FAILED;
                i++;
            }
        }

        request_config();
        //listen(500);

        uint32_t sendingD = millis() - lastTP;
        lastTP = millis();

        //Serial.println(F("Waiting for reply..."));
        {
//            uint8_t buf[RFM22B::MAX_PACKET_LENGTH];
//            int8_t len = rf22.receive(buf, RFM22B::MAX_PACKET_LENGTH, 100);
//            if (len > 0)
//            {
//                Serial.print(F("got reply: "));
//                Serial.println((char*)buf);
//            }
//            else
//            {
//                //Serial.println(F("nothing.."));
//            }
        }

        uint32_t listeningD = millis() - lastTP;


        Serial.print(readingD);
        Serial.print(F("ms, "));
        Serial.print(sendingD);
        Serial.print(F("ms, "));
        Serial.print(listeningD);
        Serial.println(F("ms"));

        s_comms.stand_by_mode();

        digitalWrite(RED_LED_PIN, LOW);    // turn the LED off by making the voltage LOW
        //delay(600);


        s_timestamp += millis() - start_tp;

        Low_Power::power_down_int(s_sleep_duration);
        s_timestamp += Low_Power::s_sleep_time - start_sleep_tp;
    }
}

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
        else if (s_state == State::READ_DATA)
        {
            read_data();
        }
        else
        {
            blink_leds(RED_LED, 2, 50);
            Low_Power::power_down_int(1000);
        }
    }
}
