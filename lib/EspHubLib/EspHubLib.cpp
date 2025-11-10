#include "EspHubLib.h"
#include "mesh_protocol.h" // New mesh protocol header
#include <WiFi.h>

StreamLogger* EspHubLog = nullptr;
EspHub* EspHub::instance = nullptr;

// Need a forward declaration for the scheduler
Scheduler meshScheduler;

EspHub::EspHub() : plcEngine(&timeManager, &meshDeviceManager), webManager(&plcEngine, &meshDeviceManager), mqttDiscoveryManager(&mqttManager, &plcEngine), otaManager(), logger(webManager) {
    instance = this;
    EspHubLog = &logger;
}

void EspHub::begin() {
    webManager.begin();
    plcEngine.begin();
    appManager.begin(plcEngine, webManager.getServer()); // Pass webManager's server
    meshDeviceManager.begin(); // Initialize MeshDeviceManager
    userManager.begin(); // Initialize UserManager
    otaManager.begin(); // Initialize OtaManager

    // painlessMesh initialization
    // mesh.setDebugMsgTypes(ERROR | STARTUP); // set before init() so that you can see startup messages
    // mesh.init("EspHubMesh", "password1234", &meshScheduler, 5566);
    // mesh.onReceive(&receivedCallback);
    // mesh.onNewConnection(&newConnectionCallback);
    // mesh.onChangedConnections(&changedConnectionCallback);
    // mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);

    // EspHubLog->println("EspHub Library Initialized with painlessMesh");
}

void EspHub::setupMesh(const char* password) {
    if (strlen(password) == 0) {
        EspHubLog->println("ERROR: Mesh password is empty. Mesh network will not be started.");
        return;
    }
    mesh.setDebugMsgTypes(ERROR | STARTUP); // set before init() so that you can see startup messages
    mesh.init("EspHubMesh", password, &meshScheduler, 5566);
    mesh.onReceive(&receivedCallback);
    mesh.onNewConnection(&newConnectionCallback);
    mesh.onChangedConnections(&changedConnectionCallback);
    mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);

    EspHubLog->println("EspHub Library Initialized with painlessMesh");
}

void EspHub::setupMqtt(const char* server, int port, MQTT_CALLBACK_SIGNATURE, bool use_tls, const char* ca_cert_path, const char* client_cert_path, const char* client_key_path) {
    mqttManager.begin(server, port, use_tls, ca_cert_path, client_cert_path, client_key_path);
    mqttManager.setCallback(callback);
}

void EspHub::mqttCallback(char* topic, byte* payload, unsigned int length) {
    // Handle MQTT messages here, e.g., for OTA updates or PLC control
    EspHubLog->printf("MQTT message received on topic: %s\n", topic);
    // Example: Check for OTA update topic
    // TODO: Implement handleMqttMessage in OtaManager
    // if (otaManager.handleMqttMessage(topic, payload, length)) {
    //     EspHubLog->println("OTA update initiated via MQTT.");
    //     return;
    // }

    // Example: PLC control messages
    if (String(topic) == "esphub/plc/control") {
        StaticJsonDocument<200> doc;
        DeserializationError error = deserializeJson(doc, payload, length);
        if (error) {
            EspHubLog->printf("deserializeJson() failed for PLC control message: %s\n", error.c_str());
            return;
        }
        const char* command = doc["command"];
        const char* programName = doc["program"];
        if (command && programName) {
            if (strcmp(command, "run") == 0) {
                runPlc(programName);
                EspHubLog->printf("PLC program '%s' started.\n", programName);
            } else if (strcmp(command, "pause") == 0) {
                pausePlc(programName);
                EspHubLog->printf("PLC program '%s' paused.\n", programName);
            } else if (strcmp(command, "stop") == 0) {
                stopPlc(programName);
                EspHubLog->printf("PLC program '%s' stopped.\n", programName);
            } else if (strcmp(command, "delete") == 0) {
                deletePlc(programName);
                EspHubLog->printf("PLC program '%s' deleted.\n", programName);
            }
        }
    }
}

void EspHub::setupTime(const char* tz_info) {
    timeManager.begin(tz_info);
}

void EspHub::loadPlcConfiguration(const char* jsonConfig) {
    // This method now needs a program name. For simplicity, let's assume a default program name for now.
    // In a real scenario, the program name would come from the web UI or MQTT command.
    String defaultProgramName = "main_program";
    if (plcEngine.loadProgram(defaultProgramName, jsonConfig)) {
        // If PLC config is valid, load the high-level applications
        StaticJsonDocument<4096> doc; // Use StaticJsonDocument for consistency
        DeserializationError error = deserializeJson(doc, jsonConfig);
        if (!error) {
            appManager.loadApplications(doc.as<JsonObject>());
        } else {
            EspHubLog->printf("ERROR: Failed to deserialize PLC config for appManager: %s\n", error.c_str());
        }
    }
}

