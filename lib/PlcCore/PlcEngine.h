#ifndef PLC_ENGINE_H
#define PLC_ENGINE_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include "PlcMemory.h"

enum class PlcState {
    STOPPED,
    RUNNING
};

class PlcEngine {
public:
    PlcEngine();
    void begin();
    bool loadConfiguration(const char* jsonConfig);
    void run();
    void stop();
    PlcState getState();

private:
    PlcMemory memory;
    JsonDocument config;
    PlcState currentState;
    TaskHandle_t plcTaskHandle;

    void evaluate();
    static void plcTask(void* parameter);
};

#endif // PLC_ENGINE_H