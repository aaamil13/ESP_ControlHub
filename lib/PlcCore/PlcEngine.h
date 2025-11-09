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
#include "PlcProgram.h" // New PlcProgram class

enum class PlcEngineState { // Renamed to avoid conflict
    STOPPED,
    RUNNING
};

class PlcEngine {
public:
    PlcEngine(TimeManager* timeManager, MeshDeviceManager* meshDeviceManager);
    void begin();
    bool loadProgram(const String& programName, const char* jsonConfig);
    void runProgram(const String& programName);
    void pauseProgram(const String& programName);
    void stopProgram(const String& programName);
    void deleteProgram(const String& programName);
    PlcEngineState getEngineState() const { return currentEngineState; } // Renamed
    PlcProgram* getProgram(const String& programName);
    std::vector<String> getProgramNames() const;
    PlcMemory& getMemory() { return programs.at("main_program")->getMemory(); } // Expose PlcMemory for external access

    // Called by the FreeRTOS task
    void evaluateAllPrograms();

private:
    std::map<String, std::unique_ptr<PlcProgram>> programs;
    PlcEngineState currentEngineState; // Renamed
    TaskHandle_t plcEngineTaskHandle; // Renamed
    TimeManager* _timeManager;
    MeshDeviceManager* _meshDeviceManager;

    static void plcEngineTask(void* parameter); // Renamed
};

#endif // PLC_ENGINE_H