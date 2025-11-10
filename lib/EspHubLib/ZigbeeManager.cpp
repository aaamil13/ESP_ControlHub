#include "ZigbeeManager.h"
#include "StreamLogger.h"

extern StreamLogger* EspHubLog;

ZigbeeManager::ZigbeeManager(MqttManager* mqtt, const String& defaultLocation)
    : DeviceManager("zigbee", ProtocolType::ZIGBEE),
      mqtt(mqtt),
      bridgeTopic("zigbee2mqtt"),
      defaultLocation(defaultLocation),
      pairingEnabled(false),
      bridgeOnline(false),
      pairingEndTime(0),
      lastDeviceListRequest(0) {
}

ZigbeeManager::~ZigbeeManager() {
}

void ZigbeeManager::begin() {
    EspHubLog->println("ZigbeeManager: Initializing...");

    if (!mqtt) {
        EspHubLog->println("ERROR: ZigbeeManager requires MqttManager");
        return;
    }

    // Subscribe to bridge topics
    subscribeToTopics();

    // Request initial device list
    requestDeviceList();

    EspHubLog->printf("ZigbeeManager initialized (bridge: %s)\n", bridgeTopic.c_str());
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

    // Check offline devices
    checkOfflineDevices(60000); // 60 second timeout
}

void ZigbeeManager::setBridgeTopic(const String& topic) {
    bridgeTopic = topic;
    EspHubLog->printf("ZigbeeManager: Bridge topic set to %s\n", topic.c_str());
}

void ZigbeeManager::setDefaultLocation(const String& location) {
    defaultLocation = location;
}

void ZigbeeManager::subscribeToTopics() {
    // Subscribe to bridge state
    String stateTopic = bridgeTopic + "/bridge/state";
    mqtt->subscribe(stateTopic.c_str());
    EspHubLog->printf("Subscribed to: %s\n", stateTopic.c_str());

    // Subscribe to bridge devices list
    String devicesTopic = bridgeTopic + "/bridge/devices";
    mqtt->subscribe(devicesTopic.c_str());
    EspHubLog->printf("Subscribed to: %s\n", devicesTopic.c_str());

    // Subscribe to all device updates (wildcard)
    String allDevicesTopic = bridgeTopic + "/#";
    mqtt->subscribe(allDevicesTopic.c_str());
    EspHubLog->printf("Subscribed to: %s\n", allDevicesTopic.c_str());
}

void ZigbeeManager::enablePairing(uint32_t duration_sec) {
    EspHubLog->printf("ZigbeeManager: Enabling pairing for %u seconds\n", duration_sec);

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

    EspHubLog->println("ZigbeeManager: Disabling pairing");

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
    EspHubLog->println("ZigbeeManager: Requesting device list");

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
        if (payload["type"] == "devices" || payload.containsKey("devices")) {
            JsonArray devices = payload["devices"].as<JsonArray>();
            if (!devices.isNull()) {
                handleBridgeDevices(devices);
            }
        }
    }
    else if (topic.indexOf("/bridge/") < 0) {
        // Regular device update (not a bridge message)
        // Extract device friendly name from topic
        String deviceName = topic.substring(bridgeTopic.length() + 1);
        // Remove /set suffix if present
        int setPos = deviceName.indexOf("/set");
        if (setPos > 0) {
            deviceName = deviceName.substring(0, setPos);
        }

        handleDeviceUpdate(deviceName, payload);
    }
}

void ZigbeeManager::handleBridgeState(const JsonObject& state) {
    const char* stateStr = state["state"];
    if (stateStr) {
        bool wasOnline = bridgeOnline;
        bridgeOnline = (strcmp(stateStr, "online") == 0);

        if (bridgeOnline && !wasOnline) {
            EspHubLog->println("Zigbee2MQTT bridge is ONLINE");
            // Request device list on reconnect
            requestDeviceList();
        } else if (!bridgeOnline && wasOnline) {
            EspHubLog->println("Zigbee2MQTT bridge is OFFLINE");
            // Mark all Zigbee devices as offline
            auto devices = getAllDevices();
            for (auto* device : devices) {
                updateDeviceStatus(device->deviceId, false);
            }
        }
    }
}

void ZigbeeManager::handleBridgeDevices(const JsonArray& devices) {
    EspHubLog->printf("ZigbeeManager: Processing %d devices from bridge\n", devices.size());

    for (JsonObject device : devices) {
        const char* ieeeAddr = device["ieee_address"];
        const char* friendlyName = device["friendly_name"];
        const char* type = device["type"];

        // Skip coordinator
        if (type && strcmp(type, "Coordinator") == 0) {
            continue;
        }

        if (ieeeAddr && friendlyName) {
            // Use device-specific location if available, otherwise use default
            String location = device["location"] | defaultLocation;
            if (location.isEmpty()) {
                location = "unknown";
            }

            // Register device
            JsonObject definition = device["definition"];
            registerZigbeeDevice(ieeeAddr, friendlyName, location, definition);
        }
    }
}

bool ZigbeeManager::registerZigbeeDevice(const String& ieeeAddr,
                                         const String& friendlyName,
                                         const String& location,
                                         const JsonObject& deviceDefinition) {
    // Build device ID: location.zigbee.friendlyName
    String deviceId = buildDeviceId(location, friendlyName);

    // Check if device already registered
    DeviceStatus* existing = registry->getDevice(deviceId);
    if (existing) {
        // Update last seen
        existing->lastSeen = millis();
        existing->isOnline = true;
        EspHubLog->printf("Updated existing Zigbee device: %s\n", deviceId.c_str());
        return true;
    }

    // Register new device
    JsonDocument configDoc;
    configDoc["ieee_address"] = ieeeAddr;
    configDoc["friendly_name"] = friendlyName;
    configDoc["location"] = location;

    bool registered = registerDevice(deviceId, configDoc.as<JsonObject>());
    if (!registered) {
        EspHubLog->printf("ERROR: Failed to register Zigbee device %s\n", deviceId.c_str());
        return false;
    }

    // Register endpoints based on device definition
    if (!deviceDefinition.isNull()) {
        registerDeviceEndpoints(ieeeAddr, friendlyName, location, deviceDefinition);
    }

    EspHubLog->printf("Registered new Zigbee device: %s (IEEE: %s)\n",
                     deviceId.c_str(), ieeeAddr.c_str());

    return true;
}

