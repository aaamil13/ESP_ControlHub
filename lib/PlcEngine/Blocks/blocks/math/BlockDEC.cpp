#include "BlockDEC.h"

bool BlockDEC::configure(const JsonObject& config, PlcMemory& memory) {
    if (config.containsKey("inputs") && config["inputs"].containsKey("in_out")) {
        input_output_var = config["inputs"]["in_out"].as<std::string>();
    }
    return true; // Basic validation for now
}

void BlockDEC::evaluate(PlcMemory& memory) {
    if (input_output_var.empty()) {
        return; // Not configured
    }

    // Assuming integer for now, can be extended with type checking
    int16_t val = memory.getValue<int16_t>(input_output_var, 0);
    memory.setValue<int16_t>(input_output_var, val - 1);
}

JsonDocument BlockDEC::getBlockSchema() {
    JsonDocument schema;
    schema["description"] = "Decrement block";
    schema["inputs"]["in_out"]["type"] = "int";
    return schema;
}