#ifndef ZIGBEE_MANAGER_H
#define ZIGBEE_MANAGER_H

#include "../Protocols/ProtocolManagerInterface.h"
#include "../Protocols/Mqtt/MqttManager.h"
#include <ArduinoJson.h>
#include <functional>
#include <map>

/**
 * ZigbeeManager - Protocol manager for Zigbee devices via Zigbee2MQTT bridge
 *
 * Implements the ProtocolManagerInterface to provide transport layer
 * for Zigbee devices via Zigbee2MQTT bridge.
 *
 * Responsibilities:
 * - Communication with Zigbee2MQTT bridge via MQTT
 * - Device discovery and pairing control
 * - Reading/writing device endpoints
 *
 * Does NOT:
 * - Store device configurations (done by DeviceConfigManager)
 * - Register IO points in DeviceRegistry (done by DeviceConfigManager)
 *
 * This manager integrates with Zigbee2MQTT (https://www.zigbee2mqtt.io/)
 * which acts as a bridge between Zigbee devices and MQTT.
 *
 * MQTT Topics:
 * - zigbee2mqtt/bridge/devices - List of all paired devices
 * - zigbee2mqtt/bridge/state - Bridge online/offline status
 * - zigbee2mqtt/[device_friendly_name] - Device state updates
 * - zigbee2mqtt/[device_friendly_name]/set - Send commands to device
 * - zigbee2mqtt/bridge/request/permit_join - Enable/disable pairing
 */

struct ZigbeeDevice {
    String deviceId;
    String ieeeAddress;
    String friendlyName;
    String model;
    String manufacturer;
    bool isOnline;
    unsigned long lastSeen;
    JsonDocument deviceDefinition;  // Cached device definition from Zigbee2MQTT
};

class ZigbeeManager : public ProtocolManagerInterface {
public:
    ZigbeeManager(MqttManager* mqtt, const String& bridgeTopic = "zigbee2mqtt");
    ~ZigbeeManager();

    // ProtocolManagerInterface implementation
    void begin() override;
    void loop() override;

    bool initializeDevice(const String& deviceId, const JsonObject& connectionConfig) override;
    bool removeDevice(const String& deviceId) override;

    bool readEndpoint(const String& deviceId, const JsonObject& endpointConfig, PlcValue& value) override;
    bool writeEndpoint(const String& deviceId, const JsonObject& endpointConfig, const PlcValue& value) override;

    bool testConnection(const JsonObject& connectionConfig) override;
    bool testEndpoint(const String& deviceId, const JsonObject& endpointConfig) override;

    String getProtocolName() const override { return "zigbee"; }
    bool isDeviceOnline(const String& deviceId) override;

    // Zigbee-specific configuration
    void setBridgeTopic(const String& topic);
    String getBridgeTopic() const { return bridgeTopic; }

    // Pairing control
    void enablePairing(uint32_t duration_sec = 60);
    void disablePairing();
    bool isPairingEnabled() const { return pairingEnabled; }

    // Device discovery
    void requestDeviceList();
    void refreshDeviceList();

    // MQTT callback handler
    void handleMqttMessage(const String& topic, const JsonObject& payload);

    // Get bridge status
    bool isBridgeOnline() const { return bridgeOnline; }

private:
    MqttManager* mqtt;
    String bridgeTopic;          // Default: "zigbee2mqtt"
    bool pairingEnabled;
    bool bridgeOnline;
    unsigned long pairingEndTime;
    unsigned long lastDeviceListRequest;

    // Device storage
    std::map<String, ZigbeeDevice> devices;  // deviceId -> device

    // Internal device management
    ZigbeeDevice* getDevice(const String& deviceId);

    // MQTT topic handlers
    void handleBridgeDevices(const JsonArray& devices);
    void handleBridgeState(const JsonObject& state);
    void handleDeviceUpdate(const String& deviceName, const JsonObject& state);

    // Device registration from discovery
    bool registerDiscoveredDevice(const String& ieeeAddr,
                                   const String& friendlyName,
                                   const JsonObjectConst& deviceDefinition);

    // Parse Zigbee type to PlcValueType
    PlcValueType zigbeeTypeToPlcType(const String& zigbeeType);

    // Subscribe to necessary MQTT topics
    void subscribeToTopics();

    // Build MQTT topic for device
    String getDeviceTopic(const String& friendlyName) const;
    String getDeviceSetTopic(const String& friendlyName) const;
};

#endif // ZIGBEE_MANAGER_H
