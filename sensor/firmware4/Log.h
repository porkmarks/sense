#include <stdio.h>

#ifdef __AVR__
#define LOG(...) printf_P(__VA_ARGS__)
//#define LOG(...) do {} while(false)
#else
#include <string>
#include <chrono>
extern std::string time_point_to_string(std::chrono::system_clock::time_point tp);
#define LOG(...) do { printf_P(PSTR("%s: "),  time_point_to_string(std::chrono::system_clock::now()).c_str()); printf_P(__VA_ARGS__); } while(false)
#endif
