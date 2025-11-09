#include "BlockABS.h"

bool BlockABS::configure(const JsonObject& config, PlcMemory& memory) {
    if (config.containsKey("inputs") && config["inputs"].containsKey("in")) {
        input_var = config["inputs"]["in"].as<std::string>();
    }
    if (config.containsKey("outputs") && config["outputs"].containsKey("out")) {
        output_var = config["outputs"]["out"].as<std::string>();
    }
    return true; // Basic validation for now
}

void BlockABS::evaluate(PlcMemory& memory) {
    if (input_var.empty() || output_var.empty()) {
        return; // Not configured
    }

    // Assuming float for now, can be extended with type checking
    float in_val = memory.getValue<float>(input_var, 0.0f);
    memory.setValue<float>(output_var, fabsf(in_val));
}

JsonDocument BlockABS::getBlockSchema() {
    JsonDocument schema;
    schema["description"] = "Absolute value block";
    schema["inputs"]["in"]["type"] = "float";
    schema["outputs"]["out"]["type"] = "float";
    return schema;
}