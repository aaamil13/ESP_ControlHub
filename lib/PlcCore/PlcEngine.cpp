#include "PlcEngine.h"
#include "blocks/logic/BlockAND.h"
#include "blocks/logic/BlockOR.h"
#include "blocks/timers/BlockTON.h"

PlcEngine::PlcEngine() : currentState(PlcState::STOPPED), plcTaskHandle(NULL), watchdog_timeout_ms(5000) { // Default 5s timeout
}

void PlcEngine::begin() {
    memory.begin();
}

bool PlcEngine::loadConfiguration(const char* jsonConfig) {
    if (currentState == PlcState::RUNNING) {
        Log->println("Cannot load new configuration while PLC is running. Please stop it first.");
        return false;
    }

    JsonDocument config;
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
            
            PlcDataType type;
            if (type_str == "bool") type = PlcDataType::BOOL;
            else if (type_str == "byte") type = PlcDataType::BYTE;
            else if (type_str == "int") type = PlcDataType::INT;
            else if (type_str == "dint") type = PlcDataType::DINT;
            else if (type_str == "real") type = PlcDataType::REAL;
            else if (type_str == "string") type = PlcDataType::STRING;
            else continue; // Skip unknown types

            memory.declareVariable(var_name, type, is_retentive);
            Log->printf("Declared variable '%s' of type %s\n", var_name, type_str.c_str());
        }
    }

    Log->println("PLC configuration loaded successfully.");

    // 3. Create and configure logic blocks
    logic_blocks.clear();
    if (config.containsKey("logic")) {
        JsonArray logic_cfg = config["logic"].as<JsonArray>();
        for (JsonObject block_cfg : logic_cfg) {
            const char* type = block_cfg["block_type"];
            if (strcmp(type, "AND") == 0) {
                auto block = std::make_unique<BlockAND>();
                block->configure(block_cfg, memory);
                logic_blocks.push_back(std::move(block));
            } else if (strcmp(type, "OR") == 0) {
                auto block = std::make_unique<BlockOR>();
                block->configure(block_cfg, memory);
                logic_blocks.push_back(std::move(block));
            } else if (strcmp(type, "TON") == 0) {
                auto block = std::make_unique<BlockTON>();
                block->configure(block_cfg, memory);
                logic_blocks.push_back(std::move(block));
            }
            // Add other block types here with else if
        }
    }

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
    // This needs the config to be stored as a member, which is not ideal.
    // Refactoring to pass config to run() might be better.
    // For now, we assume config is loaded.
    JsonDocument doc; // A temporary doc to parse the config again.
    // This is inefficient, the config should be stored.
    // Let's fix this by making `config` a member of PlcEngine.
    // It is already a member, so we can use it.

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
                } // ... and so on for other types
                
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