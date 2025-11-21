#ifndef PREFERENCES_H
#define PREFERENCES_H

#include <Arduino.h>
#include <map>
#include <string>

class Preferences {
public:
    bool begin(const char* name, bool readOnly = false) { return true; }
    void end() {}
    void clear() {}

    bool getBool(const char* key, bool defaultValue = false) { return defaultValue; }
    void putBool(const char* key, bool value) {}

    uint8_t getUChar(const char* key, uint8_t defaultValue = 0) { return defaultValue; }
    void putUChar(const char* key, uint8_t value) {}

    int16_t getShort(const char* key, int16_t defaultValue = 0) { return defaultValue; }
    void putShort(const char* key, int16_t value) {}

    uint32_t getUInt(const char* key, uint32_t defaultValue = 0) { return defaultValue; }
    void putUInt(const char* key, uint32_t value) {}

    float getFloat(const char* key, float defaultValue = 0.0f) { return defaultValue; }
    void putFloat(const char* key, float value) {}

    String getString(const char* key, const String defaultValue = String()) { return defaultValue; }
    size_t putString(const char* key, const String value) { return value.length(); }
};

#endif
