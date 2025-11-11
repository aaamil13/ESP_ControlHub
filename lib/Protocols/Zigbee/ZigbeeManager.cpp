#include "../Protocols/Zigbee/ZigbeeManager.h"
#include "../Core/Logger.h"

ZigbeeManager::ZigbeeManager(MqttManager* mqtt, const String& bridgeTopic)
    : mqtt(mqtt),
      bridgeTopic(bridgeTopic),
      pairingEnabled(false),
      bridgeOnline(false),
      pairingEndTime(0),
      lastDeviceListRequest(0) {
}

ZigbeeManager::~ZigbeeManager() {
}

void ZigbeeManager::begin() {
    LOG_INFO("ZigbeeManager", "Initializing...");

    if (!mqtt) {
        LOG_ERROR("ZigbeeManager", "MqttManager is required");
        return;
    }

    // Subscribe to bridge topics
    subscribeToTopics();

    // Request initial device list
    requestDeviceList();

    LOG_INFO("ZigbeeManager", "Initialized (bridge: " + bridgeTopic + ")");
}

void ZigbeeManager::loop() {
    // Check if pairing should be disabled
    if (pairingEnabled && millis() > pairingEndTime) {
        disablePairing();
    }

    // Periodic device list refresh (every 5 minutes)
    if (millis() - lastDeviceListRequest > 300000) {
        requestDeviceList();
    }

    // Check offline devices (60 second timeout)
    for (auto& pair : devices) {
        ZigbeeDevice& device = pair.second;
        if (device.isOnline && device.lastSeen > 0 && (millis() - device.lastSeen > 60000)) {
            device.isOnline = false;
            LOG_WARN("ZigbeeManager", "Device offline: " + device.deviceId);
        }
    }
}

// ============================================================================
// ProtocolManagerInterface Implementation
// ============================================================================

bool ZigbeeManager::initializeDevice(const String& deviceId, const JsonObject& connectionConfig) {
    LOG_INFO("ZigbeeManager", "Initializing device: " + deviceId);

    ZigbeeDevice device;
    device.deviceId = deviceId;
    device.ieeeAddress = connectionConfig["ieee_address"] | "";
    device.friendlyName = connectionConfig["friendly_name"] | "";
    device.model = connectionConfig["model"] | "";
    device.manufacturer = connectionConfig["manufacturer"] | "";
    device.isOnline = false;
    device.lastSeen = 0;

    // Store device definition for later use
    JsonObject definition = connectionConfig["definition"];
    if (!definition.isNull()) {
        JsonDocument& defDoc = device.deviceDefinition;
        JsonObject root = defDoc.to<JsonObject>();
        for (JsonPair kv : definition) {
            root[kv.key()] = kv.value();
        }
    }

    devices[deviceId] = device;

    // Subscribe to device topic
    String deviceTopic = getDeviceTopic(device.friendlyName);
    mqtt->subscribe(deviceTopic.c_str());

    LOG_INFO("ZigbeeManager", "Device initialized: " + deviceId);
    return true;
}

bool ZigbeeManager::removeDevice(const String& deviceId) {
    auto it = devices.find(deviceId);
    if (it == devices.end()) {
        return false;
    }

    // Unsubscribe from device topic
    String deviceTopic = getDeviceTopic(it->second.friendlyName);
    // Note: MQTT doesn't have unsubscribe in standard PubSubClient

    devices.erase(it);
    LOG_INFO("ZigbeeManager", "Removed device: " + deviceId);
    return true;
}

bool ZigbeeManager::readEndpoint(const String& deviceId, const JsonObject& endpointConfig, PlcValue& value) {
    ZigbeeDevice* device = getDevice(deviceId);
    if (!device) {
        LOG_ERROR("ZigbeeManager", "Device not found: " + deviceId);
        return false;
    }

    // For Zigbee, we rely on MQTT state updates
    // The current value should already be cached in the device
    // This is a read-only operation that returns the last known state

    // Parse endpoint property name
    String property = endpointConfig["property"] | "";
    if (property.isEmpty()) {
        LOG_ERROR("ZigbeeManager", "No property specified in endpoint config");
        return false;
    }

    // Get datatype
    String datatypeStr = endpointConfig["type"] | "bool";
    PlcValueType datatype = zigbeeTypeToPlcType(datatypeStr);
    value = PlcValue(datatype);

    // Note: In a full implementation, we would cache state updates
    // For now, we return a default value and log a warning
    LOG_WARN("ZigbeeManager", "Reading cached value not fully implemented for: " + property);

    return device->isOnline;
}

