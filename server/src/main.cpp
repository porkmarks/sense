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
#include "Client.h"
#include "Server.h"


#define LOG(x) std::cout << x
#define LOG_LN(x) std::cout << x << std::endl

boost::asio::io_service s_io_service;

static Server s_server(s_io_service);

typedef std::chrono::high_resolution_clock Clock;


extern std::chrono::high_resolution_clock::time_point string_to_time_point(const std::string& str);
extern std::string time_point_to_string(std::chrono::high_resolution_clock::time_point tp);


////////////////////////////////////////////////////////////////////////

int main()
{
    std::cout << "Starting...\n";

    srand(time(nullptr));

    boost::shared_ptr<boost::asio::io_service::work> work(new boost::asio::io_service::work(s_io_service));
    boost::thread_group worker_threads;
    for(int x = 0; x < 8; ++x)
    {
      worker_threads.create_thread(boost::bind(&boost::asio::io_service::run, &s_io_service));
    }

    s_server.start();

    while (true)
    {
        s_server.process();
    }

    return 0;
}

