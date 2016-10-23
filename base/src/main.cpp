#include <iostream>
#include <sstream>
#include <thread>
#include <chrono>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <boost/thread.hpp>

#include "CRC.h"
#include "DB.h"
#include "Comms.h"
#include "Storage.h"
#include "Sensors_DB.h"
#include "Scheduler.h"
#include "Admin.h"

#include "DB.h"

#include "rfm22b/rfm22b.h"
#include <pigpio.h>


#define LOG(x) std::cout << x
#define LOG_LN(x) std::cout << x << std::endl

static Sensors_DB s_sensors_db;
static Admin s_admin(s_sensors_db);

static Comms s_comms;

typedef std::chrono::high_resolution_clock Clock;

extern void run_tests();
extern std::chrono::high_resolution_clock::time_point string_to_time_point(const std::string& str);
extern std::string time_point_to_string(std::chrono::high_resolution_clock::time_point tp);


///////////////////////////////////////////////////////////////////

std::chrono::microseconds PIGPIO_PERIOD(5);

static bool initialize_pigpio()
{
    static bool initialized = false;
    if (initialized)
    {
        return true;
    }

    LOG_LN("Initializing pigpio");
    if (gpioCfgClock(PIGPIO_PERIOD.count(), 1, 0) < 0 ||
            gpioCfgPermissions(static_cast<uint64_t>(-1)))
    {
        LOG_LN("Cannot configure pigpio");
        return false;
    }
    if (gpioInitialise() < 0)
    {
        LOG_LN("Cannot initialize pigpio");
        return false;
    }

    initialized = true;
    return true;
}

static bool shutdown_pigpio()
{
    gpioTerminate();
    return true;
}

////////////////////////////////////////////////////////////////////////

