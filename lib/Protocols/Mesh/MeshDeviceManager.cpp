#include "MeshDeviceManager.h"
#include "StreamLogger.h" // For Log
#include "../PlcCore/PlcEngine.h" // For accessing PLC memory

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
    newDevice.isOnline = true; // New devices are online by default
    devices[nodeId] = newDevice;
    Log->printf("Registered new mesh device: %u (%s)\n", nodeId, name.c_str());
    // Save to NVS here
}

void MeshDeviceManager::updateDeviceLastSeen(uint32_t nodeId) {
    if (devices.count(nodeId)) {
        devices[nodeId].lastSeen = millis();
        if (!devices[nodeId].isOnline) {
            devices[nodeId].isOnline = true;
            Log->printf("Device %u (%s) is back online.\n", nodeId, devices[nodeId].name.c_str());
            // Trigger PLC logic for device online event
        }
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

void MeshDeviceManager::checkOfflineDevices(unsigned long offlineTimeoutMs) {
    unsigned long currentMillis = millis();
    for (auto& pair : devices) {
        MeshDevice& device = pair.second;
        if (device.isOnline && (currentMillis - device.lastSeen > offlineTimeoutMs)) {
            device.isOnline = false;
            Log->printf("Device %u (%s) is offline.\n", device.nodeId, device.name.c_str());
            // Trigger PLC logic for device offline event
            // This would involve setting a PLC variable associated with this device to false
            // For example: _plcEngine->getMemory().setValue<bool>("device." + device.name + ".online", false);
        }
    }
}