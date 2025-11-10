#ifndef ZIGBEE_MANAGER_H
#define ZIGBEE_MANAGER_H

#include "DeviceManager.h"
#include "MqttManager.h"
#include <ArduinoJson.h>
#include <functional>

/**
 * ZigbeeManager - Manages Zigbee devices via Zigbee2MQTT bridge
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
 *
 * Hierarchical naming:
 *   {location}.zigbee.{device_name}.{endpoint}.{datatype}
 *   Example: kitchen.zigbee.temp_sensor.temperature.real
 */
class ZigbeeManager : public DeviceManager {
public:
    ZigbeeManager(MqttManager* mqtt, const String& defaultLocation = "");
    ~ZigbeeManager();

    // DeviceManager interface
    void begin() override;
    void loop() override;

    // Configuration
    void setBridgeTopic(const String& topic);
    void setDefaultLocation(const String& location);
    String getDefaultLocation() const { return defaultLocation; }

    // Pairing control
    void enablePairing(uint32_t duration_sec = 60);
    void disablePairing();
    bool isPairingEnabled() const { return pairingEnabled; }

    // Device discovery
    void requestDeviceList();
    void refreshDeviceList();

    // Manual device registration (for pre-configured devices)
    bool registerZigbeeDevice(const String& ieeeAddr,
                             const String& friendlyName,
                             const String& location,
                             const JsonObject& deviceDefinition);

    // MQTT callback handler
    void handleMqttMessage(const String& topic, const JsonObject& payload);

    // Get bridge status
    bool isBridgeOnline() const { return bridgeOnline; }
    String getBridgeTopic() const { return bridgeTopic; }

private:
    MqttManager* mqtt;
    String bridgeTopic;          // Default: "zigbee2mqtt"
    String defaultLocation;      // Default location for auto-discovered devices
    bool pairingEnabled;
    bool bridgeOnline;
    unsigned long pairingEndTime;
    unsigned long lastDeviceListRequest;

    // MQTT topic handlers
    void handleBridgeDevices(const JsonArray& devices);
    void handleBridgeState(const JsonObject& state);
    void handleDeviceUpdate(const String& deviceName, const JsonObject& state);

    // Device endpoint registration
    void registerDeviceEndpoints(const String& ieeeAddr,
                                 const String& friendlyName,
                                 const String& location,
                                 const JsonObject& definition);

    // Create endpoint from Zigbee expose definition
    void createEndpointFromExpose(const String& deviceId,
                                  const String& location,
                                  const JsonObject& expose);

    // Parse Zigbee type to PlcValueType
    PlcValueType zigbeeTypeToPlcType(const String& zigbeeType);

    // Subscribe to necessary MQTT topics
    void subscribeToTopics();
};

#endif // ZIGBEE_MANAGER_H
