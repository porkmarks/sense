#include <iostream>
#include <thread>
#include <chrono>
#include <cstring>
#include <string>
#include <stdio.h>
#include <time.h>

#include "rfm22b/rfm22b.h"

template<class T>
void unpack(uint8_t* buffer, size_t& off, T& val)
{
  memcpy(&val, buffer + off, sizeof(val));
  off += sizeof(val);
}

// Get current date/time, format is YYYY-MM-DD HH:mm:ss
std::string current_date_time()
{
    time_t     now = time(0);
    struct tm  tstruct;
    char       buf[80];
    tstruct = *localtime(&now);
    // Visit http://en.cppreference.com/w/cpp/chrono/c/strftime
    // for more information about date/time format
    strftime(buf, sizeof(buf), "%Y-%m-%d %X", &tstruct);

    return buf;
}

static const uint32_t crc_table[16] =
{
    0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
    0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
    0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
    0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c
};

uint32_t crc_update(uint32_t crc, uint8_t data)
{
    uint8_t tbl_idx;
    tbl_idx = crc ^ (data >> (0 * 4));
    crc = crc_table[(tbl_idx & 0x0f)] ^ (crc >> 4);
    tbl_idx = crc ^ (data >> (1 * 4));
    crc = crc_table[(tbl_idx & 0x0f)] ^ (crc >> 4);
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

int main()
{
    std::cout << "Starting...\n";

    RFM22B rf22;

    while (!rf22.init())
    {
      std::cout << "init failed \n";
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    std::cout << "Frequency is " << rf22.get_carrier_frequency() << std::endl;
    std::cout << "FH Step is " << rf22.get_frequency_hopping_step_size() << std::endl;
    std::cout << "Channel is " << rf22.get_channel() << std::endl;
    std::cout << "Frequency deviation is " << rf22.get_frequency_deviation() << std::endl;
    std::cout << "Data rate is " << rf22.get_data_rate() << std::endl;
    std::cout << "Modulation Type " << (int)rf22.get_modulation_type() << std::endl;
    std::cout << "Modulation Data Source " << (int)rf22.get_modulation_data_source() << std::endl;
    std::cout << "Data Clock Configuration " << (int)rf22.get_data_clock_configuration() << std::endl;
    std::cout << "Transmission Power is " << (int)rf22.get_transmission_power() << std::endl;

    rf22.set_carrier_frequency(434.f);
    rf22.set_modulation_type(RFM22B::Modulation_Type::GFSK);
    rf22.set_modulation_data_source(RFM22B::Modulation_Data_Source::FIFO);
    rf22.set_data_clock_configuration(RFM22B::Data_Clock_Configuration::NONE);
    rf22.set_transmission_power(5);
    rf22.set_gpio_function(RFM22B::GPIO::GPIO0, RFM22B::GPIO_Function::TX_STATE);
    rf22.set_gpio_function(RFM22B::GPIO::GPIO1, RFM22B::GPIO_Function::RX_STATE);

    std::cout << "Frequency is " << rf22.get_carrier_frequency() << std::endl;
    std::cout << "FH Step is " << rf22.get_frequency_hopping_step_size() << std::endl;
    std::cout << "Channel is " << rf22.get_channel() << std::endl;
    std::cout << "Frequency deviation is " << rf22.get_frequency_deviation() << std::endl;
    std::cout << "Data rate is " << rf22.get_data_rate() << std::endl;
    std::cout << "Modulation Type " << (int)rf22.get_modulation_type() << std::endl;
    std::cout << "Modulation Data Source " << (int)rf22.get_modulation_data_source() << std::endl;
    std::cout << "Data Clock Configuration " << (int)rf22.get_data_clock_configuration() << std::endl;
    std::cout << "Transmission Power is " << (int)rf22.get_transmission_power() << std::endl;

    size_t i = 0;
    while (true)
    {
        uint8_t data[RFM22B::MAX_PACKET_LENGTH + 1];
        int size = rf22.receive(data, RFM22B::MAX_PACKET_LENGTH, 1000);
        size_t off = 0;
        constexpr size_t packet_size = 4 + 2 + 2 + 2 + 4;
        if (size > 0 && size == packet_size)
        {
            uint32_t index;
            int16_t _t, _h, _vcc;
            uint32_t crc;
            unpack(data, off, index);
            unpack(data, off, _t);
            unpack(data, off, _h);
            unpack(data, off, _vcc);
            unpack(data, off, crc);

            uint32_t computed_crc = crc32(data, off - 4);
            if (crc == computed_crc)
            {
                float t = _t / 10.f;
                float h = _h / 10.f;
                float vcc = _vcc / 100.f;

                std::cout << current_date_time() << "\t" << ++i << " / " << index << ":\tT:" << t << "°C\tH:" << h << "%\tVcc:" << vcc << "V\n";
            }
            else
            {
                std::cout << "crc " << crc << " != computed crc " << computed_crc << " \n";
            }
        }
        else if (size > 0)
        {
            std::cout << "size " << size << " != packet size " << packet_size << " \n";
        }
        std::cout << std::flush;
    }


    return 0;
}

