#ifndef STREAM_LOGGER_H
#define STREAM_LOGGER_H

#include <Arduino.h>
#include "WebManager.h"

class StreamLogger : public Print {
public:
    StreamLogger(WebManager& webManager);
    virtual size_t write(uint8_t);
    virtual size_t write(const uint8_t *buffer, size_t size);

private:
    WebManager& _webManager;
};

#endif // STREAM_LOGGER_H