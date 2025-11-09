#include "BlockStringCopy.h"

bool BlockStringCopy::configure(const JsonObject& config, PlcMemory& memory) {
    if (config.containsKey("inputs")) {
        source_var = config["inputs"]["source"].as<std::string>();
        start_index = config["inputs"]["start_index"] | 0;
        length = config["inputs"]["length"] | -1; // -1 means copy to end
    }
    if (config.containsKey("outputs") && config["outputs"].containsKey("destination")) {
        destination_var = config["outputs"]["destination"].as<std::string>();
    }
    return true; // Basic validation for now
}

void BlockStringCopy::evaluate(PlcMemory& memory) {
    if (source_var.empty() || destination_var.empty()) {
        return; // Not configured
    }

    String source_str = memory.getValue<String>(source_var, "");
    String result_str;

    if (length == -1) {
        result_str = source_str.substring(start_index);
    } else {
        result_str = source_str.substring(start_index, start_index + length);
    }
    memory.setValue<String>(destination_var, result_str);
}

JsonDocument BlockStringCopy::getBlockSchema() {
    JsonDocument schema;
    schema["description"] = "Copies a substring from a source string";
    schema["inputs"]["source"]["type"] = "string";
    schema["inputs"]["start_index"]["type"] = "int";
    schema["inputs"]["length"]["type"] = "int";
    schema["outputs"]["destination"]["type"] = "string";
    return schema;
}