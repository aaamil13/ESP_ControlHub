#include "../Devices/DeviceRegistry.h"
#include "../Core/StreamLogger.h"

extern StreamLogger* EspHubLog;

// Singleton instance
DeviceRegistry& DeviceRegistry::getInstance() {
    static DeviceRegistry instance;
    return instance;
}

DeviceRegistry::DeviceRegistry() : plcMemory(nullptr) {
    EspHubLog->println("DeviceRegistry initialized");
}

DeviceRegistry::~DeviceRegistry() {
}

// ==================== Endpoint Management ====================

bool DeviceRegistry::registerEndpoint(const Endpoint& endpoint) {
    if (endpoint.fullName.length() == 0) {
        EspHubLog->println("ERROR: Cannot register endpoint with empty name");
        return false;
    }

    endpoints[endpoint.fullName] = endpoint;
    EspHubLog->printf("Registered endpoint: %s (writable: %d)\n",
                     endpoint.fullName.c_str(),
                     endpoint.isWritable);

    // Add endpoint to device's endpoint list
    if (endpoint.deviceId.length() > 0) {
        String deviceFullId = endpoint.location + "." +
                             protocolToString(endpoint.protocol) + "." +
                             endpoint.deviceId;

        auto deviceIt = devices.find(deviceFullId);
        if (deviceIt != devices.end()) {
            bool found = false;
            for (const auto& ep : deviceIt->second.endpoints) {
                if (ep == endpoint.fullName) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                deviceIt->second.endpoints.push_back(endpoint.fullName);
            }
        }
    }

    return true;
}

bool DeviceRegistry::removeEndpoint(const String& fullName) {
    auto it = endpoints.find(fullName);
    if (it != endpoints.end()) {
        endpoints.erase(it);
        EspHubLog->printf("Removed endpoint: %s\n", fullName.c_str());
        return true;
    }
    return false;
}

Endpoint* DeviceRegistry::getEndpoint(const String& fullName) {
    auto it = endpoints.find(fullName);
    if (it != endpoints.end()) {
        return &(it->second);
    }
    return nullptr;
}

std::vector<Endpoint*> DeviceRegistry::getAllEndpoints() {
    std::vector<Endpoint*> result;
    result.reserve(endpoints.size());
    for (auto& pair : endpoints) {
        result.push_back(&pair.second);
    }
    return result;
}

std::vector<Endpoint*> DeviceRegistry::getEndpointsByProtocol(ProtocolType protocol) {
    std::vector<Endpoint*> result;
    for (auto& pair : endpoints) {
        if (pair.second.protocol == protocol) {
            result.push_back(&pair.second);
        }
    }
    return result;
}

std::vector<Endpoint*> DeviceRegistry::getEndpointsByLocation(const String& location) {
    std::vector<Endpoint*> result;
    for (auto& pair : endpoints) {
        if (pair.second.location == location) {
            result.push_back(&pair.second);
        }
    }
    return result;
}

std::vector<Endpoint*> DeviceRegistry::getEndpointsByDevice(const String& deviceId) {
    std::vector<Endpoint*> result;
    for (auto& pair : endpoints) {
        if (pair.second.deviceId == deviceId) {
            result.push_back(&pair.second);
        }
    }
    return result;
}

// ==================== Status Management ====================

void DeviceRegistry::updateEndpointStatus(const String& fullName, bool isOnline) {
    auto it = endpoints.find(fullName);
    if (it != endpoints.end()) {
        bool statusChanged = (it->second.isOnline != isOnline);
        it->second.isOnline = isOnline;
        it->second.lastSeen = millis();

        if (statusChanged) {
            EspHubLog->printf("Endpoint %s status: %s\n",
                            fullName.c_str(),
                            isOnline ? "ONLINE" : "OFFLINE");
            triggerStatusCallbacks(fullName, isOnline);
        }
    }
}

void DeviceRegistry::updateEndpointValue(const String& fullName, const PlcValue& value) {
    auto it = endpoints.find(fullName);
    if (it != endpoints.end()) {
        it->second.currentValue = value;
        it->second.lastSeen = millis();
        triggerValueCallbacks(fullName, value);
    }
}

