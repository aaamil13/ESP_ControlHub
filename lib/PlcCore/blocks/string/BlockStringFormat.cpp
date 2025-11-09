#include "BlockStringFormat.h"

bool BlockStringFormat::configure(const JsonObject& config, PlcMemory& memory) {
    if (config.containsKey("inputs")) {
        format_string_var = config["inputs"]["format_string"].as<std::string>();
        JsonArray vars_to_format = config["inputs"]["vars"].as<JsonArray>();
        for (JsonVariant v : vars_to_format) {
            input_vars.push_back(v.as<std::string>());
        }
    }
    if (config.containsKey("outputs") && config["outputs"].containsKey("out")) {
        output_var = config["outputs"]["out"].as<std::string>();
    }
    return true; // Basic validation for now
}

void BlockStringFormat::evaluate(PlcMemory& memory) {
    if (format_string_var.empty() || output_var.empty()) {
        return; // Not configured
    }

    String format_str = memory.getValue<String>(format_string_var, "");
    // This is a simplified implementation. A full implementation would need to
    // parse the format string and replace placeholders with actual variable values.
    // For now, we'll just concatenate the format string with the first input variable.
    String result_str = format_str;
    if (!input_vars.empty()) {
        result_str += memory.getValue<String>(input_vars[0], "");
    }
    memory.setValue<String>(output_var, result_str);
}

JsonDocument BlockStringFormat::getBlockSchema() {
    JsonDocument schema;
    schema["description"] = "Formats a string with variables";
    schema["inputs"]["format_string"]["type"] = "string";
    schema["inputs"]["vars"]["type"] = "array";
    schema["inputs"]["vars"]["items"]["type"] = "string";
    schema["outputs"]["out"]["type"] = "string";
    return schema;
}