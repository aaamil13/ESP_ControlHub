#include "../PlcEngine/Engine/PlcEngine.h"
#include <StreamLogger.h>
#include "../Devices/DeviceRegistry.h" // For IODirection enum

extern StreamLogger* EspHubLog;

#include "../Blocks/logic/BlockAND.h"
#include "../Blocks/logic/BlockOR.h"
#include "../Blocks/logic/BlockNOT.h"
#include "../Blocks/logic/BlockXOR.h"
#include "../Blocks/logic/BlockNAND.h"
#include "../Blocks/logic/BlockNOR.h"
#include "../Blocks/logic/BlockSR.h"
#include "../Blocks/logic/BlockRS.h"
#include "../Blocks/timers/BlockTON.h"
#include "../Blocks/timers/BlockTOF.h"
#include "../Blocks/timers/BlockTP.h"
#include "../Blocks/counters/BlockCTU.h"
#include "../Blocks/counters/BlockCTD.h"
#include "../Blocks/counters/BlockCTUD.h"
#include "../Blocks/math/BlockADD.h"
#include "../Blocks/math/BlockSUB.h"
#include "../Blocks/math/BlockMUL.h"
#include "../Blocks/math/BlockDIV.h"
#include "../Blocks/math/BlockMOD.h"
#include "../Blocks/math/BlockABS.h"
#include "../Blocks/math/BlockSQRT.h"
#include "../Blocks/math/BlockINC.h"
#include "../Blocks/math/BlockDEC.h"
#include "../Blocks/comparison/BlockGT.h"
#include "../Blocks/comparison/BlockEQ.h"
#include "../Blocks/comparison/BlockNE.h"
#include "../Blocks/comparison/BlockLT.h"
#include "../Blocks/comparison/BlockGE.h"
#include "../Blocks/comparison/BlockLE.h"
#include "../Blocks/scheduler/BlockTimeCompare.h"
#include "../Blocks/conversion/BlockBoolArrayToInt8.h"
#include "../Blocks/conversion/BlockInt8ToInt16.h"
#include "../Blocks/conversion/BlockInt8ToUint8.h"
#include "../Blocks/conversion/BlockInt16ToUint16.h"
#include "../Blocks/conversion/BlockInt32ToTime.h"
#include "../Blocks/conversion/BlockInt16ToFloat.h"
#include "../Blocks/conversion/BlockInt32ToDouble.h"
#include "../Blocks/logic/BlockSequencer.h"
#include "../Blocks/string/BlockStringConcat.h"
#include "../Blocks/string/BlockStringFind.h"
#include "../Blocks/string/BlockStringCopy.h"
#include "../Blocks/string/BlockStringFormat.h"

PlcEngine::PlcEngine(TimeManager* timeManager, MeshDeviceManager* meshDeviceManager)
    : currentEngineState(PlcEngineState::STOPPED), plcEngineTaskHandle(NULL), _timeManager(timeManager), _meshDeviceManager(meshDeviceManager) {
}

void PlcEngine::begin() {
    // No global memory.begin() needed anymore, each program has its own.
}

bool PlcEngine::loadProgram(const String& programName, const char* jsonConfig) {
    if (programs.count(programName)) {
        EspHubLog->printf("ERROR: Program '%s' already exists. Delete it first.\n", programName.c_str());
        return false;
    }

    auto newProgram = std::make_unique<PlcProgram>(programName, _timeManager, _meshDeviceManager);
    if (!newProgram->loadConfiguration(jsonConfig)) {
        EspHubLog->printf("ERROR: Failed to load configuration for program '%s'.\n", programName.c_str());
        return false;
    }
    programs[programName] = std::move(newProgram);
    EspHubLog->printf("Program '%s' loaded successfully.\n", programName.c_str());
    return true;
}

void PlcEngine::runProgram(const String& programName) {
    if (programs.count(programName)) {
        programs[programName]->run();
        // Start the global PLC engine task if not already running
        if (currentEngineState == PlcEngineState::STOPPED) {
            currentEngineState = PlcEngineState::RUNNING;
            EspHubLog->println("Starting global PLC engine task on Core 0...");
            xTaskCreatePinnedToCore(
                plcEngineTask,          // Task function
                "plcEngineTask",        // Name of the task
                10000,                  // Stack size in words
                this,                   // Task input parameter
                1,                      // Priority of the task
                &plcEngineTaskHandle,   // Task handle to keep track of the task
                0);                     // Pin to core 0
        }
    } else {
        EspHubLog->printf("ERROR: Program '%s' not found.\n", programName.c_str());
    }
}

