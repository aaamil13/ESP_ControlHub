#include <Arduino.h>
#include <StreamLogger.h>
#include <WebManager.h>

// Constructor
StreamLogger::StreamLogger(WebManager& webManager) : _webManager(webManager) {
}

// Write implementation
size_t StreamLogger::write(uint8_t c) {
    return printf("%c", c);
}

size_t StreamLogger::write(const uint8_t *buffer, size_t size) {
    for (size_t i = 0; i < size; i++) {
        printf("%c", buffer[i]);
    }
    return size;
}
