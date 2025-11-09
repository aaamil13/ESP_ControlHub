#include "BlockCTD.h"

BlockCTD::BlockCTD() : last_cd_state(false) {
}

bool BlockCTD::configure(const JsonObject& config, PlcMemory& memory) {
    if (config.containsKey("inputs")) {
        cd_var = config["inputs"]["cd"].as<std::string>();
        load_var = config["inputs"]["load"].as<std::string>();
        pv_var = config["inputs"]["pv"].as<std::string>();
    }
    if (config.containsKey("outputs")) {
        q_var = config["outputs"]["q"].as<std::string>();
        cv_var = config["outputs"]["cv"].as<std::string>();
    }
    return true; // Basic validation for now
}

void BlockCTD::evaluate(PlcMemory& memory) {
    bool cd = memory.getValue<bool>(cd_var, false);
    bool load = memory.getValue<bool>(load_var, false);
    int16_t pv = memory.getValue<int16_t>(pv_var, 0);
    int16_t cv = memory.getValue<int16_t>(cv_var, 0);

    if (load) {
        cv = pv;
    } else if (cd && !last_cd_state) { // Falling edge of CD
        if (cv > 0) {
            cv--;
        }
    }

    memory.setValue<int16_t>(cv_var, cv);
    memory.setValue<bool>(q_var, (cv == 0));

    last_cd_state = cd;
}

JsonDocument BlockCTD::getBlockSchema() {
    JsonDocument schema;
    schema["description"] = "Count Down block";
    schema["inputs"]["cd"]["type"] = "bool";
    schema["inputs"]["load"]["type"] = "bool";
    schema["inputs"]["pv"]["type"] = "int";
    schema["outputs"]["q"]["type"] = "bool";
    schema["outputs"]["cv"]["type"] = "int";
    return schema;
}