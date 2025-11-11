#include "BlockStringConcat.h"

bool BlockStringConcat::configure(const JsonObject& config, PlcMemory& memory) {
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

void BlockStringConcat::evaluate(PlcMemory& memory) {
    if (output_var.empty() || input_vars.empty()) {
        return; // Not configured
    }

    String result = "";
    for (const auto& var_name : input_vars) {
        result += memory.getValue<String>(var_name, "");
    }
    memory.setValue<String>(output_var, result);
}

JsonDocument BlockStringConcat::getBlockSchema() {
    JsonDocument schema;
    schema["description"] = "String concatenation block";
    schema["inputs"]["in1"]["type"] = "string";
    schema["inputs"]["in2"]["type"] = "string";
    schema["outputs"]["out"]["type"] = "string";
    return schema;
}