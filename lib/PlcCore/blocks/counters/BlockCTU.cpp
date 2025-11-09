#include "BlockCTU.h"

BlockCTU::BlockCTU() : last_cu_state(false) {
}

bool BlockCTU::configure(const JsonObject& config, PlcMemory& memory) {
    if (config.containsKey("inputs")) {
        cu_var = config["inputs"]["cu"].as<std::string>();
        reset_var = config["inputs"]["reset"].as<std::string>();
        pv_var = config["inputs"]["pv"].as<std::string>();
    }
    if (config.containsKey("outputs")) {
        q_var = config["outputs"]["q"].as<std::string>();
        cv_var = config["outputs"]["cv"].as<std::string>();
    }
    return true; // Basic validation for now
}

void BlockCTU::evaluate(PlcMemory& memory) {
    bool cu = memory.getValue<bool>(cu_var, false);
    bool reset = memory.getValue<bool>(reset_var, false);
    int16_t pv = memory.getValue<int16_t>(pv_var, 0);
    int16_t cv = memory.getValue<int16_t>(cv_var, 0);

    if (reset) {
        cv = 0;
    } else if (cu && !last_cu_state) { // Rising edge of CU
        if (cv < pv) {
            cv++;
        }
    }

    memory.setValue<int16_t>(cv_var, cv);
    memory.setValue<bool>(q_var, (cv >= pv));

    last_cu_state = cu;
}

JsonDocument BlockCTU::getBlockSchema() {
    JsonDocument schema;
    schema["description"] = "Count Up block";
    schema["inputs"]["cu"]["type"] = "bool";
    schema["inputs"]["reset"]["type"] = "bool";
    schema["inputs"]["pv"]["type"] = "int";
    schema["outputs"]["q"]["type"] = "bool";
    schema["outputs"]["cv"]["type"] = "int";
    return schema;
}