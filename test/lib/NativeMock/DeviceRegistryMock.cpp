#include "DeviceRegistry.h"
#include "StreamLogger.h"

extern StreamLogger* EspHubLog;

// Singleton instance
DeviceRegistry& DeviceRegistry::getInstance() {
    static DeviceRegistry instance;
    return instance;
}

DeviceRegistry::DeviceRegistry() : plcMemory(nullptr) {
}

DeviceRegistry::~DeviceRegistry() {
}

bool DeviceRegistry::registerEndpoint(const Endpoint& endpoint) {
    endpoints[endpoint.fullName] = endpoint;
    return true;
}

bool DeviceRegistry::removeEndpoint(const String& fullName) {
    endpoints.erase(fullName);
    return true;
}

Endpoint* DeviceRegistry::getEndpoint(const String& fullName) {
    if (endpoints.find(fullName) != endpoints.end()) {
        return &endpoints[fullName];
    }
    return nullptr;
}

std::vector<Endpoint*> DeviceRegistry::getAllEndpoints() {
    std::vector<Endpoint*> result;
    for (auto& pair : endpoints) {
        result.push_back(&pair.second);
    }
    return result;
}

std::vector<Endpoint*> DeviceRegistry::getEndpointsByProtocol(ProtocolType protocol) {
    return {};
}

std::vector<Endpoint*> DeviceRegistry::getEndpointsByLocation(const String& location) {
    return {};
}

std::vector<Endpoint*> DeviceRegistry::getEndpointsByDevice(const String& deviceId) {
    return {};
}

void DeviceRegistry::updateEndpointStatus(const String& fullName, bool isOnline) {
}

void DeviceRegistry::updateEndpointValue(const String& fullName, const PlcValue& value) {
    if (endpoints.find(fullName) != endpoints.end()) {
        endpoints[fullName].currentValue = value;
    }
}

void DeviceRegistry::checkOfflineDevices(uint32_t timeout_ms) {
}

bool DeviceRegistry::registerDevice(const DeviceStatus& device) {
    return true;
}

DeviceStatus* DeviceRegistry::getDevice(const String& deviceId) {
    return nullptr;
}

std::vector<DeviceStatus*> DeviceRegistry::getAllDevices() {
    return {};
}

void DeviceRegistry::updateDeviceStatus(const String& deviceId, bool isOnline) {
}

bool DeviceRegistry::registerIOPoint(const PlcIOPoint& ioPoint) {
    ioPoints[ioPoint.plcVarName] = ioPoint;
    return true;
}

bool DeviceRegistry::unregisterIOPoint(const String& plcVarName) {
    ioPoints.erase(plcVarName);
    return true;
}

PlcIOPoint* DeviceRegistry::getIOPoint(const String& plcVarName) {
    if (ioPoints.find(plcVarName) != ioPoints.end()) {
        return &ioPoints[plcVarName];
    }
    return nullptr;
}

std::vector<PlcIOPoint*> DeviceRegistry::getAllIOPoints() {
    return {};
}

void DeviceRegistry::syncToPLC() {
}

void DeviceRegistry::syncFromPLC() {
}

void DeviceRegistry::onStatusChange(StatusCallback callback) {
}

void DeviceRegistry::onValueChange(ValueCallback callback) {
}

void DeviceRegistry::triggerStatusCallbacks(const String& fullName, bool isOnline) {
}

void DeviceRegistry::triggerValueCallbacks(const String& fullName, const PlcValue& value) {
}

String DeviceRegistry::protocolToString(ProtocolType protocol) {
    return "mock";
}

ProtocolType DeviceRegistry::stringToProtocol(const String& protocolStr) {
    return ProtocolType::UNKNOWN;
}

bool DeviceRegistry::parseEndpointName(const String& fullName, String& location, String& protocol, String& device, String& endpoint, String& datatype) {
    return false;
}

String DeviceRegistry::buildEndpointName(const String& location, const String& protocol, const String& device, const String& endpoint, PlcValueType datatype) {
    return "";
}

void DeviceRegistry::clear() {
    endpoints.clear();
    devices.clear();
    ioPoints.clear();
}
