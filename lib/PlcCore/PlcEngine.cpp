#include "PlcEngine.h"

PlcEngine::PlcEngine() : currentState(PlcState::STOPPED), plcTaskHandle(NULL) {
}

void PlcEngine::begin() {
    memory.begin();
}

bool PlcEngine::loadConfiguration(const char* jsonConfig) {
    if (currentState == PlcState::RUNNING) {
        Log->println("Cannot load new configuration while PLC is running. Please stop it first.");
        return false;
    }

    DeserializationError error = deserializeJson(config, jsonConfig);
    if (error) {
        Log->print(F("deserializeJson() for PLC config failed: "));
        Log->println(error.c_str());
        return false;
    }

    // 1. Declare all variables from the "memory" block
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
    return true;
}

void PlcEngine::run() {
    if (currentState == PlcState::RUNNING) {
        Log->println("PLC is already running.");
        return;
    }
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

void PlcEngine::plcTask(void* parameter) {
    PlcEngine* self = static_cast<PlcEngine*>(parameter);
    Log->println("PLC task started.");
    for (;;) {
        self->evaluate();
        vTaskDelay(10 / portTICK_PERIOD_MS); // 10ms cycle
    }
}

void PlcEngine::evaluate() {
    // TODO: Implement the logic network evaluation
}