void ZigbeeManager::registerDeviceEndpoints(const String& ieeeAddr,
                                            const String& friendlyName,
                                            const String& location,
                                            const JsonObject& definition) {
    String deviceId = buildDeviceId(location, friendlyName);

    // Parse exposes array from device definition
    JsonArray exposes = definition["exposes"];
    if (exposes.isNull()) {
        EspHubLog->printf("WARNING: No exposes found for device %s\n", deviceId.c_str());
        return;
    }

    for (JsonObject expose : exposes) {
        createEndpointFromExpose(deviceId, location, expose);
    }
}

void ZigbeeManager::createEndpointFromExpose(const String& deviceId,
                                             const String& location,
                                             const JsonObject& expose) {
    const char* type = expose["type"];
    const char* name = expose["name"];
    const char* property = expose["property"];
    const char* access = expose["access"];

    // Skip features (nested exposes)
    if (type && strcmp(type, "composite") == 0) {
        JsonArray features = expose["features"];
        for (JsonObject feature : features) {
            createEndpointFromExpose(deviceId, location, feature);
        }
        return;
    }

    // Determine property name
    String endpointName = property ? String(property) : (name ? String(name) : "unknown");
    if (endpointName == "unknown") {
        return; // Skip unnamed properties
    }

    // Convert Zigbee type to PLC type
    PlcValueType datatype = zigbeeTypeToPlcType(type);

    // Build full endpoint name: location.zigbee.device.endpoint.datatype
    String fullName = location + ".zigbee." + deviceId.substring(deviceId.lastIndexOf('.') + 1) +
                      "." + endpointName + "." +
                      String(datatype == PlcValueType::BOOL ? "bool" :
                             datatype == PlcValueType::INT ? "int" :
                             datatype == PlcValueType::REAL ? "real" : "string");

    // Create endpoint
    Endpoint endpoint;
    endpoint.fullName = fullName;
    endpoint.location = location;
    endpoint.protocol = ProtocolType::ZIGBEE;
    endpoint.deviceId = deviceId;
    endpoint.endpoint = endpointName;
    endpoint.datatype = datatype;
    endpoint.isOnline = true;
    endpoint.lastSeen = millis();
    endpoint.isWritable = (access && (strstr(access, "w") != nullptr || strstr(access, "2") != nullptr));
    endpoint.mqttTopic = bridgeTopic + "/" + deviceId.substring(deviceId.lastIndexOf('.') + 1);
    endpoint.currentValue.type = datatype;

    // Register endpoint
    bool registered = registerEndpointHelper(endpoint);
    if (registered) {
        EspHubLog->printf("  Registered endpoint: %s (%s%s)\n",
                         fullName.c_str(),
                         endpoint.isWritable ? "RW" : "RO",
                         endpoint.isOnline ? " ONLINE" : " OFFLINE");
    }
}

PlcValueType ZigbeeManager::zigbeeTypeToPlcType(const String& zigbeeType) {
    if (zigbeeType == "binary") return PlcValueType::BOOL;
    if (zigbeeType == "numeric") return PlcValueType::REAL;
    if (zigbeeType == "enum") return PlcValueType::INT;
    if (zigbeeType == "text") return PlcValueType::STRING_TYPE;
    return PlcValueType::BOOL; // Default
}

void ZigbeeManager::handleDeviceUpdate(const String& deviceName, const JsonObject& state) {
    // Find device by friendly name
    String deviceIdPattern = ".zigbee." + deviceName;
    auto allDevices = getAllDevices();

    for (auto* device : allDevices) {
        if (device->deviceId.indexOf(deviceIdPattern) > 0) {
            // Update device last seen
            device->lastSeen = millis();
            updateDeviceStatus(device->deviceId, true);

            // Update endpoint values
            auto endpoints = registry->getEndpointsByDevice(device->deviceId);
            for (auto* endpoint : endpoints) {
                // Extract property name from endpoint
                String property = endpoint->endpoint;

                // Check if state contains this property
                if (state.containsKey(property)) {
                    PlcValue newValue(endpoint->datatype);

                    // Parse value based on type
                    switch (endpoint->datatype) {
                        case PlcValueType::BOOL:
                            if (state[property].is<bool>()) {
                                newValue.value.bVal = state[property].as<bool>();
                            } else if (state[property].is<const char*>()) {
                                const char* val = state[property];
                                newValue.value.bVal = (strcmp(val, "ON") == 0 || strcmp(val, "true") == 0);
                            }
                            break;

                        case PlcValueType::INT:
                            newValue.value.i16Val = state[property].as<int16_t>();
                            break;

                        case PlcValueType::REAL:
                            newValue.value.fVal = state[property].as<float>();
                            break;

                        case PlcValueType::STRING_TYPE:
                            strncpy(newValue.value.sVal, state[property].as<const char*>(), 63);
                            newValue.value.sVal[63] = '\0';
                            break;

                        default:
                            continue;
                    }

                    // Update endpoint value in registry
                    registry->updateEndpointValue(endpoint->fullName, newValue);
                }
            }

            break; // Found the device, stop searching
        }
    }
}