void EspHub::runPlc(const String& programName) {
    plcEngine.runProgram(programName);
}

void EspHub::pausePlc(const String& programName) {
    plcEngine.pauseProgram(programName);
}

void EspHub::stopPlc(const String& programName) {
    plcEngine.stopProgram(programName);
}

void EspHub::deletePlc(const String& programName) {
    plcEngine.deleteProgram(programName);
}

void EspHub::factoryReset() {
    EspHubLog->println("Performing factory reset...");
    // Clear all NVS namespaces
    Preferences preferences;
    preferences.begin("user_manager", false);
    preferences.clear();
    preferences.end();

    preferences.begin("plc_memory", false);
    preferences.clear();
    preferences.end();

    // Note: WiFiManager removed due to conflict with ESPAsyncWebServer
    // WiFi settings can be cleared manually via WiFi.disconnect(true, true);
    WiFi.disconnect(true, true); // Clear WiFi credentials

    EspHubLog->println("Factory reset complete. Restarting...");
    ESP.restart();
}

void EspHub::restartEsp() {
    EspHubLog->println("Restarting ESP...");
    ESP.restart();
}

void EspHub::loop() {
    mesh.update();
    appManager.updateAll();
    meshDeviceManager.checkOfflineDevices(60000); // Check for offline devices every minute (60 seconds)
    plcEngine.evaluateAllPrograms(); // Evaluate all running PLC programs
    if (mesh.isRoot()) {
        mqttManager.loop();
        // Publish MQTT Discovery messages periodically
        static unsigned long lastDiscoveryPublish = 0;
        if (millis() - lastDiscoveryPublish > 60000) { // Publish every minute
            mqttDiscoveryManager.publishDiscoveryMessages();
            lastDiscoveryPublish = millis();
        }
    }
}

void EspHub::receivedCallback(uint32_t from, String &msg) {
    EspHubLog->printf("Received from %u: %s\n", from, msg.c_str());

    StaticJsonDocument<512> doc; // Use StaticJsonDocument for mesh messages
    if (parseMeshMessage(msg, doc)) {
        MeshMessageType type = static_cast<MeshMessageType>(doc["type"].as<int>());
        switch (type) {
            case MESH_MSG_TYPE_REGISTRATION: {
                // Handle registration
                const char* device_name = doc["name"].as<const char*>();
                instance->meshDeviceManager.addDevice(from, device_name);
                EspHubLog->printf("Mesh Registration from %u: Name %s\n", from, device_name);
                break;
            }
            case MESH_MSG_TYPE_SENSOR_DATA: {
                // Update PLC memory with sensor data
                const char* var_name = doc["var_name"].as<const char*>();
                // Assuming "main_program" is the default program for now
                PlcProgram* mainProgram = instance->plcEngine.getProgram("main_program");
                if (mainProgram) {
                    if (doc["value"].is<bool>()) {
                        mainProgram->getMemory().setValue<bool>(var_name, doc["value"].as<bool>());
                    } else if (doc["value"].is<float>()) {
                        mainProgram->getMemory().setValue<float>(var_name, doc["value"].as<float>());
                    } else if (doc["value"].is<int>()) {
                        mainProgram->getMemory().setValue<int16_t>(var_name, doc["value"].as<int>());
                    }
                    instance->meshDeviceManager.updateDeviceLastSeen(from);
                    EspHubLog->printf("Mesh Sensor Data from %u: %s = %s\n", from, var_name, msg.c_str());
                } else {
                    EspHubLog->printf("ERROR: Main PLC program not loaded, cannot update sensor data for %s.\n", var_name);
                }
                break;
            }
            case MESH_MSG_TYPE_ACTUATOR_COMMAND: {
                // This message type would typically be sent *to* a device, not received by the hub
                EspHubLog->printf("Received unexpected ACTUATOR_COMMAND from %u\n", from);
                break;
            }
            case MESH_MSG_TYPE_HEARTBEAT: {
                instance->meshDeviceManager.updateDeviceLastSeen(from);
                EspHubLog->printf("Mesh Heartbeat from %u\n", from);
                break;
            }
            default:
                EspHubLog->printf("Received unknown mesh message type from %u\n", from);
                break;
        }
    }
}

void EspHub::newConnectionCallback(uint32_t nodeId) {
    EspHubLog->printf("New Connection, nodeId = %u\n", nodeId);
}

void EspHub::changedConnectionCallback() {
    EspHubLog->printf("Changed connections\n");
}

void EspHub::nodeTimeAdjustedCallback(int32_t offset) {
    EspHubLog->printf("Adjusted time %u. Offset = %d\n", instance->mesh.getNodeTime(), offset);
}