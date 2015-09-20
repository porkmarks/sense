#include <Arduino.h>
#include <avr/pgmspace.h>
#include "SPI.h"
#include "rfm22b/rfm22b.h"
#include "Low_Power.h"
#include "DHT.h"
#include "Pack.h"

//#define DHT_POWER_PIN 8     // what pin we're connected to
#define DHT_DATA_PIN 6     // what pin we're connected to
#define DHT_TYPE DHT22   // DHT 22  (AM2302)

DHT dht(DHT_DATA_PIN, DHT_TYPE);

RFM22B rf22;

Low_Power LP;

#define RED_LED_PIN 7

uint32_t s_readingIndex = 0;


void signal_start()
{
  for (uint8_t i = 0; i < 5; i++)
  {
    digitalWrite(RED_LED_PIN, HIGH);
    delay(50);
    digitalWrite(RED_LED_PIN, LOW);
    delay(100);
  }

  digitalWrite(RED_LED_PIN, LOW);

  Serial.println(F("Starting..."));
}

float read_vcc()
{
  // Read 1.1V reference against AVcc
  // set the reference to Vcc and the measurement to the internal 1.1V reference
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);

  delay(2); // Wait for Vref to settle
  ADCSRA |= _BV(ADSC); // Start conversion
  while (bit_is_set(ADCSRA,ADSC)); // measuring

  uint8_t low  = ADCL; // must read ADCL first - it then locks ADCH
  uint8_t high = ADCH; // unlocks both

  int32_t result = (high<<8) | low;

  float vcc = 1.1f * 1023.f / result;//1125300L / result; // Calculate Vcc (in mV); 1125300 = 1.1*1023*1000
  return vcc;
}

static const PROGMEM uint32_t crc_table[16] =
{
    0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
    0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
    0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
    0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c
};

uint32_t crc_update(uint32_t crc, uint8_t data)
{
    byte tbl_idx;
    tbl_idx = crc ^ (data >> (0 * 4));
    crc = pgm_read_dword_near(crc_table + (tbl_idx & 0x0f)) ^ (crc >> 4);
    tbl_idx = crc ^ (data >> (1 * 4));
    crc = pgm_read_dword_near(crc_table + (tbl_idx & 0x0f)) ^ (crc >> 4);
    return crc;
}

uint32_t crc32(const uint8_t* data, size_t size)
{
  uint32_t crc = ~0L;
  while (size-- > 0)
  {
    crc = crc_update(crc, *data++);
  }
  crc = ~crc;
  return crc;
}


void setup()
{
  // put your setup code here, to run once:
  Serial.begin(2400);
  delay(500);

  dht.begin();
  delay(500);

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

  while (!rf22.init())
  {
    Serial.println(F("init failed"));
    delay(500);
  }

  rf22.set_carrier_frequency(434.f);
  rf22.set_modulation_type(RFM22B::Modulation_Type::GFSK);
  rf22.set_modulation_data_source(RFM22B::Modulation_Data_Source::FIFO);
  rf22.set_data_clock_configuration(RFM22B::Data_Clock_Configuration::NONE);
  rf22.set_transmission_power(20);
  rf22.set_gpio_function(RFM22B::GPIO::GPIO0, RFM22B::GPIO_Function::TX_STATE);
  rf22.set_gpio_function(RFM22B::GPIO::GPIO1, RFM22B::GPIO_Function::RX_STATE);

  Serial.print(F("Frequency is ")); Serial.println(rf22.get_carrier_frequency());
  Serial.print(F("FH Step is ")); Serial.println(rf22.get_frequency_hopping_step_size());
  Serial.print(F("Channel is ")); Serial.println(rf22.get_channel());
  Serial.print(F("Frequency deviation is ")); Serial.println(rf22.get_frequency_deviation());
  Serial.print(F("Data rate is ")); Serial.println(rf22.get_data_rate());
  Serial.print(F("Modulation Type ")); Serial.println((int)rf22.get_modulation_type());
  Serial.print(F("Modulation Data Source ")); Serial.println((int)rf22.get_modulation_data_source());
  Serial.print(F("Data Clock Configuration ")); Serial.println((int)rf22.get_data_clock_configuration());
  Serial.print(F("Transmission Power is ")); Serial.println(rf22.get_transmission_power());

  pinMode(RED_LED_PIN, OUTPUT);

  signal_start();
}

int main()
{
    init();
    setup();

    while (true)
    {
        // put your main code here, to run repeatedly:
        s_readingIndex++;

        //digitalWrite(DHT_POWER_PIN, HIGH);
        digitalWrite(RED_LED_PIN, HIGH);   // turn the LED on (HIGH is the voltage level)

        Serial.print(F("Awake for..."));
        uint32_t lastTP = millis();

        // Reading temperature or humidity takes about 250 milliseconds!
        // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
        float t, h;
        if (!dht.read(t, h))
        {
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

        rf22.idle_mode();
        uint8_t data[RFM22B::MAX_PACKET_LENGTH];
        size_t off = 0;
        pack(data, off, s_readingIndex);
        pack(data, off, static_cast<int16_t>(t*10.f));
        pack(data, off, static_cast<int16_t>(h*10.f));
        pack(data, off, static_cast<int16_t>(vcc*100.f));
        pack(data, off, crc32(data, off));
        rf22.send(data, off);

        uint32_t sendingD = millis() - lastTP;
        lastTP = millis();

        //Serial.println(F("Waiting for reply..."));
        {
            uint8_t buf[RFM22B::MAX_PACKET_LENGTH];
            int8_t len = rf22.receive(buf, RFM22B::MAX_PACKET_LENGTH, 100);
            if (len > 0)
            {
                Serial.print(F("got reply: "));
                Serial.println((char*)buf);
            }
            else
            {
                //Serial.println(F("nothing.."));
            }
        }

        uint32_t listeningD = millis() - lastTP;


        Serial.print(readingD);
        Serial.print(F("ms, "));
        Serial.print(sendingD);
        Serial.print(F("ms, "));
        Serial.print(listeningD);
        Serial.println(F("ms"));

        rf22.stand_by_mode();
        Serial.flush();

        digitalWrite(RED_LED_PIN, LOW);    // turn the LED off by making the voltage LOW
        //delay(600);

        LP.deep_sleep(10000);
    }
}
