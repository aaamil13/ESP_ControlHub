#include "DeviceManager.h"
#include "StreamLogger.h"

extern StreamLogger* EspHubLog;

DeviceManager::DeviceManager(const String& protocolName, ProtocolType protocolType)
    : protocolName(protocolName), protocolType(protocolType) {
    registry = &DeviceRegistry::getInstance();
    EspHubLog->printf("DeviceManager for %s initialized\n", protocolName.c_str());
}

DeviceManager::~DeviceManager() {
}

bool DeviceManager::registerDevice(const String& deviceId, const JsonObject& config) {
    DeviceStatus device;
    device.deviceId = deviceId;
    device.protocol = protocolType;
    device.isOnline = true;
    device.lastSeen = millis();
    device.offlineThreshold = config["offline_threshold"] | 60000;

    return registry->registerDevice(device);
}

void DeviceManager::removeDevice(const String& deviceId) {
    // Remove all endpoints for this device
    auto endpoints = registry->getEndpointsByDevice(deviceId);
    for (auto* endpoint : endpoints) {
        registry->removeEndpoint(endpoint->fullName);
    }

    // Note: DeviceRegistry doesn't have removeDevice method yet
    // This is a placeholder
    EspHubLog->printf("Removed device: %s\n", deviceId.c_str());
}

void DeviceManager::updateDeviceStatus(const String& deviceId, bool isOnline) {
    registry->updateDeviceStatus(deviceId, isOnline);

    // Also update all endpoints for this device
    auto endpoints = registry->getEndpointsByDevice(deviceId);
    for (auto* endpoint : endpoints) {
        registry->updateEndpointStatus(endpoint->fullName, isOnline);
    }
}

std::vector<DeviceStatus*> DeviceManager::getAllDevices() {
    // Filter devices by this protocol
    std::vector<DeviceStatus*> result;
    auto allDevices = registry->getAllDevices();
    for (auto* device : allDevices) {
        if (device->protocol == protocolType) {
            result.push_back(device);
        }
    }
    return result;
}

DeviceStatus* DeviceManager::getDevice(const String& deviceId) {
    return registry->getDevice(deviceId);
}

void DeviceManager::checkOfflineDevices(uint32_t timeout_ms) {
    unsigned long now = millis();
    auto devices = getAllDevices();
    for (auto* device : devices) {
        if (device->isOnline && (now - device->lastSeen > timeout_ms)) {
            updateDeviceStatus(device->deviceId, false);
        }
    }
}

// Helper functions

bool DeviceManager::registerEndpointHelper(const Endpoint& endpoint) {
    return registry->registerEndpoint(endpoint);
}

void DeviceManager::updateEndpointStatusHelper(const String& fullName, bool isOnline) {
    registry->updateEndpointStatus(fullName, isOnline);
}

void DeviceManager::updateEndpointValueHelper(const String& fullName, const PlcValue& value) {
    registry->updateEndpointValue(fullName, value);
}

String DeviceManager::buildDeviceId(const String& location, const String& deviceName) {
    return location + "." + protocolName + "." + deviceName;
}
