#pragma once

#include <string>
#include <chrono>
#include <iostream>

extern std::string time_point_to_string(std::chrono::system_clock::time_point tp);

#define LOGI std::cout << time_point_to_string(std::chrono::system_clock::now()) << ": "
#define LOGE std::cerr << time_point_to_string(std::chrono::system_clock::now()) << ": " << __FILE__ << ":" << __LINE__ << ":: "

#define LOG(...) do { printf_P(PSTR("%s: "),  time_point_to_string(std::chrono::system_clock::now()).c_str()); printf_P(__VA_ARGS__); } while(false)
