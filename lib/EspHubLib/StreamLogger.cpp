#include "StreamLogger.h"

StreamLogger::StreamLogger(WebManager& webManager) : _webManager(webManager) {
}

size_t StreamLogger::write(uint8_t c) {
    Serial.write(c);
    _webManager.log(String((char)c));
    return 1;
}

size_t StreamLogger::write(const uint8_t *buffer, size_t size) {
    Serial.write(buffer, size);
    String s = "";
    for (size_t i = 0; i < size; i++) {
        s += (char)buffer[i];
    }
    _webManager.log(s);
    return size;
}