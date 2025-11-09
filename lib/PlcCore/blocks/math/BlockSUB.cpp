#include "BlockSUB.h"

bool BlockSUB::configure(const JsonObject& config, PlcMemory& memory) {
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

void BlockSUB::evaluate(PlcMemory& memory) {
    if (output_var.empty() || input_vars.empty()) {
        return; // Not configured
    }

    // Assuming all inputs are of type float for simplicity
    float result = memory.getValue<float>(input_vars[0], 0.0f);
    for (size_t i = 1; i < input_vars.size(); ++i) {
        result -= memory.getValue<float>(input_vars[i], 0.0f);
    }
    memory.setValue<float>(output_var, result);
}

JsonDocument BlockSUB::getBlockSchema() {
    JsonDocument schema;
    schema["description"] = "Subtraction block";
    schema["inputs"]["in1"]["type"] = "float";
    schema["inputs"]["in2"]["type"] = "float";
    schema["outputs"]["out"]["type"] = "float";
    return schema;
}