bool ZigbeeManager::writeEndpoint(const String& deviceId, const JsonObject& endpointConfig, const PlcValue& value) {
    ZigbeeDevice* device = getDevice(deviceId);
    if (!device) {
        LOG_ERROR("ZigbeeManager", "Device not found: " + deviceId);
        return false;
    }

    // Parse endpoint property name
    String property = endpointConfig["property"] | "";
    if (property.isEmpty()) {
        LOG_ERROR("ZigbeeManager", "No property specified in endpoint config");
        return false;
    }

    // Build MQTT payload
    JsonDocument doc;
    JsonObject payload = doc.to<JsonObject>();

    // Add value based on type
    switch (value.type) {
        case PlcValueType::BOOL:
            payload[property] = value.value.bVal ? "ON" : "OFF";
            break;
        case PlcValueType::INT:
            payload[property] = value.value.i16Val;
            break;
        case PlcValueType::REAL:
            payload[property] = value.value.fVal;
            break;
        case PlcValueType::STRING_TYPE:
            payload[property] = value.value.sVal;
            break;
        default:
            LOG_ERROR("ZigbeeManager", "Unsupported value type");
            return false;
    }

    // Serialize and publish
    String payloadStr;
    serializeJson(doc, payloadStr);

    String topic = getDeviceSetTopic(device->friendlyName);
    mqtt->publish(topic.c_str(), payloadStr.c_str());

    LOG_INFO("ZigbeeManager", "Wrote to " + deviceId + "." + property + ": " + payloadStr);

    return true;
}

bool ZigbeeManager::testConnection(const JsonObject& connectionConfig) {
    // For Zigbee, test if bridge is online and MQTT is connected
    if (!mqtt) {
        LOG_ERROR("ZigbeeManager", "MQTT manager not available");
        return false;
    }

    // Check bridge topic matches
    String testBridgeTopic = connectionConfig["bridge_topic"] | bridgeTopic;
    if (testBridgeTopic != bridgeTopic) {
        LOG_ERROR("ZigbeeManager", "Bridge topic mismatch");
        return false;
    }

    LOG_INFO("ZigbeeManager", "Connection test OK (bridge: " + String(bridgeOnline ? "online" : "offline") + ")");
    return bridgeOnline;
}

bool ZigbeeManager::testEndpoint(const String& deviceId, const JsonObject& endpointConfig) {
    ZigbeeDevice* device = getDevice(deviceId);
    if (!device) {
        LOG_ERROR("ZigbeeManager", "Device not found: " + deviceId);
        return false;
    }

    // Check if property exists in endpoint config
    String property = endpointConfig["property"] | "";
    if (property.isEmpty()) {
        LOG_ERROR("ZigbeeManager", "No property specified");
        return false;
    }

    LOG_INFO("ZigbeeManager", "Endpoint test OK for " + deviceId + "." + property);
    return device->isOnline;
}

bool ZigbeeManager::isDeviceOnline(const String& deviceId) {
    ZigbeeDevice* device = getDevice(deviceId);
    if (!device) {
        return false;
    }

    // Consider device offline if not seen in last 60 seconds
    if (device->lastSeen > 0 && (millis() - device->lastSeen > 60000)) {
        device->isOnline = false;
    }

    return device->isOnline && bridgeOnline;
}

// ============================================================================
// Zigbee-Specific Methods
// ============================================================================

void ZigbeeManager::setBridgeTopic(const String& topic) {
    bridgeTopic = topic;
    LOG_INFO("ZigbeeManager", "Bridge topic set to: " + topic);
}

void ZigbeeManager::subscribeToTopics() {
    // Subscribe to bridge state
    String stateTopic = bridgeTopic + "/bridge/state";
    mqtt->subscribe(stateTopic.c_str());
    LOG_INFO("ZigbeeManager", "Subscribed to: " + stateTopic);

    // Subscribe to bridge devices list
    String devicesTopic = bridgeTopic + "/bridge/devices";
    mqtt->subscribe(devicesTopic.c_str());
    LOG_INFO("ZigbeeManager", "Subscribed to: " + devicesTopic);

    // Subscribe to all device updates (wildcard)
    String allDevicesTopic = bridgeTopic + "/#";
    mqtt->subscribe(allDevicesTopic.c_str());
    LOG_INFO("ZigbeeManager", "Subscribed to: " + allDevicesTopic);
}

void ZigbeeManager::enablePairing(uint32_t duration_sec) {
    LOG_INFO("ZigbeeManager", "Enabling pairing for " + String(duration_sec) + " seconds");

    // Send permit_join request via MQTT
    JsonDocument doc;
    doc["value"] = true;
    doc["time"] = duration_sec;

    String payload;
    serializeJson(doc, payload);

    String topic = bridgeTopic + "/bridge/request/permit_join";
    mqtt->publish(topic.c_str(), payload.c_str());

    pairingEnabled = true;
    pairingEndTime = millis() + (duration_sec * 1000);
}

void ZigbeeManager::disablePairing() {
    if (!pairingEnabled) return;

    LOG_INFO("ZigbeeManager", "Disabling pairing");

    // Send permit_join request via MQTT
    JsonDocument doc;
    doc["value"] = false;

    String payload;
    serializeJson(doc, payload);

    String topic = bridgeTopic + "/bridge/request/permit_join";
    mqtt->publish(topic.c_str(), payload.c_str());

    pairingEnabled = false;
}

