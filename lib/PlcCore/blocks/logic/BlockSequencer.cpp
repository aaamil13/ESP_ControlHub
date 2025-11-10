#include "BlockSequencer.h"
#include <StreamLogger.h> // For Log

BlockSequencer::BlockSequencer() : current_step(0) {
}

bool BlockSequencer::configure(const JsonObject& config, PlcMemory& memory) {
    if (config.containsKey("outputs")) {
        output_done_var = config["outputs"]["done"].as<std::string>();
        output_active_var = config["outputs"]["active"].as<std::string>();
    }

    if (config.containsKey("steps")) {
        JsonArray steps_cfg = config["steps"].as<JsonArray>();
        for (JsonObject step_cfg : steps_cfg) {
            SequencerStep step;
            step.actions = step_cfg["actions"].as<JsonArray>();
            step.transition_condition_var = step_cfg["transition_condition"].as<std::string>();
            step.timeout_ms = step_cfg["timeout_ms"] | 0;
            steps.push_back(step);
        }
    }
    return true; // Basic validation for now
}

void BlockSequencer::evaluate(PlcMemory& memory) {
    if (steps.empty()) {
        memory.setValue<bool>(output_done_var, true);
        memory.setValue<bool>(output_active_var, false);
        return;
    }

    memory.setValue<bool>(output_active_var, true);
    memory.setValue<bool>(output_done_var, false);

    SequencerStep& current_s = steps[current_step];

    // Execute actions for the current step
    executeActions(current_s.actions, memory);

    // Check for transition condition
    bool transition_met = memory.getValue<bool>(current_s.transition_condition_var, false);
    bool timeout_occurred = false;

    if (current_s.timeout_ms > 0) {
        if (current_s.start_time == 0) { // First entry into this step
            current_s.start_time = millis();
        }
        if (millis() - current_s.start_time >= current_s.timeout_ms) {
            timeout_occurred = true;
            Log->printf("Sequencer timeout in step %d\n", current_step);
        }
    }

    if (transition_met || timeout_occurred) {
        current_step++;
        if (current_step >= steps.size()) {
            current_step = 0; // Loop back to start or stop
            memory.setValue<bool>(output_done_var, true);
            memory.setValue<bool>(output_active_var, false);
        }
        current_s.start_time = 0; // Reset timer for next step
    }
}

void BlockSequencer::executeActions(const JsonArray& actions, PlcMemory& memory) {
    for (JsonObject action : actions) {
        const char* action_type = action["action"];
        if (strcmp(action_type, "set_value") == 0) {
            const char* var_name = action["variable"];
            if (action["value"].is<bool>()) {
                memory.setValue<bool>(var_name, action["value"].as<bool>());
            } else if (action["value"].is<float>()) {
                memory.setValue<float>(var_name, action["value"].as<float>());
            } else if (action["value"].is<int>()) {
                memory.setValue<int16_t>(var_name, action["value"].as<int>());
            }
            // Add other types as needed
        }
    }
}

JsonDocument BlockSequencer::getBlockSchema() {
    JsonDocument schema;
    schema["description"] = "Sequencer block for step-by-step control";
    schema["inputs"]["start"]["type"] = "bool"; // Example input to start sequence
    schema["outputs"]["done"]["type"] = "bool";
    schema["outputs"]["active"]["type"] = "bool";
    schema["steps"]["type"] = "array";
    schema["steps"]["items"]["type"] = "object";
    schema["steps"]["items"]["properties"]["actions"]["type"] = "array";
    schema["steps"]["items"]["properties"]["transition_condition"]["type"] = "string";
    schema["steps"]["items"]["properties"]["timeout_ms"]["type"] = "uint32";
    return schema;
}