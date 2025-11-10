#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>
#include "StreamLogger.h"

// External logger instance
extern StreamLogger* EspHubLog;

// Logging macros
#define LOG_INFO(tag, message) \
    if (EspHubLog) { \
        EspHubLog->print("[INFO] "); \
        EspHubLog->print(tag); \
        EspHubLog->print(": "); \
        EspHubLog->println(message); \
    }

#define LOG_WARN(tag, message) \
    if (EspHubLog) { \
        EspHubLog->print("[WARN] "); \
        EspHubLog->print(tag); \
        EspHubLog->print(": "); \
        EspHubLog->println(message); \
    }

#define LOG_ERROR(tag, message) \
    if (EspHubLog) { \
        EspHubLog->print("[ERROR] "); \
        EspHubLog->print(tag); \
        EspHubLog->print(": "); \
        EspHubLog->println(message); \
    }

#endif // LOGGER_H