void ZigbeeManager::requestDeviceList() {
    LOG_INFO("ZigbeeManager", "Requesting device list");

    // Zigbee2MQTT automatically publishes to bridge/devices topic
    // We just need to request it
    String topic = bridgeTopic + "/bridge/request/device/options";
    JsonDocument doc;
    doc["id"] = "all";

    String payload;
    serializeJson(doc, payload);

    mqtt->publish(topic.c_str(), payload.c_str());
    lastDeviceListRequest = millis();
}

void ZigbeeManager::refreshDeviceList() {
    requestDeviceList();
}

void ZigbeeManager::handleMqttMessage(const String& topic, const JsonObject& payload) {
    // Parse topic to determine message type
    if (topic.indexOf("/bridge/state") > 0) {
        handleBridgeState(payload);
    }
    else if (topic.indexOf("/bridge/devices") > 0) {
        // Device list update
        JsonArray devicesArray = payload["devices"].as<JsonArray>();
        if (!devicesArray.isNull()) {
            handleBridgeDevices(devicesArray);
        }
    }
    else if (topic.indexOf("/bridge/") < 0 && topic.indexOf("/set") < 0) {
        // Regular device update (not a bridge message, not a set command)
        // Extract device friendly name from topic
        String deviceName = topic.substring(bridgeTopic.length() + 1);

        handleDeviceUpdate(deviceName, payload);
    }
}

// ============================================================================
// Private Methods
// ============================================================================

ZigbeeDevice* ZigbeeManager::getDevice(const String& deviceId) {
    auto it = devices.find(deviceId);
    if (it != devices.end()) {
        return &it->second;
    }
    return nullptr;
}

void ZigbeeManager::handleBridgeState(const JsonObject& state) {
    const char* stateStr = state["state"];
    if (stateStr) {
        bool wasOnline = bridgeOnline;
        bridgeOnline = (strcmp(stateStr, "online") == 0);

        if (bridgeOnline && !wasOnline) {
            LOG_INFO("ZigbeeManager", "Zigbee2MQTT bridge is ONLINE");
            // Request device list on reconnect
            requestDeviceList();
        } else if (!bridgeOnline && wasOnline) {
            LOG_WARN("ZigbeeManager", "Zigbee2MQTT bridge is OFFLINE");
            // Mark all Zigbee devices as offline
            for (auto& pair : devices) {
                pair.second.isOnline = false;
            }
        }
    }
}

void ZigbeeManager::handleBridgeDevices(const JsonArray& devicesArray) {
    LOG_INFO("ZigbeeManager", "Processing " + String(devicesArray.size()) + " devices from bridge");

    for (JsonObjectConst device : devicesArray) {
        const char* ieeeAddr = device["ieee_address"];
        const char* friendlyName = device["friendly_name"];
        const char* type = device["type"];

        // Skip coordinator
        if (type && strcmp(type, "Coordinator") == 0) {
            continue;
        }

        if (ieeeAddr && friendlyName) {
            JsonObjectConst definition = device["definition"].as<JsonObjectConst>();
            registerDiscoveredDevice(ieeeAddr, friendlyName, definition);
        }
    }
}

void ZigbeeManager::handleDeviceUpdate(const String& deviceName, const JsonObject& state) {
    // Find device by friendly name
    for (auto& pair : devices) {
        ZigbeeDevice& device = pair.second;
        if (device.friendlyName == deviceName) {
            // Update device last seen
            device.lastSeen = millis();
            device.isOnline = true;

            LOG_INFO("ZigbeeManager", "Device update: " + deviceName);

            // Note: In a full implementation, we would cache the state values here
            // for use in readEndpoint()

            break;
        }
    }
}

bool ZigbeeManager::registerDiscoveredDevice(const String& ieeeAddr,
                                              const String& friendlyName,
                                              const JsonObjectConst& deviceDefinition) {
    // Check if device already exists (by IEEE address)
    for (auto& pair : devices) {
        if (pair.second.ieeeAddress == ieeeAddr) {
            // Update existing device
            pair.second.lastSeen = millis();
            pair.second.isOnline = true;
            LOG_INFO("ZigbeeManager", "Updated existing device: " + pair.first);
            return true;
        }
    }

    LOG_INFO("ZigbeeManager", "Discovered new device: " + friendlyName + " (IEEE: " + ieeeAddr + ")");

    // Note: In a full implementation with DeviceConfigManager integration,
    // we would create a JSON device configuration and register it via DeviceConfigManager

    return true;
}

PlcValueType ZigbeeManager::zigbeeTypeToPlcType(const String& zigbeeType) {
    if (zigbeeType == "binary") return PlcValueType::BOOL;
    if (zigbeeType == "numeric") return PlcValueType::REAL;
    if (zigbeeType == "enum") return PlcValueType::INT;
    if (zigbeeType == "text") return PlcValueType::STRING_TYPE;
    return PlcValueType::BOOL; // Default
}

String ZigbeeManager::getDeviceTopic(const String& friendlyName) const {
    return bridgeTopic + "/" + friendlyName;
}

String ZigbeeManager::getDeviceSetTopic(const String& friendlyName) const {
    return bridgeTopic + "/" + friendlyName + "/set";
}
