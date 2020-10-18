#include "Chrono.h"

#include <stdint.h>
#include "Log.h"
#ifdef __AVR__
#   include "RX8010S.h"
#   include "digitalWriteFast.h"
#   include "Arduino_Compat.h"
#   include "Scope_Sync.h"
#   include <util/delay_basic.h>
#   include <stdio.h>
#else
#   include <thread>
#   include <chrono>
#endif

namespace chrono
{
#ifdef __AVR__
volatile uint64_t s_time_point = 0;
uint32_t s_cpu_freq = F_CPU;
uint32_t s_timer1_period_us = 1000000ULL * k_timer1_div / s_cpu_freq;
uint32_t s_timer1_overflow_period_us = 1000000ULL * 65536ULL * k_timer1_div / s_cpu_freq;
#else
#endif
}

#ifdef __AVR__

ISR(TIMER1_OVF_vect)
{
  chrono::s_time_point += uint64_t(chrono::s_timer1_overflow_period_us);
}
#endif

namespace chrono
{


void delay_us(micros us)
{
#ifdef __AVR__
    if (us.count <= 3)
    {
      return;
    }
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
    if (us.count <= 0)
    {
      return;
    }
    std::this_thread::sleep_for(std::chrono::microseconds(us.count));
#endif
}

time_ms now()
{
#ifdef __AVR__
    Scope_Sync ss;
    return time_ms(now_us().ticks / 1000);
#else
    return time_ms(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
#endif
}
time_us now_us()
{
#ifdef __AVR__
    Scope_Sync ss;
    return time_us(s_time_point + uint64_t(TCNT1) * uint64_t(s_timer1_period_us));
#else
    return time_us(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
#endif
}

constexpr uint8_t k_min_osccal = 128;
constexpr uint8_t k_max_osccal = 255;

void init_clock(uint32_t freq)
{
#ifdef __AVR__
    s_cpu_freq = freq;
    s_timer1_period_us = 1000000ULL * uint64_t(k_timer1_div) / uint64_t(s_cpu_freq);
    s_timer1_overflow_period_us = 1000000ULL * 65536ULL * uint64_t(k_timer1_div) / uint64_t(s_cpu_freq);

    //hot calibration setup (timer1)
    TCCR1A = 0; //normal mode for timer 1
    TCCR1B = bit(CS11) | bit(CS10); //64 prescaler, ~15K ticks per second, overflow at ~4 seconds
    TCNT1 = 0;     // clear timer1 counter
    TIFR1 = 0xFF;
    TIMSK1 = bit(TOIE1);


    OSCCAL = (uint16_t(k_min_osccal) + uint16_t(k_max_osccal)) / 2; //starting value, center
    //OSCCAL = 191;
    //calibrate();
#endif
}

#ifdef __AVR__
void calibrate(RX8010S& rtc)
{
    chrono::micros rtc_period;
    if (!rtc.enablePeriodicTimer(RX8010S::Frequency::_4096Hz, 128, rtc_period))
    {
        return;
    }
    
    uint16_t target = rtc_period.count / s_timer1_period_us;
    uint16_t margin = target / 200; // 0.5% on both sides
    uint16_t target_max = target + margin;
    uint16_t target_min = target - margin;
    //LOG(PSTR("Target: %d, %d\n"), target_min, target_max);

    int count = 63;
    while (count-- > 0)
    {
        while (digitalReadFast(PD4) != 0);
        while (digitalReadFast(PD4) == 0);
        
        uint16_t t1_start = TCNT1;
        while (digitalReadFast(PD4) != 0);
        uint16_t t1_end = TCNT1;

        uint16_t d = (t1_end > t1_start) ? t1_end - t1_start : 65536 - t1_start + t1_end;
        if (d > target_max && OSCCAL > k_min_osccal) //too fast, decrease the OSCCAL
        {
            OSCCAL--;
            //LOG(PSTR("osccal-- %d, %d\n"), (int)OSCCAL, d);
        }
        else if (d < target_min && OSCCAL < k_max_osccal) //too slow, increase the OSCCAL
        {
            OSCCAL++;
            //LOG(PSTR("osccal++ %d, %d\n"), (int)OSCCAL, d);
        }
        else
        {
            //LOG(PSTR("osccal== %d, %d\n"), (int)OSCCAL, d);
            break;
        }
    }

    rtc.disablePeriodicTimer();
}
#endif

}
