#include "BlockStatusHandler.h"
#include <DeviceRegistry.h>
#include <StreamLogger.h>

extern StreamLogger* EspHubLog;

BlockStatusHandler::BlockStatusHandler()
    : deviceRegistry(nullptr), lastKnownStatus(false), initialized(false) {
}

BlockStatusHandler::~BlockStatusHandler() {
}

void BlockStatusHandler::setDeviceRegistry(DeviceRegistry* registry) {
    deviceRegistry = registry;
}

bool BlockStatusHandler::configure(const JsonObject& config, PlcMemory& memory) {
    // Parse inputs
    if (config["inputs"].is<JsonObject>() && config["inputs"]["endpoint_name"].is<const char*>()) {
        endpoint_name_var = config["inputs"]["endpoint_name"].as<std::string>();
    } else {
        EspHubLog->println("ERROR: StatusHandler requires 'endpoint_name' input");
        return false;
    }

    // Parse outputs
    if (config["outputs"].is<JsonObject>()) {
        JsonObject outputs = config["outputs"];
        if (outputs["is_online"].is<const char*>()) {
            is_online_var = outputs["is_online"].as<std::string>();
        }
        if (outputs["on_online"].is<const char*>()) {
            on_online_var = outputs["on_online"].as<std::string>();
        }
        if (outputs["on_offline"].is<const char*>()) {
            on_offline_var = outputs["on_offline"].as<std::string>();
        }
    }

    // Validate that we have at least one output
    if (is_online_var.empty() && on_online_var.empty() && on_offline_var.empty()) {
        EspHubLog->println("ERROR: StatusHandler requires at least one output");
        return false;
    }

    EspHubLog->printf("StatusHandler configured: monitoring %s\n", endpoint_name_var.c_str());
    return true;
}

void BlockStatusHandler::evaluate(PlcMemory& memory) {
    if (endpoint_name_var.empty() || !deviceRegistry) {
        return;
    }

    // Get the endpoint name from PLC memory
    String currentEndpoint = memory.getValue<String>(endpoint_name_var, String(""));
    if (currentEndpoint.isEmpty()) {
        return;
    }

    // Check if we're monitoring a different endpoint now
    if (currentEndpoint != monitoredEndpoint) {
        monitoredEndpoint = currentEndpoint;
        initialized = false;
        EspHubLog->printf("StatusHandler: Now monitoring %s\n", monitoredEndpoint.c_str());
    }

    // Update status
    updateStatus(memory);
}

void BlockStatusHandler::updateStatus(PlcMemory& memory) {
    // Query current status from DeviceRegistry
    Endpoint* endpoint = deviceRegistry->getEndpoint(monitoredEndpoint);
    bool currentStatus = endpoint ? endpoint->isOnline : false;

    // Detect status changes
    if (!initialized) {
        // First run - set initial status without triggering events
        lastKnownStatus = currentStatus;
        initialized = true;

        // Set current status output
        if (!is_online_var.empty()) {
            memory.setValue<bool>(is_online_var, currentStatus);
        }

        EspHubLog->printf("StatusHandler: Initial status for %s: %s\n",
                         monitoredEndpoint.c_str(),
                         currentStatus ? "ONLINE" : "OFFLINE");
    } else if (currentStatus != lastKnownStatus) {
        // Status changed!
        EspHubLog->printf("StatusHandler: %s changed from %s to %s\n",
                         monitoredEndpoint.c_str(),
                         lastKnownStatus ? "ONLINE" : "OFFLINE",
                         currentStatus ? "ONLINE" : "OFFLINE");

        lastKnownStatus = currentStatus;

        // Update status output
        if (!is_online_var.empty()) {
            memory.setValue<bool>(is_online_var, currentStatus);
        }

        // Trigger appropriate event
        if (currentStatus && !on_online_var.empty()) {
            // Device went ONLINE
            memory.setValue<bool>(on_online_var, true);
            EspHubLog->printf("StatusHandler: Triggered ON_ONLINE for %s\n", monitoredEndpoint.c_str());
        } else if (!currentStatus && !on_offline_var.empty()) {
            // Device went OFFLINE
            memory.setValue<bool>(on_offline_var, true);
            EspHubLog->printf("StatusHandler: Triggered ON_OFFLINE for %s\n", monitoredEndpoint.c_str());
        }
    } else {
        // No change - reset triggers to false
        if (!on_online_var.empty()) {
            memory.setValue<bool>(on_online_var, false);
        }
        if (!on_offline_var.empty()) {
            memory.setValue<bool>(on_offline_var, false);
        }

        // Keep is_online updated
        if (!is_online_var.empty()) {
            memory.setValue<bool>(is_online_var, currentStatus);
        }
    }
}

JsonDocument BlockStatusHandler::getBlockSchema() {
    JsonDocument schema;
    schema["type"] = "StatusHandler";
    schema["description"] = "Monitors endpoint online/offline status and triggers PLC events";
    schema["category"] = "events";

    JsonObject inputs = schema["inputs"].to<JsonObject>();
    inputs["endpoint_name"]["type"] = "string";
    inputs["endpoint_name"]["description"] = "Full endpoint name to monitor (e.g., 'kitchen.zigbee.relay.switch1.bool')";

    JsonObject outputs = schema["outputs"].to<JsonObject>();
    outputs["is_online"]["type"] = "bool";
    outputs["is_online"]["description"] = "Current online status of the endpoint";
    outputs["on_online"]["type"] = "bool";
    outputs["on_online"]["description"] = "Trigger (one-shot) when device goes online";
    outputs["on_offline"]["type"] = "bool";
    outputs["on_offline"]["description"] = "Trigger (one-shot) when device goes offline";

    return schema;
}
