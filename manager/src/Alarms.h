#pragma once

#include "DB.h"

class Alarms
{
public:
    Alarms(DB& db);

    bool create(std::string const& name);
    bool open(std::string const& name);

    bool save() const;
    bool saveAs(std::string const& filename) const;

    enum class Watch
    {
        None,
        Bigger,
        Smaller,
    };

    enum class Action
    {
        None,
        Email
    };


    struct Alarm
    {
        Watch temperatureWatch = Watch::None;
        float temperature = 0;

        Watch humidityWatch = Watch::None;
        float humidity = 0;

        bool vccWatch = false;
        uint8_t flags = 0;

        Action action = Action::None;
        std::string emailRecipient;
    };

    size_t getAlarmCount() const;
    Alarm const& getAlarm(size_t index) const;

private:
    DB& m_db;

    std::vector<Alarm> m_alarms;
};

