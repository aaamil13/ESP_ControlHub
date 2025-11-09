#include "PlcEngine.h"
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

PlcEngine::PlcEngine(TimeManager* timeManager) : currentState(PlcState::STOPPED), plcTaskHandle(NULL), watchdog_timeout_ms(5000), _timeManager(timeManager) { // Default 5s timeout
}

void PlcEngine::begin() {
    memory.begin();
}

bool PlcEngine::loadConfiguration(const char* jsonConfig) {
    if (currentState == PlcState::RUNNING) {
        Log->println("Cannot load new configuration while PLC is running. Please stop it first.");
        return false;
    }

    // Clear previous configuration
    config.clear();
    logic_blocks.clear();
    memory.clear(); // Assuming PlcMemory has a clear method

    DeserializationError error = deserializeJson(config, jsonConfig);
    if (error) {
        Log->print(F("deserializeJson() for PLC config failed: "));
        Log->println(error.c_str());
        return false;
    }

    // 1. Set watchdog timeout
    watchdog_timeout_ms = config["watchdog_timeout_ms"] | 5000;

    // 2. Declare all variables from the "memory" block
    if (config.containsKey("memory")) {
        JsonObject mem_block = config["memory"];
        for (JsonPair kv : mem_block) {
            const char* var_name = kv.key().c_str();
            JsonObject var_attrs = kv.value().as<JsonObject>();
            
            String type_str = var_attrs["type"];
            bool is_retentive = var_attrs["retentive"] | false;
            String mesh_link = var_attrs["mesh_link"] | "";
            
            PlcDataType type;
            if (type_str == "bool") type = PlcDataType::BOOL;
            else if (type_str == "byte") type = PlcDataType::BYTE;
            else if (type_str == "int") type = PlcDataType::INT;
            else if (type_str == "dint") type = PlcDataType::DINT;
            else if (type_str == "real") type = PlcDataType::REAL;
            else if (type_str == "string") type = PlcDataType::STRING;
            else {
                Log->printf("ERROR: Unknown variable type '%s' for variable '%s'\n", type_str.c_str(), var_name);
                return false;
            }

            if (!memory.declareVariable(var_name, type, is_retentive, mesh_link)) {
                Log->printf("ERROR: Failed to declare variable '%s'\n", var_name);
                return false;
            }
            Log->printf("Declared variable '%s' of type %s (mesh_link: %s)\n", var_name, type_str.c_str(), mesh_link.c_str());
        }
    }

    // 3. Create and configure logic blocks
    if (config.containsKey("logic")) {
        JsonArray logic_cfg = config["logic"].as<JsonArray>();
        for (JsonObject block_cfg : logic_cfg) {
            const char* type = block_cfg["block_type"];
            std::unique_ptr<PlcBlock> block;

            if (strcmp(type, "AND") == 0) {
                block = std::make_unique<BlockAND>();
            } else if (strcmp(type, "OR") == 0) {
                block = std::make_unique<BlockOR>();
            } else if (strcmp(type, "NOT") == 0) {
                block = std::make_unique<BlockNOT>();
            } else if (strcmp(type, "XOR") == 0) {
                block = std::make_unique<BlockXOR>();
            } else if (strcmp(type, "NAND") == 0) {
                block = std::make_unique<BlockNAND>();
            } else if (strcmp(type, "NOR") == 0) {
                block = std::make_unique<BlockNOR>();
            } else if (strcmp(type, "SR") == 0) {
                block = std::make_unique<BlockSR>();
            } else if (strcmp(type, "RS") == 0) {
                block = std::make_unique<BlockRS>();
            } else if (strcmp(type, "TON") == 0) {
                block = std::make_unique<BlockTON>();
            } else if (strcmp(type, "TOF") == 0) {
                block = std::make_unique<BlockTOF>();
            } else if (strcmp(type, "TP") == 0) {
                block = std::make_unique<BlockTP>();
            } else if (strcmp(type, "CTU") == 0) {
                block = std::make_unique<BlockCTU>();
            } else if (strcmp(type, "CTD") == 0) {
                block = std::make_unique<BlockCTD>();
            } else if (strcmp(type, "CTUD") == 0) {
                block = std::make_unique<BlockCTUD>();
            } else if (strcmp(type, "ADD") == 0) {
                block = std::make_unique<BlockADD>();
            } else if (strcmp(type, "SUB") == 0) {
                block = std::make_unique<BlockSUB>();
            } else if (strcmp(type, "MUL") == 0) {
                block = std::make_unique<BlockMUL>();
            } else if (strcmp(type, "DIV") == 0) {
                block = std::make_unique<BlockDIV>();
            } else if (strcmp(type, "MOD") == 0) {
                block = std::make_unique<BlockMOD>();
            } else if (strcmp(type, "ABS") == 0) {
                block = std::make_unique<BlockABS>();
            } else if (strcmp(type, "SQRT") == 0) {
                block = std::make_unique<BlockSQRT>();
            } else if (strcmp(type, "INC") == 0) {
                block = std::make_unique<BlockINC>();
            } else if (strcmp(type, "DEC") == 0) {
                block = std::make_unique<BlockDEC>();
            } else if (strcmp(type, "GT") == 0) {
                block = std::make_unique<BlockGT>();
            } else if (strcmp(type, "EQ") == 0) {
                block = std::make_unique<BlockEQ>();
            } else if (strcmp(type, "NE") == 0) {
                block = std::make_unique<BlockNE>();
            } else if (strcmp(type, "LT") == 0) {
                block = std::make_unique<BlockLT>();
            } else if (strcmp(type, "GE") == 0) {
                block = std::make_unique<BlockGE>();
            } else if (strcmp(type, "LE") == 0) {
                block = std::make_unique<BlockLE>();
            } else if (strcmp(type, "TIME_COMPARE") == 0) {
                block = std::make_unique<BlockTimeCompare>(_timeManager);
            } else if (strcmp(type, "BOOL_ARRAY_TO_INT8") == 0) {
                block = std::make_unique<BlockBoolArrayToInt8>();
            } else if (strcmp(type, "INT8_TO_INT16") == 0) {
                block = std::make_unique<BlockInt8ToInt16>();
            } else if (strcmp(type, "INT8_TO_UINT8") == 0) {
                block = std::make_unique<BlockInt8ToUint8>();
            } else if (strcmp(type, "INT16_TO_UINT16") == 0) {
                block = std::make_unique<BlockInt16ToUint16>();
            } else if (strcmp(type, "INT32_TO_TIME") == 0) {
                block = std::make_unique<BlockInt32ToTime>();
            } else if (strcmp(type, "INT16_TO_FLOAT") == 0) {
                block = std::make_unique<BlockInt16ToFloat>();
            } else if (strcmp(type, "INT32_TO_DOUBLE") == 0) {
                block = std::make_unique<BlockInt32ToDouble>();
            } else if (strcmp(type, "SEQUENCER") == 0) {
                block = std::make_unique<BlockSequencer>();
            }
            // Add other block types here with else if

            if (block) {
                if (block->configure(block_cfg, memory)) {
                    logic_blocks.push_back(std::move(block));
                } else {
                    Log->printf("ERROR: Failed to configure block of type '%s'\n", type);
                    return false;
                }
            } else {
                Log->printf("ERROR: Unknown block type '%s'\n", type);
                return false;
            }
        }
    }

    Log->println("PLC configuration loaded successfully.");
    return true;
}

