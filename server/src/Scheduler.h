#pragma once

#include <chrono>
#include <vector>

class Scheduler
{
public:
    typedef std::chrono::high_resolution_clock Clock;
    typedef uint32_t Slot_Id;

    void set_slot_duration(Clock::duration period);

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

    Clock::time_point get_next_time_point();
    Clock::duration get_all_slot_duration();

    Clock::duration m_slot_duration = std::chrono::seconds(10);
    Slot_Id m_last_id = 0;
};
