#ifndef TIME_MANAGER_H
#define TIME_MANAGER_H

#include <Arduino.h>
#include "time.h"

class TimeManager {
public:
    TimeManager();
    void begin(const char* tz_info);
    String getFormattedTime();
    bool isTimeSet();
    tm getCurrentTime(); // Get current time as tm struct

private:
    const char* ntpServer = "pool.ntp.org";
};

#endif // TIME_MANAGER_H