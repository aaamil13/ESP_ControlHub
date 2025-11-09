#include "BlockSR.h"

BlockSR::BlockSR() {
}

bool BlockSR::configure(const JsonObject& config, PlcMemory& memory) {
    if (config.containsKey("inputs")) {
        set_var = config["inputs"]["set"].as<std::string>();
        reset_var = config["inputs"]["reset"].as<std::string>();
    }
    if (config.containsKey("outputs") && config["outputs"].containsKey("out")) {
        output_var = config["outputs"]["out"].as<std::string>();
    }
    return true; // Basic validation for now
}

void BlockSR::evaluate(PlcMemory& memory) {
    if (set_var.empty() || reset_var.empty() || output_var.empty()) {
        return; // Not configured
    }

    bool set_val = memory.getValue<bool>(set_var, false);
    bool reset_val = memory.getValue<bool>(reset_var, false);
    bool current_output = memory.getValue<bool>(output_var, false);

    if (reset_val) {
        current_output = false;
    } else if (set_val) {
        current_output = true;
    }
    // If both set and reset are false, output retains its last state.
    // If both are true, reset takes precedence (standard SR behavior).

    memory.setValue<bool>(output_var, current_output);
}

JsonDocument BlockSR::getBlockSchema() {
    JsonDocument schema;
    schema["description"] = "Set-Reset Latch (Reset dominant)";
    schema["inputs"]["set"]["type"] = "bool";
    schema["inputs"]["reset"]["type"] = "bool";
    schema["outputs"]["out"]["type"] = "bool";
    return schema;
}