static bool powerup_rf()
{
    int sdn_gpio = 6;

    //configure the GPIOs

    int sdn_gpio_mode = gpioGetMode(sdn_gpio);
    if (sdn_gpio_mode == PI_BAD_GPIO)
    {
        LOG_LN("The SDN GPIO is bad");
        return false;
    }
    int res = gpioSetMode(sdn_gpio, PI_OUTPUT);
    if (res != 0)
    {
        LOG_LN("The SDN GPIO is bad");
        goto error;
    }
    //power on procedure
    res = gpioWrite(sdn_gpio, 1);
    if (res != 0)
    {
        LOG_LN("Failed to set SDN");
        goto error;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    res = gpioWrite(sdn_gpio, 0);
    if (res != 0)
    {
        LOG_LN("Failed to set SDN");
        goto error;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    return true;

error:
    gpioSetMode(sdn_gpio, sdn_gpio_mode);
    return false;
}

////////////////////////////////////////////////////////////////////////

void fill_config_packet(data::Config& packet, Sensors_DB::Sensor const& sensor)
{
    packet.base_time_point = chrono::time_s(Clock::to_time_t(Clock::now()));
    packet.measurement_period = chrono::seconds(std::chrono::duration_cast<std::chrono::seconds>(s_sensors_db.get_measurement_period()).count());
    packet.next_measurement_time_point = chrono::time_s(Clock::to_time_t(s_sensors_db.compute_next_measurement().first));
    packet.comms_period = chrono::seconds(std::chrono::duration_cast<std::chrono::seconds>(s_sensors_db.compute_comms_period()).count());
    packet.next_comms_time_point = chrono::time_s(Clock::to_time_t(s_sensors_db.compute_next_comms_time_point(sensor.id)));
    packet.last_confirmed_measurement_index = s_sensors_db.compute_last_confirmed_measurement_index(sensor.id);

    std::cout << "\tConfig for id: " << sensor.id
              << "\n\tbase time point: " << packet.base_time_point.ticks
              << "\n\tmeasurement period: " << packet.measurement_period.count
              << "\n\tnext measurement time point: " << packet.next_measurement_time_point.ticks
              << "\n\tcomms period: " << packet.comms_period.count
              << "\n\tnext comms time point: " << packet.next_comms_time_point.ticks
              << "\n\tlast confirmed measurement index: " << packet.last_confirmed_measurement_index
              << "\n";
}

////////////////////////////////////////////////////////////////////////

//static bool apply_config(std::string& error)
//{
//    if (!s_sensors.init(s_admin.get_db_server(), s_admin.get_db_name(), s_admin.get_db_username(), s_admin.get_db_password(), s_admin.get_db_port()))
//    {
//        error = "Sensors init failed";
//        return false;
//    }

//    return true;
//}

////////////////////////////////////////////////////////////////////////

int main(int, const char**)
{
    std::cout << "Starting...\n";

    srand(time(nullptr));

    if (!initialize_pigpio())
    {
        LOG_LN("Cannot initialize PIGPIO");
        exit(1);
    }

    if (!powerup_rf())
    {
        LOG_LN("Cannot power on the RF chip");
        exit(1);
    }

    size_t tries = 0;

    //    run_tests();

    if (!s_admin.init())
    {
        std::cerr << "Admin init failed\n";
        return -1;
    }

    tries = 0;
    while (!s_comms.init(1))
    {
        tries++;
        std::cerr << "comms init failed. Trying again: " << tries << "\n";
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    s_comms.set_source_address(Comms::BASE_ADDRESS);
    s_comms.set_destination_address(Comms::BROADCAST_ADDRESS);

    while (true)
    {
        //std::string cmd = read_fifo();
        //process_command(cmd);
        //client.process();

        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        uint8_t size = s_comms.receive_packet(1000);
        if (size > 0)
        {
            data::Type type = s_comms.get_packet_type();
            //LOG_LN("Received packed of " << (int)size << " bytes. Type: "<< (int)type);
            if (type == data::Type::PAIR_REQUEST && size == sizeof(data::Pair_Request))
            {
                Sensors_DB::Sensor const* sensor = s_sensors_db.add_expected_sensor();
                if (sensor)
                {
                    std::cout << "Adding sensor " << sensor->name << ", id " << sensor->id << ", address " << sensor->address << "\n";

                    data::Pair_Response packet;
                    packet.address = sensor->address;
                    //sprintf(packet.name, "sensor %d", packet.address);

                    s_comms.set_destination_address(Comms::BROADCAST_ADDRESS);
                    s_comms.begin_packet(data::Type::PAIR_RESPONSE);
                    s_comms.pack(packet);
                    if (s_comms.send_packet(1))
                    {
                        std::cout << "Pair successful\n";
                    }
                    else
                    {
                        s_sensors_db.revert_to_expected_sensor(sensor->id);
                        std::cerr << "Pair failed (comms)\n";
                    }
                    std::cout << "\n";
                }
                else
                {
                    std::cerr << "Pairing failed (db)\n";
                }
            }
            else if (type == data::Type::MEASUREMENT_BATCH && size >= data::MEASUREMENT_BATCH_PACKET_MIN_SIZE)
            {
                Sensors_DB::Sensor_Address address = s_comms.get_packet_source_address();
                const Sensors_DB::Sensor* sensor = s_sensors_db.find_sensor_by_address(address);
                if (sensor)
                {
                    std::cout << "Measurement batch reported by " << sensor->id << "\n";

                    data::Measurement_Batch batch;
                    memcpy(&batch, s_comms.get_packet_payload(), size);
                    if (size == data::MEASUREMENT_BATCH_PACKET_MIN_SIZE + batch.count * sizeof(data::Measurement))
                    {
                        std::cout << "\tIndices: " << batch.index << " - " << batch.index + batch.count - 1 << "\n";

                        Sensors_DB::Measurement m;
                        size_t count = std::min<size_t>(batch.count, data::Measurement_Batch::MAX_COUNT);
                        for (size_t i = 0; i < count; i++)
                        {
                            m.index = batch.index + i;
                            m.flags = batch.measurements[i].flags;
                            m.s2b_input_dBm = s_comms.get_input_dBm();
                            batch.measurements[i].unpack(m.vcc, m.humidity, m.temperature);

                            s_sensors_db.add_measurement(sensor->id, m);
                        }
                    }
                    else
                    {
                        std::cerr << "\tMalformed measurement batch data!\n";
                    }
                    std::cout << "\n";
                }
                else
                {
                    std::cerr << "\tSensor not found!\n";
                }
            }
            else if (type == data::Type::FIRST_CONFIG_REQUEST && size == sizeof(data::First_Config_Request))
            {
                Sensors_DB::Sensor_Address address = s_comms.get_packet_source_address();
                const Sensors_DB::Sensor* sensor = s_sensors_db.find_sensor_by_address(address);
                if (sensor)
                {
                    std::cout << "First config requested by " << sensor->id << "\n";

                    data::First_Config packet;
                    packet.first_measurement_index = s_sensors_db.compute_next_measurement().second;
                    s_sensors_db.set_sensor_measurement_range(sensor->id, packet.first_measurement_index, 0);
                    fill_config_packet(packet.config, *sensor);

                    s_comms.set_destination_address(address);
                    s_comms.begin_packet(data::Type::FIRST_CONFIG);
                    s_comms.pack(packet);
                    if (s_comms.send_packet(1))
                    {
                        std::cout << "\tFirst Config successful\n";
                    }
                    else
                    {
                        std::cerr << "\tFirst Config failed\n";
                    }
                    std::cout << "\n";
                }
                else
                {
                    std::cerr << "\tSensor not found!\n";
                }
            }
            else if (type == data::Type::CONFIG_REQUEST && size == sizeof(data::Config_Request))
            {
                data::Config_Request const& config_request = *reinterpret_cast<data::Config_Request const*>(s_comms.get_packet_payload());

                Sensors_DB::Sensor_Address address = s_comms.get_packet_source_address();
                const Sensors_DB::Sensor* sensor = s_sensors_db.find_sensor_by_address(address);
                if (sensor)
                {
                    std::cout << "Config requested by " << sensor->id << "\n";
                    std::cout << "\tStored range: " << config_request.first_measurement_index << " to " << config_request.first_measurement_index + config_request.measurement_count << " (" << config_request.measurement_count << " measurements) \n";

                    s_sensors_db.set_sensor_measurement_range(sensor->id, config_request.first_measurement_index, config_request.measurement_count);
                    s_sensors_db.set_sensor_b2s_input_dBm(sensor->id, config_request.b2s_input_dBm);

                    data::Config packet;
                    fill_config_packet(packet, *sensor);

                    s_comms.set_destination_address(address);
                    s_comms.begin_packet(data::Type::CONFIG);
                    s_comms.pack(packet);
                    if (s_comms.send_packet(1))
                    {
                        std::cout << "\tSchedule successful\n";
                    }
                    else
                    {
                        std::cerr << "\tSchedule failed\n";
                    }
                    std::cout << "\n";
                }
                else
                {
                    std::cerr << "\tSensor not found!\n";
                }
            }
        }
        std::cout << std::flush;
    }

    shutdown_pigpio();

    return 0;
}