void PlcEngine::pauseProgram(const String& programName) {
    if (programs.count(programName)) {
        programs[programName]->pause();
    } else {
        EspHubLog->printf("ERROR: Program '%s' not found.\n", programName.c_str());
    }
}

void PlcEngine::stopProgram(const String& programName) {
    if (programs.count(programName)) {
        programs[programName]->stop();
        // Check if all programs are stopped, then stop the global PLC engine task
        bool allStopped = true;
        for (auto const& [name, program] : programs) {
            if (program->getState() != PlcProgramState::STOPPED) {
                allStopped = false;
                break;
            }
        }
        if (allStopped && currentEngineState == PlcEngineState::RUNNING) {
            if (plcEngineTaskHandle != NULL) {
                vTaskDelete(plcEngineTaskHandle);
                plcEngineTaskHandle = NULL;
            }
            currentEngineState = PlcEngineState::STOPPED;
            EspHubLog->println("Global PLC engine task stopped.");
        }
    } else {
        EspHubLog->printf("ERROR: Program '%s' not found.\n", programName.c_str());
    }
}

void PlcEngine::deleteProgram(const String& programName) {
    if (programs.count(programName)) {
        if (programs[programName]->getState() != PlcProgramState::STOPPED) {
            EspHubLog->printf("ERROR: Cannot delete program '%s' while it is running or paused. Stop it first.\n", programName.c_str());
            return;
        }
        programs.erase(programName);
        EspHubLog->printf("Program '%s' deleted.\n", programName.c_str());
        // Also delete the file from LittleFS
    } else {
        EspHubLog->printf("ERROR: Program '%s' not found.\n", programName.c_str());
    }
}

PlcProgram* PlcEngine::getProgram(const String& programName) {
    if (programs.count(programName)) {
        return programs[programName].get();
    }
    return nullptr;
}

std::vector<String> PlcEngine::getProgramNames() const {
    std::vector<String> names;
    for (auto const& [name, program] : programs) {
        names.push_back(name);
    }
    return names;
}

void PlcEngine::evaluateAllPrograms() {
    // PHASE 1: READ - Sync all INPUTS from devices to PLC memory
    // This reads the current state of all input devices into PLC variables
    IODirection inputDirection = IODirection::IO_INPUT;
    for (auto& pair : programs) {
        if (pair.second->getState() == PlcProgramState::RUNNING) {
            PlcMemory& memory = pair.second->getMemory();
            memory.syncIOPoints(&inputDirection);
        }
    }

    // PHASE 2: EXECUTE - Run program logic
    // Process all PLC program logic with the freshly read inputs
    for (auto& pair : programs) {
        if (pair.second->getState() == PlcProgramState::RUNNING) {
            pair.second->evaluate();
        }
    }

    // PHASE 3: WRITE - Sync all OUTPUTS from PLC memory to devices
    // This writes the calculated output values to physical devices
    IODirection outputDirection = IODirection::IO_OUTPUT;
    for (auto& pair : programs) {
        if (pair.second->getState() == PlcProgramState::RUNNING) {
            PlcMemory& memory = pair.second->getMemory();
            memory.syncIOPoints(&outputDirection);
        }
    }
}

void PlcEngine::plcEngineTask(void* parameter) {
    PlcEngine* self = static_cast<PlcEngine*>(parameter);
    EspHubLog->println("Global PLC engine task started.");

    // Initialize and subscribe to the watchdog
    // Watchdog timeout is now per program, but a global one can be useful
    // esp_task_wdt_init(self->watchdog_timeout_ms / 1000, true); 
    // esp_task_wdt_add(NULL); 

    for (;;) {
        // esp_task_wdt_reset(); // Feed the dog
        self->evaluateAllPrograms();
        vTaskDelay(10 / portTICK_PERIOD_MS); // 10ms cycle
    }
}