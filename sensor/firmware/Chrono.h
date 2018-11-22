#pragma once

#include <stdint.h>
#ifdef __AVR__
//#   include <Arduino.h>
#   include "Scope_Sync.h"
#   include <util/delay_basic.h>
#   include <stdio.h>
#else
#   include <thread>
#   include <chrono>
#endif


namespace chrono
{
struct millis;
struct micros;
struct seconds;
struct secondsf;

template<class Rep, class T>
struct duration
{
    typedef Rep rep_t;
    rep_t count;

    constexpr duration() : count(0) {}
    explicit constexpr duration(rep_t count) : count(count) {}
    constexpr duration(duration<Rep, T> const& other) = default;
    duration& operator=(duration<Rep, T> const& other) { count = other.count; return *this; }
    T operator-(T const& other) const { T x(count - other.count); return x; }
    T operator+(T const& other) const { T x(count + other.count); return x; }
    T operator*(rep_t scale) const { T x(count * scale); return x; }
    duration<Rep, T>& operator-=(T const& other) { count -= other.count; return *this; }
    duration<Rep, T>& operator+=(T const& other) { count += other.count; return *this; }
    duration<Rep, T>& operator*(rep_t scale) { count *= scale; return *this; }
    bool operator<(T const& other) const { return count < other.count; }
    bool operator<=(T const& other) const { return count <= other.count; }
    bool operator>(T const& other) const { return count > other.count; }
    bool operator>=(T const& other) const { return count >= other.count; }
    bool operator==(T const& other) const { return count == other.count; }
    bool operator!=(T const& other) const { return !operator==(other); }
};

struct millis : public duration<int32_t, millis>
{
    constexpr millis() = default;
    explicit constexpr millis(micros const& other);
    explicit constexpr millis(secondsf const& other);
    explicit constexpr millis(seconds const& other);
    explicit constexpr millis(rep_t count) : duration<int32_t, millis>(count) {}
};

struct micros : public duration<int32_t, micros>
{
    constexpr micros() = default;
    explicit constexpr micros(millis const& other) : duration<int32_t, micros>(other.count * 1000) {}
    explicit constexpr micros(secondsf const& other);
    explicit constexpr micros(seconds const& other);
    explicit constexpr micros(rep_t count) : duration<int32_t, micros>(count) {}
};

struct seconds : public duration<int32_t, seconds>
{
    constexpr seconds() = default;
    explicit constexpr seconds(millis const& other) : duration<int32_t, seconds>(other.count * 1000) {}
    explicit constexpr seconds(micros const& other) : duration<int32_t, seconds>(other.count * 1000000) {}
    explicit constexpr seconds(secondsf const& other);
    explicit constexpr seconds(rep_t count) : duration<int32_t, seconds>(count) {}
};

struct secondsf : public duration<float, secondsf>
{
    constexpr secondsf() = default;
    explicit constexpr secondsf(millis const& other) : duration<float, secondsf>(other.count * 0.001f) {}
    explicit constexpr secondsf(micros const& other) : duration<float, secondsf>(other.count * 0.000001f) {}
    explicit constexpr secondsf(seconds const& other) : duration<float, secondsf>(other.count) {}
    explicit constexpr secondsf(rep_t count) : duration<float, secondsf>(count) {}
};

inline constexpr millis::millis(micros const& other) : duration<int32_t, millis>(other.count / 1000) {}
inline constexpr millis::millis(secondsf const& other) : duration<int32_t, millis>(other.count * 1000.f) {}
inline constexpr millis::millis(seconds const& other) : duration<int32_t, millis>(other.count * 1000) {}
inline constexpr micros::micros(secondsf const& other) : duration<int32_t, micros>(other.count * 1000000.f) {}
inline constexpr micros::micros(seconds const& other) : duration<int32_t, micros>(other.count * 1000000) {}
inline constexpr seconds::seconds(secondsf const& other) : duration<int32_t, seconds>(other.count) {}

template<class Rep, class Duration>
struct time
{
    typedef Rep rep_t;
    typedef Duration duration_t;
    typedef time<Rep, Duration> time_t;
    rep_t ticks;

    time() : ticks(0) {}
    explicit time(rep_t ticks) : ticks(ticks) {}
    time(time_t const& other) : ticks(other.ticks) {}
    time_t& operator=(time_t const& other) { ticks = other.ticks; return *this; }
    time_t& operator=(volatile time_t const& other) { ticks = other.ticks; return *this; }
    time_t operator-(duration_t const& other) const { time_t x(ticks - other.count); return x; }
    time_t operator+(duration_t const& other) const { time_t x(ticks + other.count); return x; }
    time_t& operator-=(duration_t const& other) { ticks -= other.count; return *this; }
    time_t& operator+=(duration_t const& other) { ticks += other.count; return *this; }
    duration_t operator-(time_t const& other) const { duration_t x(ticks - other.ticks); return x; }
    duration_t operator-(volatile time_t const& other) const { duration_t x(ticks - other.ticks); return x; }
    bool operator<(time_t const& other) const { return ticks < other.ticks; }
    bool operator<=(time_t const& other) const { return ticks <= other.ticks; }
    bool operator>(time_t const& other) const { return ticks > other.ticks; }
    bool operator>=(time_t const& other) const { return ticks >= other.ticks; }
    bool operator==(time_t const& other) const { return ticks == other.ticks; }
    bool operator!=(time_t const& other) const { return !operator==(other); }

    Duration time_since_epoch() const { return Duration(static_cast<typename Duration::rep_t>(ticks)); }
};

typedef time<uint64_t, millis> time_ms;
typedef time<uint64_t, micros> time_us;
typedef time<uint32_t, seconds> time_s;


template<class D>
void delay(D duration)
{
    micros us(duration);
#ifdef __AVR__
    constexpr uint8_t cycles_per_us = F_CPU / 1000000ULL;
//    delayMicroseconds(us.count);
    uint64_t cycles = (us.count * cycles_per_us) >> 2; //the _delay_loop_2 executes 4 cycles per iteration
    while (cycles > 0)
    {
        uint16_t c = cycles > 65535 ? 65535 : (uint16_t)cycles;
        _delay_loop_2(c);
        cycles -= c;
    }
#else
    std::this_thread::sleep_for(std::chrono::microseconds(us.count));
#endif
}

#ifdef __AVR__
extern volatile time_us s_time_point;
static constexpr micros k_period(31250ULL);
#else
static constexpr micros k_period(1ULL);
#endif


inline time_ms now()
{
#ifdef __AVR__
    Scope_Sync ss;
    return time_ms(s_time_point.ticks / 1000);
#else
    return time_ms(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
#endif
}
inline time_us now_us()
{
#ifdef __AVR__
    Scope_Sync ss;
    return time_us(s_time_point.ticks);
#else
    return time_us(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
#endif
}

}
