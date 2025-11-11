#include "BlockInt32ToTime.h"

bool BlockInt32ToTime::configure(const JsonObject& config, PlcMemory& memory) {
    if (config.containsKey("inputs") && config["inputs"].containsKey("in")) {
        input_var = config["inputs"]["in"].as<std::string>();
    }
    if (config.containsKey("outputs")) {
        output_var_hour = config["outputs"]["hour"].as<std::string>();
        output_var_minute = config["outputs"]["minute"].as<std::string>();
        output_var_second = config["outputs"]["second"].as<std::string>();
    }
    return true; // Basic validation for now
}

void BlockInt32ToTime::evaluate(PlcMemory& memory) {
    if (input_var.empty() || output_var_hour.empty() || output_var_minute.empty() || output_var_second.empty()) {
        return; // Not configured
    }

    time_t unix_time = memory.getValue<uint32_t>(input_var, 0);
    struct tm timeinfo;
    gmtime_r(&unix_time, &timeinfo); // Use gmtime_r for thread safety

    memory.setValue<int16_t>(output_var_hour, timeinfo.tm_hour);
    memory.setValue<int16_t>(output_var_minute, timeinfo.tm_min);
    memory.setValue<int16_t>(output_var_second, timeinfo.tm_sec);
}

JsonDocument BlockInt32ToTime::getBlockSchema() {
    JsonDocument schema;
    schema["description"] = "Converts Unix timestamp (int32_t) to hour, minute, second";
    schema["inputs"]["in"]["type"] = "int32";
    schema["outputs"]["hour"]["type"] = "int16";
    schema["outputs"]["minute"]["type"] = "int16";
    schema["outputs"]["second"]["type"] = "int16";
    return schema;
}