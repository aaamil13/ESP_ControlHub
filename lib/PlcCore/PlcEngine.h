#ifndef PLC_ENGINE_H
#define PLC_ENGINE_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include "PlcMemory.h"
#include "blocks/PlcBlock.h"
#include <esp_task_wdt.h>
#include <vector>
#include <memory>
#include "../../EspHubLib/TimeManager.h" // For scheduler blocks

enum class PlcState {
    STOPPED,
    RUNNING
};

class PlcEngine {
public:
    PlcEngine(TimeManager* timeManager);
    void begin();
    bool loadConfiguration(const char* jsonConfig);
    void run();
    void stop();
    PlcState getState();

private:
    PlcMemory memory;
    std::vector<std::unique_ptr<PlcBlock>> logic_blocks;
    JsonDocument config;
    PlcState currentState;
    TaskHandle_t plcTaskHandle;
    uint32_t watchdog_timeout_ms;
    TimeManager* _timeManager;

    void executeInitBlock();
    void evaluate();
    static void plcTask(void* parameter);
};

#endif // PLC_ENGINE_H