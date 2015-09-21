#include <iostream>
#include <thread>
#include <chrono>
#include <cstring>
#include <string>
#include <stdio.h>
#include <time.h>
#include "CRC.h"
#include "Comms.h"

#include "rfm22b/rfm22b.h"

static Comms s_comms;

uint32_t s_last_address = Comms::SLAVE_ADDRESS_BEGIN;


// Get current date/time, format is YYYY-MM-DD HH:mm:ss
std::string date_time(time_t tp)
{
    struct tm  tstruct;
    char       buf[80];
    tstruct = *localtime(&tp);
    // Visit http://en.cppreference.com/w/cpp/chrono/c/strftime
    // for more information about date/time format
    strftime(buf, sizeof(buf), "%Y-%m-%d %X", &tstruct);
    return buf;
}
std::string current_date_time()
{
    return date_time(time(nullptr));
}

int main()
{
    std::cout << "Starting...\n";

    while (!s_comms.init(5))
    {
      std::cout << "init failed \n";
      return -1;
    }

    s_comms.set_address(Comms::SERVER_ADDRESS);

    size_t i = 0;
    while (true)
    {
        uint8_t size = s_comms.receive_packet(1000);
        if (size > 0)
        {
            data::Type type = s_comms.get_packet_type();
            if (type == data::Type::PAIR_REQUEST && size == sizeof(data::Pair_Request))
            {
                data::Pair_Response packet;
                packet.address = s_last_address++;
                packet.timestamp = time(nullptr);
                s_comms.begin_packet(data::Type::PAIR_RESPONSE);
                s_comms.pack(packet);
                s_comms.send_packet(3);
            }
            else if (type == data::Type::MEASUREMENT && size == sizeof(data::Measurement))
            {
                const data::Measurement* packet_ptr = reinterpret_cast<const data::Measurement*>(s_comms.get_packet_payload());
                uint32_t timestamp;
                uint8_t index;
                float vcc, t, h;
                packet_ptr->unpack(timestamp, index, vcc, h, t);
                std::cout << date_time(timestamp) << " / " << current_date_time() << "\t" << int(index) << "\tVcc:" << vcc << "V\tH:" << h << "%\tT:" << t << "C" << "\n";
            }
        }
        std::cout << std::flush;
    }


    return 0;
}

