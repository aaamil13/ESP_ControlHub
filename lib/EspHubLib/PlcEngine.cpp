#include "PlcEngine.h"

PlcEngine::PlcEngine() {
}

void PlcEngine::begin() {
    memory.begin();
}

void PlcEngine::loadConfiguration(const char* jsonConfig) {
    DeserializationError error = deserializeJson(config, jsonConfig);
    if (error) {
        Log->print(F("deserializeJson() for PLC config failed: "));
        Log->println(error.c_str());
        return;
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

    Log->println("PLC configuration loaded.");
}

void PlcEngine::evaluate() {
    // TODO: Implement the logic network evaluation
    // This will involve:
    // 1. Iterating through the "logic" block of the config
    // 2. Finding the corresponding function block (AND, OR, TON, etc.)
    // 3. Reading input values from PlcMemory
    // 4. Executing the block's logic
    // 5. Writing output values back to PlcMemory
}