void DeviceRegistry::checkOfflineDevices(uint32_t timeout_ms) {
    unsigned long now = millis();
    for (auto& pair : endpoints) {
        if (pair.second.isOnline && (now - pair.second.lastSeen > timeout_ms)) {
            updateEndpointStatus(pair.first, false);
        }
    }
}

// ==================== Device Management ====================

bool DeviceRegistry::registerDevice(const DeviceStatus& device) {
    if (device.deviceId.length() == 0) {
        EspHubLog->println("ERROR: Cannot register device with empty ID");
        return false;
    }

    devices[device.deviceId] = device;
    EspHubLog->printf("Registered device: %s\n", device.deviceId.c_str());
    return true;
}

DeviceStatus* DeviceRegistry::getDevice(const String& deviceId) {
    auto it = devices.find(deviceId);
    if (it != devices.end()) {
        return &(it->second);
    }
    return nullptr;
}

std::vector<DeviceStatus*> DeviceRegistry::getAllDevices() {
    std::vector<DeviceStatus*> result;
    result.reserve(devices.size());
    for (auto& pair : devices) {
        result.push_back(&pair.second);
    }
    return result;
}

void DeviceRegistry::updateDeviceStatus(const String& deviceId, bool isOnline) {
    auto it = devices.find(deviceId);
    if (it != devices.end()) {
        it->second.isOnline = isOnline;
        it->second.lastSeen = millis();
    }
}

// ==================== PLC IO Points ====================

bool DeviceRegistry::registerIOPoint(const PlcIOPoint& ioPoint) {
    if (ioPoint.plcVarName.length() == 0 || ioPoint.mappedEndpoint.length() == 0) {
        EspHubLog->println("ERROR: Cannot register IO point with empty names");
        return false;
    }

    // Verify endpoint exists
    if (!getEndpoint(ioPoint.mappedEndpoint)) {
        EspHubLog->printf("WARNING: Endpoint %s not found for IO point %s\n",
                         ioPoint.mappedEndpoint.c_str(),
                         ioPoint.plcVarName.c_str());
    }

    ioPoints[ioPoint.plcVarName] = ioPoint;
    EspHubLog->printf("Registered IO point: %s -> %s (%s)\n",
                     ioPoint.plcVarName.c_str(),
                     ioPoint.mappedEndpoint.c_str(),
                     ioPoint.direction == IODirection::IO_INPUT ? "INPUT" : "OUTPUT");
    return true;
}

bool DeviceRegistry::unregisterIOPoint(const String& plcVarName) {
    auto it = ioPoints.find(plcVarName);
    if (it != ioPoints.end()) {
        ioPoints.erase(it);
        EspHubLog->printf("Unregistered IO point: %s\n", plcVarName.c_str());
        return true;
    }
    return false;
}

PlcIOPoint* DeviceRegistry::getIOPoint(const String& plcVarName) {
    auto it = ioPoints.find(plcVarName);
    if (it != ioPoints.end()) {
        return &(it->second);
    }
    return nullptr;
}

std::vector<PlcIOPoint*> DeviceRegistry::getAllIOPoints() {
    std::vector<PlcIOPoint*> result;
    result.reserve(ioPoints.size());
    for (auto& pair : ioPoints) {
        result.push_back(&pair.second);
    }
    return result;
}

// ==================== PLC Sync ====================

void DeviceRegistry::syncToPLC() {
    if (!plcMemory) return;

    // Sync INPUT endpoints to PLC variables
    for (auto& ioPointPair : ioPoints) {
        PlcIOPoint& ioPoint = ioPointPair.second;
        if (ioPoint.direction == IODirection::IO_INPUT && ioPoint.autoSync) {
            Endpoint* endpoint = getEndpoint(ioPoint.mappedEndpoint);
            if (endpoint && endpoint->isOnline) {
                // Copy value from endpoint to PLC
                // Note: Actual implementation depends on PlcMemory setValue variants
                // This is a placeholder
                EspHubLog->printf("Sync INPUT %s <- %s\n",
                                 ioPoint.plcVarName.c_str(),
                                 ioPoint.mappedEndpoint.c_str());
            }
        }
    }
}

