#include "BlockBoolArrayToInt8.h"

bool BlockBoolArrayToInt8::configure(const JsonObject& config, PlcMemory& memory) {
    if (config.containsKey("inputs")) {
        JsonArray inputs = config["inputs"].as<JsonArray>();
        for (JsonVariant v : inputs) {
            input_vars.push_back(v.as<std::string>());
        }
    }
    if (config.containsKey("outputs") && config["outputs"].containsKey("out")) {
        output_var = config["outputs"]["out"].as<std::string>();
    }
    return true; // Basic validation for now
}

void BlockBoolArrayToInt8::evaluate(PlcMemory& memory) {
    if (output_var.empty() || input_vars.empty()) {
        return; // Not configured
    }

    int8_t result = 0;
    for (size_t i = 0; i < input_vars.size() && i < 8; ++i) {
        if (memory.getValue<bool>(input_vars[i], false)) {
            result |= (1 << i);
        }
    }
    memory.setValue<int8_t>(output_var, result);
}

JsonDocument BlockBoolArrayToInt8::getBlockSchema() {
    JsonDocument schema;
    schema["description"] = "Converts an array of up to 8 booleans to an int8_t";
    schema["inputs"]["in"]["type"] = "array";
    schema["inputs"]["in"]["items"]["type"] = "bool";
    schema["outputs"]["out"]["type"] = "int8";
    return schema;
}