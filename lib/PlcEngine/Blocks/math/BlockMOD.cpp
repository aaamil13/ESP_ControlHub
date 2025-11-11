#include "BlockMOD.h"

bool BlockMOD::configure(const JsonObject& config, PlcMemory& memory) {
    if (config.containsKey("inputs")) {
        input1_var = config["inputs"]["in1"].as<std::string>();
        input2_var = config["inputs"]["in2"].as<std::string>();
    }
    if (config.containsKey("outputs") && config["outputs"].containsKey("out")) {
        output_var = config["outputs"]["out"].as<std::string>();
    }
    return true; // Basic validation for now
}

void BlockMOD::evaluate(PlcMemory& memory) {
    if (input1_var.empty() || input2_var.empty() || output_var.empty()) {
        return; // Not configured
    }

    // Assuming integer modulo for now, can be extended with type checking
    int16_t in1_val = memory.getValue<int16_t>(input1_var, 0);
    int16_t in2_val = memory.getValue<int16_t>(input2_var, 0);

    if (in2_val != 0) {
        memory.setValue<int16_t>(output_var, in1_val % in2_val);
    } else {
        // Handle division by zero, e.g., set output to 0
        memory.setValue<int16_t>(output_var, 0);
    }
}

JsonDocument BlockMOD::getBlockSchema() {
    JsonDocument schema;
    schema["description"] = "Modulo block";
    schema["inputs"]["in1"]["type"] = "int";
    schema["inputs"]["in2"]["type"] = "int";
    schema["outputs"]["out"]["type"] = "int";
    return schema;
}