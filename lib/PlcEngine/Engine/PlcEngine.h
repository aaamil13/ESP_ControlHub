#ifndef PLC_ENGINE_H
#define PLC_ENGINE_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include "../PlcEngine/Engine/PlcMemory.h"
#include "../Blocks/PlcBlock.h"
#include <esp_task_wdt.h>
#include <vector>
#include <memory>

// Polyfill for std::make_unique if not available in C++11
#if __cplusplus < 201402L
namespace std {
    template<typename T, typename... Args>
    unique_ptr<T> make_unique(Args&&... args) {
        return unique_ptr<T>(new T(std::forward<Args>(args)...));
    }
}
#endif

#include "../../Core/TimeManager.h" // For scheduler blocks
class MeshDeviceManager; // Forward declaration
#include "../PlcEngine/Engine/PlcProgram.h" // New PlcProgram class

enum class PlcEngineState {
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
    PlcEngineState getEngineState() const { return currentEngineState; }
    PlcProgram* getProgram(const String& programName);
    std::vector<String> getProgramNames() const;
    PlcMemory& getMemory() { return programs.at("main_program")->getMemory(); } // Expose PlcMemory for external access

    // Called by the FreeRTOS task
    void evaluateAllPrograms();

private:
    std::map<String, std::unique_ptr<PlcProgram>> programs;
    PlcEngineState currentEngineState;
    TaskHandle_t plcEngineTaskHandle;
    TimeManager* _timeManager;
    MeshDeviceManager* _meshDeviceManager;

    static void plcEngineTask(void* parameter);
};

#endif // PLC_ENGINE_H