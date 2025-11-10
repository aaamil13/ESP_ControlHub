#include "PlcEngine.h"
#include <StreamLogger.h>

extern StreamLogger* EspHubLog;

#include "blocks/logic/BlockAND.h"
#include "blocks/logic/BlockOR.h"
#include "blocks/logic/BlockNOT.h"
#include "blocks/logic/BlockXOR.h"
#include "blocks/logic/BlockNAND.h"
#include "blocks/logic/BlockNOR.h"
#include "blocks/logic/BlockSR.h"
#include "blocks/logic/BlockRS.h"
#include "blocks/timers/BlockTON.h"
#include "blocks/timers/BlockTOF.h"
#include "blocks/timers/BlockTP.h"
#include "blocks/counters/BlockCTU.h"
#include "blocks/counters/BlockCTD.h"
#include "blocks/counters/BlockCTUD.h"
#include "blocks/math/BlockADD.h"
#include "blocks/math/BlockSUB.h"
#include "blocks/math/BlockMUL.h"
#include "blocks/math/BlockDIV.h"
#include "blocks/math/BlockMOD.h"
#include "blocks/math/BlockABS.h"
#include "blocks/math/BlockSQRT.h"
#include "blocks/math/BlockINC.h"
#include "blocks/math/BlockDEC.h"
#include "blocks/comparison/BlockGT.h"
#include "blocks/comparison/BlockEQ.h"
#include "blocks/comparison/BlockNE.h"
#include "blocks/comparison/BlockLT.h"
#include "blocks/comparison/BlockGE.h"
#include "blocks/comparison/BlockLE.h"
#include "blocks/scheduler/BlockTimeCompare.h"
#include "blocks/conversion/BlockBoolArrayToInt8.h"
#include "blocks/conversion/BlockInt8ToInt16.h"
#include "blocks/conversion/BlockInt8ToUint8.h"
#include "blocks/conversion/BlockInt16ToUint16.h"
#include "blocks/conversion/BlockInt32ToTime.h"
#include "blocks/conversion/BlockInt16ToFloat.h"
#include "blocks/conversion/BlockInt32ToDouble.h"
#include "blocks/logic/BlockSequencer.h"
#include "blocks/string/BlockStringConcat.h"
#include "blocks/string/BlockStringFind.h"
#include "blocks/string/BlockStringCopy.h"
#include "blocks/string/BlockStringFormat.h"

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
    for (auto& pair : programs) {
        pair.second->evaluate();
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