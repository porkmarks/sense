#include "Alarms.h"

Alarms::Alarms(DB& db)
    : m_db(db)
{

}

bool Alarms::create(std::string const& name)
{
    return false;
}

bool Alarms::open(std::string const& name)
{
    return false;
}

bool Alarms::save() const
{
    return false;
}

bool Alarms::saveAs(std::string const& filename) const
{
    return false;
}

size_t Alarms::getAlarmCount() const
{
    return m_alarms.size();
}

Alarms::Alarm const& Alarms::getAlarm(size_t index) const
{
    return m_alarms[index];
}
