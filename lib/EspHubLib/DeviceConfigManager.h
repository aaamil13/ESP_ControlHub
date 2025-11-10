#ifndef DEVICE_CONFIG_MANAGER_H
#define DEVICE_CONFIG_MANAGER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <map>
#include <vector>
#include "ProtocolManagerInterface.h"
#include "../PlcCore/PlcMemory.h"

/**
 * DeviceConfigManager - Central coordinator for all devices
 *
 * Responsibilities:
 * - Load/Save device configurations from/to JSON files
 * - Maintain registry of all device configs (all protocols)
 * - Route IO operations to appropriate protocol managers
 * - Provide unified device management API
 * - Handle device lifecycle (create, update, delete)
 *
 * This class does NOT:
 * - Implement protocol-specific communication (done by protocol managers)
 * - Store runtime IO values (done by DeviceRegistry)
 */
class DeviceConfigManager {
public:
    DeviceConfigManager();
    ~DeviceConfigManager();

    // Initialization
    void begin();

    // Protocol manager registration
    void registerProtocolManager(const String& protocolName, ProtocolManagerInterface* manager);
    ProtocolManagerInterface* getProtocolManager(const String& protocolName);

    // Device configuration loading
    bool loadDevice(const JsonObject& config);
    bool loadDeviceFromFile(const String& filepath);
    bool loadAllDevices(); // Load all from /config/devices/
    int getLoadedDeviceCount() const { return deviceConfigs.size(); }

    // Device configuration saving
    bool saveDevice(const String& deviceId);
    bool saveDeviceToFile(const String& deviceId, const String& filepath);
    bool saveAllDevices();

    // Device lifecycle
    bool createDevice(const JsonObject& config);
    bool updateDevice(const String& deviceId, const JsonObject& config);
    bool deleteDevice(const String& deviceId);

    // Device access
    bool hasDevice(const String& deviceId) const;
    JsonDocument getDeviceConfig(const String& deviceId);
    String getDeviceProtocol(const String& deviceId);
    String getDeviceLocation(const String& deviceId);
    String getDeviceFriendlyName(const String& deviceId);

    // Device queries
    std::vector<String> getAllDeviceIds() const;
    std::vector<String> getDevicesByProtocol(const String& protocol) const;
    std::vector<String> getDevicesByLocation(const String& location) const;
    std::vector<String> getDevicesByTag(const String& tag) const;

    // Endpoint queries
    std::vector<String> getDeviceEndpoints(const String& deviceId) const;
    JsonObject getEndpointConfig(const String& deviceId, const String& endpointName);

    // IO operations (delegate to protocol managers)
    bool readEndpoint(const String& deviceId, const String& endpointName, PlcValue& value);
    bool writeEndpoint(const String& deviceId, const String& endpointName, const PlcValue& value);
    bool readAllEndpoints(const String& deviceId); // Read all readable endpoints

    // Testing
    bool testDeviceConnection(const String& deviceId);
    bool testDeviceConnection(const JsonObject& config); // Test without saving
    bool testEndpoint(const String& deviceId, const String& endpointName);

    // Device status
    bool isDeviceOnline(const String& deviceId);
    void updateDeviceStatus(const String& deviceId, bool online);

    // Template management
    bool loadTemplate(const String& templateId, JsonDocument& templateDoc);
    std::vector<String> getAvailableTemplates();

    // Statistics
    struct DeviceStats {
        int totalDevices;
        int onlineDevices;
        int offlineDevices;
        std::map<String, int> devicesByProtocol;
        std::map<String, int> devicesByLocation;
    };
    DeviceStats getStatistics();

private:
    // Device storage
    std::map<String, JsonDocument> deviceConfigs;  // deviceId -> full config
    std::map<String, ProtocolManagerInterface*> protocolManagers; // protocol -> manager

    // Helper methods
    bool validateDeviceConfig(const JsonObject& config);
    String generateDeviceFilename(const String& deviceId);
    bool initializeDeviceConnection(const String& deviceId, const JsonObject& config);
    JsonObject findEndpointInConfig(const JsonDocument& deviceConfig, const String& endpointName);

    // File operations
    bool ensureConfigDirectory();
    String getDeviceFilePath(const String& deviceId);
};

#endif // DEVICE_CONFIG_MANAGER_H
