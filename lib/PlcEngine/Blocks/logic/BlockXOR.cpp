#include "BlockXOR.h"

bool BlockXOR::configure(const JsonObject& config, PlcMemory& memory) {
    if (config.containsKey("inputs")) {
        JsonObject inputs = config["inputs"];
        for (JsonPair kv : inputs) {
            input_vars.push_back(kv.value().as<std::string>());
        }
    }
    if (config.containsKey("outputs") && config["outputs"].containsKey("out")) {
        output_var = config["outputs"]["out"].as<std::string>();
    }
    return true; // Basic validation for now
}

void BlockXOR::evaluate(PlcMemory& memory) {
    if (output_var.empty() || input_vars.empty()) {
        return; // Not configured
    }

    bool result = false;
    if (!input_vars.empty()) {
        result = memory.getValue<bool>(input_vars[0], false);
        for (size_t i = 1; i < input_vars.size(); ++i) {
            result = result ^ memory.getValue<bool>(input_vars[i], false);
        }
    }
    memory.setValue<bool>(output_var, result);
}

JsonDocument BlockXOR::getBlockSchema() {
    JsonDocument schema;
    schema["description"] = "Logical XOR block";
    schema["inputs"]["in1"]["type"] = "bool";
    schema["inputs"]["in2"]["type"] = "bool";
    schema["outputs"]["out"]["type"] = "bool";
    return schema;
}