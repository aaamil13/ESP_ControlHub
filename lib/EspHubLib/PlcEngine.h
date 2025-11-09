#ifndef PLC_ENGINE_H
#define PLC_ENGINE_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include "PlcMemory.h"

class PlcEngine {
public:
    PlcEngine();
    void begin();
    void loadConfiguration(const char* jsonConfig);
    void evaluate();

private:
    PlcMemory memory;
    JsonDocument config;
};

#endif // PLC_ENGINE_H