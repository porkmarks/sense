#include <iostream>
#include <chrono>
#include <random>
#include <time.h>

using namespace std;

int main(int argc, const char* argv[])
{
    if (argc < 3)
    {
        std::cout << "Run as " << argv[0] << " period_days interval_minutes\n";
        exit(1);
    }

    auto period_days = std::chrono::hours(24 * atoi(argv[1]));
    auto interval_minutes = std::chrono::minutes(atoi(argv[2]));

    if (period_days.count() == 0 || interval_minutes.count() == 0)
    {
        std::cout << "Bad arguments...\n";
        std::cout << "Run as " << argv[0] << " period_days interval_minutes\n";
        exit(1);
    }

    std::cout << "idx\tdate\ttemperature\thumidity\n";

    auto end_tp = std::chrono::system_clock::now();
    auto start_tp = end_tp - period_days;


    float temp = 22.f;
    float hum  = 50.f;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dist(-0.1, 0.1);

    auto tp = start_tp;
    size_t index = 1;
    while (tp < end_tp)
    {
        //print
        //22	2015-10-22	26	60

        char tp_str[64];
        auto rawtime = std::chrono::system_clock::to_time_t(tp);
        struct tm* timeinfo = localtime (&rawtime);
        std::strftime(tp_str, sizeof(tp_str), "%F %T", timeinfo);

        std::cout << index << "\t" << tp_str << "\t"  << temp << "\t"  << hum << "\n";

        float r = dist(gen);
        temp += r;

        r = dist(gen);
        hum  += r;

        tp += interval_minutes;
        index++;
    }

    return 0;
}

