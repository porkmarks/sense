#pragma once

#include <chrono>
#include <vector>

class Scheduler
{
public:
    typedef std::chrono::high_resolution_clock Clock;
    typedef uint32_t Slot_Id;

    void set_measurement_period(Clock::duration period);
    Clock::duration get_measurement_period() const;

    void set_slot_duration(Clock::duration period);
    Clock::duration get_slot_duration() const;

    Slot_Id add_slot();
    void remove_slot(Slot_Id id);

    Clock::time_point schedule(Slot_Id id);
    //void check_in(Slot_Id id);

private:

    struct Slot
    {
        Slot_Id id = 0;
        Clock::time_point next_time_point = Clock::time_point(Clock::duration::zero());
    };

    std::vector<Slot> m_slots;

    Clock::duration get_next_slot_delta();
    Clock::duration get_all_slot_duration();

    Clock::duration m_measurement_period = std::chrono::minutes(5);
    Clock::duration m_slot_duration = std::chrono::seconds(10);
    Slot_Id m_last_id = 0;
};
