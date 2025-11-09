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
#include "../../EspHubLib/MeshDeviceManager.h" // For sending commands to mesh devices

enum class PlcState {
    STOPPED,
    RUNNING
};

class PlcEngine {
public:
    PlcEngine(TimeManager* timeManager, MeshDeviceManager* meshDeviceManager);
    void begin();
    bool loadConfiguration(const char* jsonConfig);
    void run();
    void stop();
    PlcState getState();
    PlcMemory& getMemory() { return memory; } // Expose PlcMemory for external access

private:
    PlcMemory memory;
    std::vector<std::unique_ptr<PlcBlock>> logic_blocks;
    StaticJsonDocument<4096> config; // Max 4KB for PLC config
    PlcState currentState;
    TaskHandle_t plcTaskHandle;
    uint32_t watchdog_timeout_ms;
    TimeManager* _timeManager;
    MeshDeviceManager* _meshDeviceManager;

    void executeInitBlock();
    void evaluate();
    static void plcTask(void* parameter);
};

#endif // PLC_ENGINE_H