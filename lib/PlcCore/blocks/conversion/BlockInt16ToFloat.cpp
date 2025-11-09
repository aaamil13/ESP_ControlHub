#include "BlockInt16ToFloat.h"

bool BlockInt16ToFloat::configure(const JsonObject& config, PlcMemory& memory) {
    if (config.containsKey("inputs") && config["inputs"].containsKey("in")) {
        input_var = config["inputs"]["in"].as<std::string>();
    }
    if (config.containsKey("outputs") && config["outputs"].containsKey("out")) {
        output_var = config["outputs"]["out"].as<std::string>();
    }
    return true; // Basic validation for now
}

void BlockInt16ToFloat::evaluate(PlcMemory& memory) {
    if (input_var.empty() || output_var.empty()) {
        return; // Not configured
    }

    int16_t in_val = memory.getValue<int16_t>(input_var, 0);
    memory.setValue<float>(output_var, static_cast<float>(in_val));
}

JsonDocument BlockInt16ToFloat::getBlockSchema() {
    JsonDocument schema;
    schema["description"] = "Converts int16_t to float";
    schema["inputs"]["in"]["type"] = "int16";
    schema["outputs"]["out"]["type"] = "float";
    return schema;
}