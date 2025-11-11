#include "BlockTimeCompare.h"

BlockTimeCompare::BlockTimeCompare(TimeManager* timeManager) {
    _timeManager = timeManager;
}

bool BlockTimeCompare::configure(const JsonObject& config, PlcMemory& memory) {
    if (config.containsKey("outputs") && config["outputs"].containsKey("out")) {
        output_var = config["outputs"]["out"].as<std::string>();
    }
    if (config.containsKey("time")) {
        hour = config["time"]["hour"] | 0;
        minute = config["time"]["minute"] | 0;
        second = config["time"]["second"] | 0;
    }
    return true; // Basic validation for now
}

void BlockTimeCompare::evaluate(PlcMemory& memory) {
    if (output_var.empty() || !_timeManager || !_timeManager->isTimeSet()) {
        memory.setValue<bool>(output_var, false);
        return;
    }

    struct tm timeinfo;
    getLocalTime(&timeinfo);

    bool result = (timeinfo.tm_hour == hour && timeinfo.tm_min == minute && timeinfo.tm_sec == second);
    memory.setValue<bool>(output_var, result);
}

JsonDocument BlockTimeCompare::getBlockSchema() {
    JsonDocument schema;
    schema["description"] = "Time comparison block";
    schema["inputs"]["time"]["type"] = "object";
    schema["inputs"]["time"]["properties"]["hour"]["type"] = "int";
    schema["inputs"]["time"]["properties"]["minute"]["type"] = "int";
    schema["inputs"]["time"]["properties"]["second"]["type"] = "int";
    schema["outputs"]["out"]["type"] = "bool";
    return schema;
}