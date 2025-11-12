#ifndef PLC_PROGRAM_H
#define PLC_PROGRAM_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>
#include <memory>
#include "../PlcEngine/Engine/PlcMemory.h"
#include "../Blocks/PlcBlock.h"
#include "../../Core/TimeManager.h" // For scheduler blocks
#include "../../Protocols/Mesh/MeshDeviceManager.h" // For sending commands to mesh devices

enum class PlcProgramState {
    STOPPED,
    RUNNING,
    PAUSED
};

class PlcProgram {
public:
    PlcProgram(const String& name, TimeManager* timeManager, MeshDeviceManager* meshDeviceManager);
    bool loadConfiguration(const char* jsonConfig);
    void run();
    void pause();
    void stop();
    PlcProgramState getState() const { return currentState; }
    const String& getName() const { return _name; }

    void evaluate(); // Called by PlcEngine
    PlcMemory& getMemory() { return memory; } // Expose PlcMemory for external access

    // Memory management
    size_t getEstimatedMemoryUsage() const;
    bool validateMemoryAvailable(size_t requiredBytes) const;

private:
    String _name;
    PlcMemory memory;
    std::vector<std::unique_ptr<PlcBlock>> logic_blocks;
    StaticJsonDocument<4096> config; // Max 4KB for PLC config
    PlcProgramState currentState;
    uint32_t watchdog_timeout_ms;
    TimeManager* _timeManager;
    MeshDeviceManager* _meshDeviceManager;

    void executeInitBlock();
};

#endif // PLC_PROGRAM_H