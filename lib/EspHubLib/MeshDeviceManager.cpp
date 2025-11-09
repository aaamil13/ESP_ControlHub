#include "MeshDeviceManager.h"
#include "StreamLogger.h" // For Log

extern StreamLogger* Log;

MeshDeviceManager::MeshDeviceManager() {
}

void MeshDeviceManager::begin() {
    // Load devices from NVS if persistence is implemented
    Log->println("MeshDeviceManager initialized.");
}

void MeshDeviceManager::addDevice(uint32_t nodeId, const String& name) {
    if (devices.count(nodeId)) {
        Log->printf("Device %u already registered.\n", nodeId);
        return;
    }
    MeshDevice newDevice;
    newDevice.nodeId = nodeId;
    newDevice.name = name;
    newDevice.lastSeen = millis();
    devices[nodeId] = newDevice;
    Log->printf("Registered new mesh device: %u (%s)\n", nodeId, name.c_str());
    // Save to NVS here
}

void MeshDeviceManager::updateDeviceLastSeen(uint32_t nodeId) {
    if (devices.count(nodeId)) {
        devices[nodeId].lastSeen = millis();
    } else {
        Log->printf("WARNING: Heartbeat from unknown device %u\n", nodeId);
    }
}

MeshDevice* MeshDeviceManager::getDevice(uint32_t nodeId) {
    if (devices.count(nodeId)) {
        return &devices[nodeId];
    }
    return nullptr;
}

std::vector<MeshDevice> MeshDeviceManager::getAllDevices() {
    std::vector<MeshDevice> allDevices;
    for (auto const& [nodeId, device] : devices) {
        allDevices.push_back(device);
    }
    return allDevices;
}