#include "TimeManager.h"

TimeManager::TimeManager() {
}

void TimeManager::begin(const char* tz_info) {
    configTzTime(tz_info, ntpServer);
}

String TimeManager::getFormattedTime() {
    struct tm timeinfo;
    if(!getLocalTime(&timeinfo)){
        return "Time not set";
    }
    char buffer[32];
    strftime(buffer, sizeof(buffer), "%A, %B %d %Y %H:%M:%S", &timeinfo);
    return String(buffer);
}

bool TimeManager::isTimeSet() {
    struct tm timeinfo;
    return getLocalTime(&timeinfo);
}