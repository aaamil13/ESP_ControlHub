#include "BlockTON.h"

BlockTON::BlockTON() : preset_time(0), start_time(0), timing(false) {
}

bool BlockTON::configure(const JsonObject& config, PlcMemory& memory) {
    if (config.containsKey("inputs")) {
        input_var = config["inputs"]["in"].as<std::string>();
        preset_time = config["inputs"]["pt"];
    }
    if (config.containsKey("outputs")) {
        output_var_q = config["outputs"]["q"].as<std::string>();
        if (config["outputs"].containsKey("et")) {
            output_var_et = config["outputs"]["et"].as<std::string>();
        }
    }
    return true;
}

JsonDocument BlockTON::getBlockSchema() {
    JsonDocument schema;
    schema["description"] = "Timer ON Delay block";
    schema["inputs"]["in"]["type"] = "bool";
    schema["inputs"]["pt"]["type"] = "uint32";
    schema["outputs"]["q"]["type"] = "bool";
    schema["outputs"]["et"]["type"] = "uint32";
    return schema;
}

void BlockTON::evaluate(PlcMemory& memory) {
    bool in = memory.getValue<bool>(input_var, false);
    unsigned long current_time = millis();
    unsigned long elapsed_time = 0;

    if (in && !timing) {
        // Rising edge
        timing = true;
        start_time = current_time;
    }

    if (timing) {
        elapsed_time = current_time - start_time;
        if (elapsed_time >= preset_time) {
            memory.setValue<bool>(output_var_q, true);
            elapsed_time = preset_time;
        }
    }

    if (!in) {
        timing = false;
        memory.setValue<bool>(output_var_q, false);
        elapsed_time = 0;
    }

    if (!output_var_et.empty()) {
        memory.setValue<uint32_t>(output_var_et, elapsed_time);
    }
}