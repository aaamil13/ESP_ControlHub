#include "BlockStringFind.h"

bool BlockStringFind::configure(const JsonObject& config, PlcMemory& memory) {
    if (config.containsKey("inputs")) {
        input_string_var = config["inputs"]["string"].as<std::string>();
        substring_var = config["inputs"]["substring"].as<std::string>();
    }
    if (config.containsKey("outputs") && config["outputs"].containsKey("index")) {
        output_index_var = config["outputs"]["index"].as<std::string>();
    }
    return true; // Basic validation for now
}

void BlockStringFind::evaluate(PlcMemory& memory) {
    if (input_string_var.empty() || substring_var.empty() || output_index_var.empty()) {
        return; // Not configured
    }

    String input_str = memory.getValue<String>(input_string_var, "");
    String substring_str = memory.getValue<String>(substring_var, "");

    int index = input_str.indexOf(substring_str);
    memory.setValue<int16_t>(output_index_var, index);
}

JsonDocument BlockStringFind::getBlockSchema() {
    JsonDocument schema;
    schema["description"] = "Finds a substring within a string";
    schema["inputs"]["string"]["type"] = "string";
    schema["inputs"]["substring"]["type"] = "string";
    schema["outputs"]["index"]["type"] = "int";
    return schema;
}