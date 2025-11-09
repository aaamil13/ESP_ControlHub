#include "BlockCTUD.h"

BlockCTUD::BlockCTUD() : last_cu_state(false), last_cd_state(false) {
}

bool BlockCTUD::configure(const JsonObject& config, PlcMemory& memory) {
    if (config.containsKey("inputs")) {
        cu_var = config["inputs"]["cu"].as<std::string>();
        cd_var = config["inputs"]["cd"].as<std::string>();
        reset_var = config["inputs"]["reset"].as<std::string>();
        load_var = config["inputs"]["load"].as<std::string>();
        pv_var = config["inputs"]["pv"].as<std::string>();
    }
    if (config.containsKey("outputs")) {
        qu_var = config["outputs"]["qu"].as<std::string>();
        qd_var = config["outputs"]["qd"].as<std::string>();
        cv_var = config["outputs"]["cv"].as<std::string>();
    }
    return true; // Basic validation for now
}

void BlockCTUD::evaluate(PlcMemory& memory) {
    bool cu = memory.getValue<bool>(cu_var, false);
    bool cd = memory.getValue<bool>(cd_var, false);
    bool reset = memory.getValue<bool>(reset_var, false);
    bool load = memory.getValue<bool>(load_var, false);
    int16_t pv = memory.getValue<int16_t>(pv_var, 0);
    int16_t cv = memory.getValue<int16_t>(cv_var, 0);

    if (reset) {
        cv = 0;
    } else if (load) {
        cv = pv;
    } else {
        if (cu && !last_cu_state) { // Rising edge of CU
            if (cv < pv) {
                cv++;
            }
        }
        if (cd && !last_cd_state) { // Rising edge of CD
            if (cv > 0) {
                cv--;
            }
        }
    }

    memory.setValue<int16_t>(cv_var, cv);
    memory.setValue<bool>(qu_var, (cv >= pv));
    memory.setValue<bool>(qd_var, (cv <= 0));

    last_cu_state = cu;
    last_cd_state = cd;
}

JsonDocument BlockCTUD::getBlockSchema() {
    JsonDocument schema;
    schema["description"] = "Count Up/Down block";
    schema["inputs"]["cu"]["type"] = "bool";
    schema["inputs"]["cd"]["type"] = "bool";
    schema["inputs"]["reset"]["type"] = "bool";
    schema["inputs"]["load"]["type"] = "bool";
    schema["inputs"]["pv"]["type"] = "int";
    schema["outputs"]["qu"]["type"] = "bool";
    schema["outputs"]["qd"]["type"] = "bool";
    schema["outputs"]["cv"]["type"] = "int";
    return schema;
}