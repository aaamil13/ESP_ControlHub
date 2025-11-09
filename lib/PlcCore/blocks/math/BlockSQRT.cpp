#include "BlockSQRT.h"
#include <cmath> // For sqrtf

bool BlockSQRT::configure(const JsonObject& config, PlcMemory& memory) {
    if (config.containsKey("inputs") && config["inputs"].containsKey("in")) {
        input_var = config["inputs"]["in"].as<std::string>();
    }
    if (config.containsKey("outputs") && config["outputs"].containsKey("out")) {
        output_var = config["outputs"]["out"].as<std::string>();
    }
    return true; // Basic validation for now
}

void BlockSQRT::evaluate(PlcMemory& memory) {
    if (input_var.empty() || output_var.empty()) {
        return; // Not configured
    }

    float in_val = memory.getValue<float>(input_var, 0.0f);
    if (in_val >= 0) {
        memory.setValue<float>(output_var, sqrtf(in_val));
    } else {
        // Handle error for negative input, e.g., set output to 0 or NaN
        memory.setValue<float>(output_var, 0.0f);
    }
}

JsonDocument BlockSQRT::getBlockSchema() {
    JsonDocument schema;
    schema["description"] = "Square Root block";
    schema["inputs"]["in"]["type"] = "float";
    schema["outputs"]["out"]["type"] = "float";
    return schema;
}