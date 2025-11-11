#include "BlockGT.h"

bool BlockGT::configure(const JsonObject& config, PlcMemory& memory) {
    if (config.containsKey("inputs")) {
        input1_var = config["inputs"]["in1"].as<std::string>();
        input2_var = config["inputs"]["in2"].as<std::string>();
    }
    if (config.containsKey("outputs") && config["outputs"].containsKey("out")) {
        output_var = config["outputs"]["out"].as<std::string>();
    }
    return true; // Basic validation for now
}

void BlockGT::evaluate(PlcMemory& memory) {
    if (input1_var.empty() || input2_var.empty() || output_var.empty()) {
        return; // Not configured
    }

    // Assuming float comparison for now, can be extended with type checking
    float in1_val = memory.getValue<float>(input1_var, 0.0f);
    float in2_val = memory.getValue<float>(input2_var, 0.0f);

    memory.setValue<bool>(output_var, in1_val > in2_val);
}

JsonDocument BlockGT::getBlockSchema() {
    JsonDocument schema;
    schema["description"] = "Greater Than comparison block";
    schema["inputs"]["in1"]["type"] = "float";
    schema["inputs"]["in2"]["type"] = "float";
    schema["outputs"]["out"]["type"] = "bool";
    return schema;
}