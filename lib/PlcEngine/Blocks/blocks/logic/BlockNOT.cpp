#include "BlockNOT.h"

bool BlockNOT::configure(const JsonObject& config, PlcMemory& memory) {
    if (config.containsKey("inputs") && config["inputs"].containsKey("in")) {
        input_var = config["inputs"]["in"].as<std::string>();
    }
    if (config.containsKey("outputs") && config["outputs"].containsKey("out")) {
        output_var = config["outputs"]["out"].as<std::string>();
    }
    return true; // Basic validation for now
}

void BlockNOT::evaluate(PlcMemory& memory) {
    if (input_var.empty() || output_var.empty()) {
        return; // Not configured
    }

    bool in_val = memory.getValue<bool>(input_var, false);
    memory.setValue<bool>(output_var, !in_val);
}

JsonDocument BlockNOT::getBlockSchema() {
    JsonDocument schema;
    schema["description"] = "Logical NOT block";
    schema["inputs"]["in"]["type"] = "bool";
    schema["outputs"]["out"]["type"] = "bool";
    return schema;
}