#include "EspHubLib.h"
#include "mesh_protocol.h" // New mesh protocol header
#include <WiFi.h>

StreamLogger* Log = nullptr;
EspHub* EspHub::instance = nullptr;

// Need a forward declaration for the scheduler
Scheduler meshScheduler;

EspHub::EspHub() : plcEngine(&timeManager, &meshDeviceManager), webManager(&plcEngine, &meshDeviceManager), mqttDiscoveryManager(&mqttManager, &plcEngine), logger(webManager) {
    instance = this;
    Log = &logger;
}

void EspHub::begin() {
    webManager.begin();
    plcEngine.begin();
    appManager.begin(plcEngine, webManager.getServer()); // Pass webManager's server
    meshDeviceManager.begin(); // Initialize MeshDeviceManager
    userManager.begin(); // Initialize UserManager

    // painlessMesh initialization
    // mesh.setDebugMsgTypes(ERROR | STARTUP); // set before init() so that you can see startup messages
    // mesh.init("EspHubMesh", "password1234", &meshScheduler, 5566);
    // mesh.onReceive(&receivedCallback);
    // mesh.onNewConnection(&newConnectionCallback);
    // mesh.onChangedConnections(&changedConnectionCallback);
    // mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);

    // Log->println("EspHub Library Initialized with painlessMesh");
}

void EspHub::setupMesh(const char* password) {
    if (strlen(password) == 0) {
        Log->println("ERROR: Mesh password is empty. Mesh network will not be started.");
        return;
    }
    mesh.setDebugMsgTypes(ERROR | STARTUP); // set before init() so that you can see startup messages
    mesh.init("EspHubMesh", password, &meshScheduler, 5566);
    mesh.onReceive(&receivedCallback);
    mesh.onNewConnection(&newConnectionCallback);
    mesh.onChangedConnections(&changedConnectionCallback);
    mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);

    Log->println("EspHub Library Initialized with painlessMesh");
}

void EspHub::setupMqtt(const char* server, int port, MQTT_CALLBACK_SIGNATURE, bool use_tls) {
    mqttManager.begin(server, port, use_tls);
    mqttManager.setCallback(callback);
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
            Log->printf("ERROR: Failed to deserialize PLC config for appManager: %s\n", error.c_str());
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
    Log->println("Performing factory reset...");
    // Clear all NVS namespaces
    Preferences preferences;
    preferences.begin("user_manager", false);
    preferences.clear();
    preferences.end();

    preferences.begin("plc_memory", false);
    preferences.clear();
    preferences.end();

    // Clear WiFiManager settings
    WiFiManager wm;
    wm.resetSettings();

    Log->println("Factory reset complete. Restarting...");
    ESP.restart();
}

void EspHub::restartEsp() {
    Log->println("Restarting ESP...");
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
    Log->printf("Received from %u: %s\n", from, msg.c_str());

    StaticJsonDocument<512> doc; // Use StaticJsonDocument for mesh messages
    if (parseMeshMessage(msg, doc)) {
        MeshMessageType type = static_cast<MeshMessageType>(doc["type"].as<int>());
        switch (type) {
            case MESH_MSG_TYPE_REGISTRATION: {
                // Handle registration
                const char* device_name = doc["name"].as<const char*>();
                instance->meshDeviceManager.addDevice(from, device_name);
                Log->printf("Mesh Registration from %u: Name %s\n", from, device_name);
                break;
            }
            case MESH_MSG_TYPE_SENSOR_DATA: {
                // Update PLC memory with sensor data
                const char* var_name = doc["var_name"].as<const char*>();
                if (doc["value"].is<bool>()) {
                    instance->plcEngine.getProgram("main_program")->getMemory().setValue<bool>(var_name, doc["value"].as<bool>());
                } else if (doc["value"].is<float>()) {
                    instance->plcEngine.getProgram("main_program")->getMemory().setValue<float>(var_name, doc["value"].as<float>());
                } else if (doc["value"].is<int>()) {
                    instance->plcEngine.getProgram("main_program")->getMemory().setValue<int16_t>(var_name, doc["value"].as<int>());
                }
                instance->meshDeviceManager.updateDeviceLastSeen(from);
                Log->printf("Mesh Sensor Data from %u: %s = %s\n", from, var_name, msg.c_str());
                break;
            }
            case MESH_MSG_TYPE_ACTUATOR_COMMAND: {
                // This message type would typically be sent *to* a device, not received by the hub
                Log->printf("Received unexpected ACTUATOR_COMMAND from %u\n", from);
                break;
            }
            case MESH_MSG_TYPE_HEARTBEAT: {
                instance->meshDeviceManager.updateDeviceLastSeen(from);
                Log->printf("Mesh Heartbeat from %u\n", from);
                break;
            }
            default:
                Log->printf("Received unknown mesh message type from %u\n", from);
                break;
        }
    }
}

void EspHub::newConnectionCallback(uint32_t nodeId) {
    Log->printf("New Connection, nodeId = %u\n", nodeId);
}

void EspHub::changedConnectionCallback() {
    Log->printf("Changed connections\n");
}

void EspHub::nodeTimeAdjustedCallback(int32_t offset) {
    Log->printf("Adjusted time %u. Offset = %d\n", instance->mesh.getNodeTime(), offset);
}