void PlcEngine::run() {
    if (currentState == PlcState::RUNNING) {
        Log->println("PLC is already running.");
        return;
    }
    
    executeInitBlock();

    currentState = PlcState::RUNNING;
    Log->println("Starting PLC task on Core 0...");
    xTaskCreatePinnedToCore(
        plcTask,          // Task function
        "plcTask",        // Name of the task
        10000,            // Stack size in words
        this,             // Task input parameter
        1,                // Priority of the task
        &plcTaskHandle,   // Task handle to keep track of the task
        0);               // Pin to core 0
}

void PlcEngine::stop() {
    if (currentState == PlcState::STOPPED) {
        Log->println("PLC is already stopped.");
        return;
    }
    if (plcTaskHandle != NULL) {
        vTaskDelete(plcTaskHandle);
        plcTaskHandle = NULL;
    }
    currentState = PlcState::STOPPED;
    Log->println("PLC task stopped.");
}

PlcState PlcEngine::getState() {
    return currentState;
}

void PlcEngine::executeInitBlock() {
    if (config.containsKey("init")) {
        Log->println("Executing INIT block...");
        JsonArray init_block = config["init"].as<JsonArray>();
        for (JsonObject action : init_block) {
            const char* action_type = action["action"];
            if (strcmp(action_type, "set_value") == 0) {
                const char* var_name = action["variable"];
                
                if (action["value"].is<bool>()) {
                    memory.setValue<bool>(var_name, action["value"].as<bool>());
                } else if (action["value"].is<float>()) {
                    memory.setValue<float>(var_name, action["value"].as<float>());
                } else if (action["value"].is<int>()) {
                    memory.setValue<int16_t>(var_name, action["value"].as<int>());
                }
                // Add other types as needed
                
                Log->printf("INIT: Set %s\n", var_name);
            }
        }
    }
}

void PlcEngine::plcTask(void* parameter) {
    PlcEngine* self = static_cast<PlcEngine*>(parameter);
    Log->println("PLC task started.");

    // Initialize and subscribe to the watchdog
    esp_task_wdt_init(self->watchdog_timeout_ms / 1000, true); // timeout in seconds, panic=true
    esp_task_wdt_add(NULL); // Subscribe current task

    for (;;) {
        esp_task_wdt_reset(); // Feed the dog
        self->evaluate();
        vTaskDelay(10 / portTICK_PERIOD_MS); // 10ms cycle
    }
}

void PlcEngine::evaluate() {
    for (auto& block : logic_blocks) {
        block->evaluate(memory);
    }
}