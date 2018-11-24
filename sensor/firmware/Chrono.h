#pragma once

#include <stdint.h>
#ifdef __AVR__
#   include <stdio.h>
#else
#   include <thread>
#   include <chrono>
#endif

namespace chrono
{
#ifdef __AVR__
extern uint32_t s_cpu_freq;
constexpr uint32_t k_timer1_div = 64;
constexpr uint32_t k_timer2_div = 1024; //the normal RTC div
extern uint32_t s_timer1_period_us;
extern uint32_t s_timer1_overflow_period_us;
constexpr uint32_t k_timer2_period_us = 1000000ULL * k_timer2_div / 31250ULL;
constexpr uint32_t k_max_timerh_increment = 8000000;
#endif

  
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

void delay_us(micros us);

template<class D>
void delay(D duration)
{
    delay_us(micros(duration));
}

#ifdef __AVR__
static constexpr micros k_period(31250ULL);
#else
static constexpr micros k_period(1ULL);
#endif


time_ms now();
time_us now_us();
void init_clock(uint32_t freq);
void calibrate();

}
