#include "BlockMUL.h"

bool BlockMUL::configure(const JsonObject& config, PlcMemory& memory) {
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

void BlockMUL::evaluate(PlcMemory& memory) {
    if (output_var.empty() || input_vars.empty()) {
        return; // Not configured
    }

    // Assuming all inputs are of type float for simplicity
    float result = 1.0f;
    for (const auto& var_name : input_vars) {
        result *= memory.getValue<float>(var_name, 0.0f);
    }
    memory.setValue<float>(output_var, result);
}

JsonDocument BlockMUL::getBlockSchema() {
    JsonDocument schema;
    schema["description"] = "Multiplication block";
    schema["inputs"]["in1"]["type"] = "float";
    schema["inputs"]["in2"]["type"] = "float";
    schema["outputs"]["out"]["type"] = "float";
    return schema;
}