#pragma once

#include <stdint.h>
#ifdef __AVR__
#   include <Arduino.h>
#   include "Scope_Sync.h"
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

    duration() : count(0) {}
    explicit duration(rep_t count) : count(count) {}
    duration(duration<Rep, T> const& other) = default;
    duration& operator=(duration<Rep, T> const& other) { count = other.count; return *this; }
    T operator-(T const& other) const { T x(count - other.count); return x; }
    T operator+(T const& other) const { T x(count + other.count); return x; }
    duration<Rep, T>& operator-=(T const& other) { count -= other.count; return *this; }
    duration<Rep, T>& operator+=(T const& other) { count += other.count; return *this; }
    bool operator<(T const& other) const { return count < other.count; }
    bool operator<=(T const& other) const { return count <= other.count; }
    bool operator>(T const& other) const { return count > other.count; }
    bool operator>=(T const& other) const { return count >= other.count; }
    bool operator==(T const& other) const { return count == other.count; }
    bool operator!=(T const& other) const { return !operator==(other); }
};

struct millis : public duration<int32_t, millis>
{
    millis() = default;
    explicit millis(micros const& other);
    explicit millis(secondsf const& other);
    explicit millis(seconds const& other);
    explicit millis(rep_t count) : duration<int32_t, millis>(count) {}
};

struct micros : public duration<int32_t, micros>
{
    micros() = default;
    explicit micros(millis const& other) : duration<int32_t, micros>(other.count * 1000) {}
    explicit micros(secondsf const& other);
    explicit micros(seconds const& other);
    explicit micros(rep_t count) : duration<int32_t, micros>(count) {}
};

struct seconds : public duration<int32_t, seconds>
{
    seconds() = default;
    explicit seconds(millis const& other) : duration<int32_t, seconds>(other.count * 1000) {}
    explicit seconds(micros const& other) : duration<int32_t, seconds>(other.count * 1000000) {}
    explicit seconds(secondsf const& other);
    explicit seconds(rep_t count) : duration<int32_t, seconds>(count) {}
};

struct secondsf : public duration<float, secondsf>
{
    secondsf() = default;
    explicit secondsf(millis const& other) : duration<float, secondsf>(other.count * 0.001f) {}
    explicit secondsf(micros const& other) : duration<float, secondsf>(other.count * 0.000001f) {}
    explicit secondsf(seconds const& other) : duration<float, secondsf>(other.count) {}
    explicit secondsf(rep_t count) : duration<float, secondsf>(count) {}
};

inline millis::millis(micros const& other) : duration<int32_t, millis>(other.count / 1000) {}
inline millis::millis(secondsf const& other) : duration<int32_t, millis>(other.count * 1000.f) {}
inline millis::millis(seconds const& other) : duration<int32_t, millis>(other.count * 1000) {}
inline micros::micros(secondsf const& other) : duration<int32_t, micros>(other.count * 1000000.f) {}
inline micros::micros(seconds const& other) : duration<int32_t, micros>(other.count * 1000000) {}
inline seconds::seconds(secondsf const& other) : duration<int32_t, seconds>(other.count) {}

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


#ifdef __AVR__
template<class D>
void delay(D duration)
{
    millis ms(duration);
    ::delay(ms.count);
}

extern volatile time_us s_time_point;

inline time_ms now()
{
  /*
    static uint32_t s_last_millis = ::millis();
    uint32_t ms = ::millis();
    if (ms > s_last_millis)
    {
        s_time_point += millis(ms - s_last_millis);
    }
    s_last_millis = ms;
    return time_ms(s_time_point.ticks);
    */
    Scope_Sync ss;
    return time_ms(s_time_point.ticks / 1000);
}
inline time_us now_us()
{
  /*
    static uint32_t s_last_millis = ::millis();
    uint32_t ms = ::millis();
    if (ms > s_last_millis)
    {
        s_time_point += millis(ms - s_last_millis);
    }
    s_last_millis = ms;
    return time_ms(s_time_point.ticks);
    */
    Scope_Sync ss;
    return time_us(s_time_point.ticks);
}
#endif
}
