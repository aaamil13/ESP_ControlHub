#include "BlockOR.h"

bool BlockOR::configure(const JsonObject& config, PlcMemory& memory) {
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

void BlockOR::evaluate(PlcMemory& memory) {
    if (output_var.empty() || input_vars.empty()) {
        return; // Not configured
    }

    bool result = false;
    for (const auto& var_name : input_vars) {
        if (memory.getValue<bool>(var_name, false)) {
            result = true;
            break;
        }
    }
    memory.setValue<bool>(output_var, result);
}

JsonDocument BlockOR::getBlockSchema() {
    JsonDocument schema;
    schema["description"] = "Logical OR block";
    schema["inputs"]["in1"]["type"] = "bool";
    schema["inputs"]["in2"]["type"] = "bool";
    schema["outputs"]["out"]["type"] = "bool";
    return schema;
}