#include "Scheduler.h"
#include <cassert>
#include <algorithm>
#include <iostream>

/*
void Scheduler::set_slot_duration(Clock::duration duration)
{
    m_slot_duration = std::max(Clock::duration(std::chrono::seconds(10)), duration);
}
Scheduler::Clock::duration Scheduler::get_slot_duration() const
{
    return m_slot_duration;
}

void Scheduler::set_measurement_period(Clock::duration period)
{
    m_measurement_period = std::max(Clock::duration(std::chrono::seconds(30)), period);
}
Scheduler::Clock::duration Scheduler::get_measurement_period(Clock::duration) const
{
    return m_measurement_period;
}

Scheduler::Slot_Id Scheduler::add_slot()
{
    Slot slot;
    slot.id = ++m_last_id;
    slot.next_time_point = Clock::now();
    m_slots.push_back(slot);
    return slot.id;
}

void Scheduler::remove_slot(Slot_Id id)
{
    auto it = std::find_if(m_slots.begin(), m_slots.end(), [id](const Slot& slot)
    {
        return slot.id == id;
    });
    if (it != m_slots.end())
    {
        std::cerr << "Cannot find slot id " << id << "\n";
        return;
    }
    m_slots.erase(it);
}

Scheduler::Clock::time_point Scheduler::get_next_slot_time_point()
{
    if (m_slots.empty())
    {
        return Clock::now() + m_measurement_period;
    }

    Clock::time_point max = Clock::time_point(Clock::duration::zero());
    for (const Slot& slot: m_slots)
    {
        max = std::max(max, slot.next_time_point);
    }
    return max + m_slot_duration;
}

Scheduler::Clock::duration Scheduler::get_all_slot_duration()
{
    return m_slot_duration * m_slots.size();
}

Scheduler::Clock::time_point Scheduler::schedule(Slot_Id id)
{
    auto it = std::find_if(m_slots.begin(), m_slots.end(), [id](const Slot& slot)
    {
        return slot.id == id;
    });
    assert(it != m_slots.end());

    if (it != m_slots.end())
    {
        Clock::time_point next = get_next_slot_time_point();
        it->next_time_point = next;
        return it->next_time_point;
    }

    return Clock::now() + std::chrono::hours(999999999);
}

//void Scheduler::check_in(Slot_Id id)
//{

//}

*/