void DeviceRegistry::syncFromPLC() {
    if (!plcMemory) return;

    // Sync OUTPUT PLC variables to endpoints
    for (auto& ioPointPair : ioPoints) {
        PlcIOPoint& ioPoint = ioPointPair.second;
        if (ioPoint.direction == IODirection::IO_OUTPUT && ioPoint.autoSync) {
            Endpoint* endpoint = getEndpoint(ioPoint.mappedEndpoint);
            if (endpoint && endpoint->isOnline) {
                // Copy value from PLC to endpoint
                // Note: Actual implementation depends on PlcMemory getValue variants
                // This is a placeholder
                EspHubLog->printf("Sync OUTPUT %s -> %s\n",
                                 ioPoint.plcVarName.c_str(),
                                 ioPoint.mappedEndpoint.c_str());
            }
        }
    }
}

// ==================== Callbacks ====================

void DeviceRegistry::onStatusChange(StatusCallback callback) {
    statusCallbacks.push_back(callback);
}

void DeviceRegistry::onValueChange(ValueCallback callback) {
    valueCallbacks.push_back(callback);
}

void DeviceRegistry::triggerStatusCallbacks(const String& fullName, bool isOnline) {
    for (auto& callback : statusCallbacks) {
        callback(fullName, isOnline);
    }
}

void DeviceRegistry::triggerValueCallbacks(const String& fullName, const PlcValue& value) {
    for (auto& callback : valueCallbacks) {
        callback(fullName, value);
    }
}

// ==================== Utility Functions ====================

String DeviceRegistry::protocolToString(ProtocolType protocol) {
    switch (protocol) {
        case ProtocolType::MESH: return "mesh";
        case ProtocolType::ZIGBEE: return "zigbee";
        case ProtocolType::BLE: return "ble";
        case ProtocolType::WIFI: return "wifi";
        case ProtocolType::MODBUS: return "modbus";
        default: return "unknown";
    }
}

ProtocolType DeviceRegistry::stringToProtocol(const String& protocolStr) {
    String lower = protocolStr;
    lower.toLowerCase();
    if (lower == "mesh") return ProtocolType::MESH;
    if (lower == "zigbee") return ProtocolType::ZIGBEE;
    if (lower == "ble") return ProtocolType::BLE;
    if (lower == "wifi") return ProtocolType::WIFI;
    if (lower == "modbus") return ProtocolType::MODBUS;
    return ProtocolType::UNKNOWN;
}

bool DeviceRegistry::parseEndpointName(const String& fullName,
                                       String& location,
                                       String& protocol,
                                       String& device,
                                       String& endpoint,
                                       String& datatype) {
    // Format: location.protocol.device.endpoint.datatype
    int firstDot = fullName.indexOf('.');
    if (firstDot == -1) return false;

    int secondDot = fullName.indexOf('.', firstDot + 1);
    if (secondDot == -1) return false;

    int thirdDot = fullName.indexOf('.', secondDot + 1);
    if (thirdDot == -1) return false;

    int fourthDot = fullName.indexOf('.', thirdDot + 1);
    if (fourthDot == -1) return false;

    location = fullName.substring(0, firstDot);
    protocol = fullName.substring(firstDot + 1, secondDot);
    device = fullName.substring(secondDot + 1, thirdDot);
    endpoint = fullName.substring(thirdDot + 1, fourthDot);
    datatype = fullName.substring(fourthDot + 1);

    return true;
}

String DeviceRegistry::buildEndpointName(const String& location,
                                        const String& protocol,
                                        const String& device,
                                        const String& endpoint,
                                        PlcValueType datatype) {
    String datatypeStr;
    switch (datatype) {
        case PlcValueType::BOOL: datatypeStr = "bool"; break;
        case PlcValueType::BYTE: datatypeStr = "byte"; break;
        case PlcValueType::INT: datatypeStr = "int"; break;
        case PlcValueType::DINT: datatypeStr = "dint"; break;
        case PlcValueType::REAL: datatypeStr = "float"; break;
        case PlcValueType::STRING_TYPE: datatypeStr = "string"; break;
        default: datatypeStr = "unknown"; break;
    }

    return location + "." + protocol + "." + device + "." + endpoint + "." + datatypeStr;
}

void DeviceRegistry::clear() {
    endpoints.clear();
    devices.clear();
    ioPoints.clear();
    statusCallbacks.clear();
    valueCallbacks.clear();
    EspHubLog->println("DeviceRegistry cleared");
}
