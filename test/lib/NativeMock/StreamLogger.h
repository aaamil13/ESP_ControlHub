#ifndef STREAM_LOGGER_H
#define STREAM_LOGGER_H

#include <Arduino.h>
#include <cstdarg>
#include <WebManager.h>

class StreamLogger : public Print {
public:
    StreamLogger(WebManager& webManager) : _webManager(webManager) {}
    virtual size_t write(uint8_t c) {
        return fwrite(&c, 1, 1, stdout);
    }
    virtual size_t write(const uint8_t *buffer, size_t size) {
        return fwrite(buffer, 1, size, stdout);
    }
    
    // Add printf support
    size_t printf(const char *format, ...) __attribute__ ((format (printf, 2, 3))) {
        va_list arg;
        va_start(arg, format);
        size_t len = vprintf(format, arg);
        va_end(arg);
        return len;
    }

private:
    WebManager& _webManager;
};

extern StreamLogger* EspHubLog;

#endif
