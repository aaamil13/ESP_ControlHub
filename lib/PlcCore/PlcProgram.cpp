#include "PlcProgram.h"
#include "../../EspHubLib/StreamLogger.h" // For EspHubLog
extern StreamLogger* EspHubLog; // Declare EspHubLog

// Include all block headers
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


PlcProgram::PlcProgram(const String& name, TimeManager* timeManager, MeshDeviceManager* meshDeviceManager)
    : _name(name), currentState(PlcProgramState::STOPPED), watchdog_timeout_ms(5000), _timeManager(timeManager), _meshDeviceManager(meshDeviceManager) {
}

bool PlcProgram::loadConfiguration(const char* jsonConfig) {
    if (currentState == PlcProgramState::RUNNING) {
        EspHubLog->printf("Cannot load new configuration for program '%s' while it is running. Please stop it first.\n", _name.c_str());
        return false;
    }

    // Clear previous configuration
    config.clear();
    logic_blocks.clear();
    memory.clear(); // Clear memory for this program

    DeserializationError error = deserializeJson(config, jsonConfig);
    if (error) {
        EspHubLog->printf(F("deserializeJson() for PLC program '%s' config failed: %s\n"), _name.c_str(), error.c_str());
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
            
            PlcValueType type;
            if (type_str == "bool") type = PlcValueType::BOOL;
            else if (type_str == "byte") type = PlcValueType::BYTE;
            else if (type_str == "int") type = PlcValueType::INT;
            else if (type_str == "dint") type = PlcValueType::DINT;
            else if (type_str == "real") type = PlcValueType::REAL;
            else if (type_str == "string") type = PlcValueType::STRING_TYPE;
            else {
                EspHubLog->printf("ERROR: Program '%s': Unknown variable type '%s' for variable '%s'\n", _name.c_str(), type_str.c_str(), var_name);
                return false;
            }

            if (!memory.declareVariable(var_name, type, is_retentive, mesh_link)) {
                EspHubLog->printf("ERROR: Program '%s': Failed to declare variable '%s'\n", _name.c_str(), var_name);
                return false;
            }
            EspHubLog->printf("Program '%s': Declared variable '%s' of type %s (mesh_link: %s)\n", _name.c_str(), var_name, type_str.c_str(), mesh_link.c_str());
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
                    EspHubLog->printf("ERROR: Program '%s': Failed to configure block of type '%s'\n", _name.c_str(), type);
                    return false;
                }
            } else {
                EspHubLog->printf("ERROR: Program '%s': Unknown block type '%s'\n", _name.c_str(), type);
                return false;
            }
        }
    }

    EspHubLog->printf("PLC program '%s' configuration loaded successfully.\n", _name.c_str());
    return true;
}

void PlcProgram::run() {
    if (currentState == PlcProgramState::RUNNING) {
        EspHubLog->printf("PLC program '%s' is already running.\n", _name.c_str());
        return;
    }
    
    executeInitBlock();

    currentState = PlcProgramState::RUNNING;
    EspHubLog->printf("PLC program '%s' started.\n", _name.c_str());
}

void PlcProgram::pause() {
    if (currentState == PlcProgramState::PAUSED) {
        EspHubLog->printf("PLC program '%s' is already paused.\n", _name.c_str());
        return;
    }
    currentState = PlcProgramState::PAUSED;
    EspHubLog->printf("PLC program '%s' paused.\n", _name.c_str());
}

void PlcProgram::stop() {
    if (currentState == PlcProgramState::STOPPED) {
        EspHubLog->printf("PLC program '%s' is already stopped.\n", _name.c_str());
        return;
    }
    currentState = PlcProgramState::STOPPED;
    EspHubLog->printf("PLC program '%s' stopped.\n", _name.c_str());
}

void PlcProgram::evaluate() {
    if (currentState != PlcProgramState::RUNNING) {
        return;
    }
    for (auto& block : logic_blocks) {
        block->evaluate(memory);
    }
}

void PlcProgram::executeInitBlock() {
    if (config.containsKey("init")) {
        EspHubLog->printf("Program '%s': Executing INIT block...\n", _name.c_str());
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
                
                EspHubLog->printf("Program '%s': INIT: Set %s\n", _name.c_str(), var_name);
            }
        }
    }
}