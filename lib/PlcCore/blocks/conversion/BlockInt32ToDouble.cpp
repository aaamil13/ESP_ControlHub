#include "BlockInt32ToDouble.h"

bool BlockInt32ToDouble::configure(const JsonObject& config, PlcMemory& memory) {
    if (config.containsKey("inputs") && config["inputs"].containsKey("in")) {
        input_var = config["inputs"]["in"].as<std::string>();
    }
    if (config.containsKey("outputs") && config["outputs"].containsKey("out")) {
        output_var = config["outputs"]["out"].as<std::string>();
    }
    return true; // Basic validation for now
}

void BlockInt32ToDouble::evaluate(PlcMemory& memory) {
    if (input_var.empty() || output_var.empty()) {
        return; // Not configured
    }

    int32_t in_val = memory.getValue<int32_t>(input_var, 0);
    memory.setValue<double>(output_var, static_cast<double>(in_val));
}

JsonDocument BlockInt32ToDouble::getBlockSchema() {
    JsonDocument schema;
    schema["description"] = "Converts int32_t to double";
    schema["inputs"]["in"]["type"] = "int32";
    schema["outputs"]["out"]["type"] = "double";
    return schema;
}