#include "EspHubLib.h"
#include "esp_hub_protocol.h"
#include <WiFi.h>

StreamLogger* Log = nullptr;
EspHub* EspHub::instance = nullptr;

// Need a forward declaration for the scheduler
Scheduler meshScheduler;

EspHub::EspHub() : logger(webManager) {
    instance = this;
    Log = &logger;
}

void EspHub::begin() {
    deviceManager.begin();
    webManager.begin();
    plcEngine.begin();

    // painlessMesh initialization
    mesh.setDebugMsgTypes(ERROR | STARTUP); // set before init() so that you can see startup messages
    mesh.init("EspHubMesh", "password1234", &meshScheduler, 5566);
    mesh.onReceive(&receivedCallback);
    mesh.onNewConnection(&newConnectionCallback);
    mesh.onChangedConnections(&changedConnectionCallback);
    mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);

    Log->println("EspHub Library Initialized with painlessMesh");
}

void EspHub::loadPlcConfiguration(const char* jsonConfig) {
    plcEngine.loadConfiguration(jsonConfig);
}

void EspHub::runPlc() {
    plcEngine.run();
}

void EspHub::stopPlc() {
    plcEngine.stop();
}

void EspHub::loop() {
    mesh.update();
    mqttManager.loop();
}

void EspHub::receivedCallback(uint32_t from, String &msg) {
    Log->printf("Received from %u: %s\n", from, msg.c_str());

    // Here we would parse the message and act on it
    // For example, if it's a data message, we'd publish it to MQTT
    // if (mesh.isRoot()) {
    //     instance->mqttManager.publish("esphub/data", msg.c_str());
    // }
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