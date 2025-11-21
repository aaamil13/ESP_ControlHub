#include <WebManager.h>

WebManager::WebManager(PlcEngine* plcEngine, MeshDeviceManager* meshDeviceManager, ZigbeeManager* zigbeeManager) 
    : _plcEngine(plcEngine), _meshDeviceManager(meshDeviceManager), _zigbeeManager(zigbeeManager), server(80), ws("/ws") {
}

void WebManager::log(const String& message) {
    printf("[WebManager] %s\n", message.c_str());
}

void WebManager::begin() {}

// Static member definition
WebManager* WebManager::instance = nullptr;
