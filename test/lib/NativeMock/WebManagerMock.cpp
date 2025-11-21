#include <WebManager.h>

WebManager::WebManager(PlcEngine* plcEngine, MeshDeviceManager* meshDeviceManager, ZigbeeManager* zigbeeManager) 
    : AsyncWebServer(80), ws("/ws"), _plcEngine(plcEngine), _meshDeviceManager(meshDeviceManager), _zigbeeManager(zigbeeManager) {
    instance = this;
}

void WebManager::log(const String& message) {
    printf("[WebManager] %s\n", message.c_str());
}

void WebManager::begin() {}

// Static member definition
WebManager* WebManager::instance = nullptr;
