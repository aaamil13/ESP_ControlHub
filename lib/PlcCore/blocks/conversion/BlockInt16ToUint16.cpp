#include "BlockInt16ToUint16.h"

bool BlockInt16ToUint16::configure(const JsonObject& config, PlcMemory& memory) {
    if (config.containsKey("inputs") && config["inputs"].containsKey("in")) {
        input_var = config["inputs"]["in"].as<std::string>();
    }
    if (config.containsKey("outputs") && config["outputs"].containsKey("out")) {
        output_var = config["outputs"]["out"].as<std::string>();
    }
    return true; // Basic validation for now
}

void BlockInt16ToUint16::evaluate(PlcMemory& memory) {
    if (input_var.empty() || output_var.empty()) {
        return; // Not configured
    }

    int16_t in_val = memory.getValue<int16_t>(input_var, 0);
    memory.setValue<uint16_t>(output_var, static_cast<uint16_t>(in_val));
}

JsonDocument BlockInt16ToUint16::getBlockSchema() {
    JsonDocument schema;
    schema["description"] = "Converts int16_t to uint16_t";
    schema["inputs"]["in"]["type"] = "int16";
    schema["outputs"]["out"]["type"] = "uint16";
    return schema;
}