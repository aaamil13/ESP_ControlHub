#include "BlockInt8ToInt16.h"

bool BlockInt8ToInt16::configure(const JsonObject& config, PlcMemory& memory) {
    if (config.containsKey("inputs") && config["inputs"].containsKey("in")) {
        input_var = config["inputs"]["in"].as<std::string>();
    }
    if (config.containsKey("outputs") && config["outputs"].containsKey("out")) {
        output_var = config["outputs"]["out"].as<std::string>();
    }
    return true; // Basic validation for now
}

void BlockInt8ToInt16::evaluate(PlcMemory& memory) {
    if (input_var.empty() || output_var.empty()) {
        return; // Not configured
    }

    int8_t in_val = memory.getValue<int8_t>(input_var, 0);
    memory.setValue<int16_t>(output_var, static_cast<int16_t>(in_val));
}

JsonDocument BlockInt8ToInt16::getBlockSchema() {
    JsonDocument schema;
    schema["description"] = "Converts int8_t to int16_t";
    schema["inputs"]["in"]["type"] = "int8";
    schema["outputs"]["out"]["type"] = "int16";
    return schema;
}