#ifndef WIFI_DEVICE_MANAGER_H
#define WIFI_DEVICE_MANAGER_H

#include "DeviceManager.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <map>
#include <functional>

/**
 * WiFiDeviceManager - Generic manager for WiFi-connected devices
 *
 * Supports JSON-based device configuration allowing flexible integration
 * with various WiFi devices without hardcoding device types.
 *
 * Supported Connection Types:
 * - HTTP/HTTPS REST APIs
 * - MQTT (via MqttManager integration)
 * - WebSocket (future)
 *
 * Supported Devices (via JSON config):
 * - Tasmota devices
 * - ESPHome devices
 * - Shelly devices
 * - Sonoff devices
 * - Custom HTTP API devices
 *
 * Hierarchical naming:
 *   {location}.wifi.{device_id}.{endpoint}.{datatype}
 *   Example: kitchen.wifi.sonoff_1.relay.bool
 */

// Forward declarations
class MqttManager;

// Connection types
enum class ConnectionType {
    HTTP,
    HTTPS,
    MQTT
};

// HTTP method
enum class HttpMethod {
    GET,
    POST,
    PUT,
    DELETE
};

// Endpoint configuration
struct EndpointConfig {
    String name;
    PlcValueType datatype;
    String access;            // "r", "w", or "rw"
    uint32_t pollingInterval; // milliseconds

    // Read configuration
    bool hasRead;
    HttpMethod readMethod;
    String readPath;
    String readValuePath;     // JSON path for value extraction
    std::map<String, bool> readValueMap; // Value mapping (e.g., "ON" -> true)

    // Write configuration
    bool hasWrite;
    HttpMethod writeMethod;
    String writePath;
    String writeBodyTemplate;
    std::map<bool, String> writeValueFormat; // Format for writing (true -> "On")

    // MQTT configuration
    String mqttReadTopic;
    String mqttWriteTopic;

    // State
    PlcValue currentValue;
    unsigned long lastRead;
};

// Device configuration
struct WiFiDeviceConfig {
    String deviceId;
    String friendlyName;
    String location;

    // Connection
    ConnectionType connectionType;
    String host;
    int port;
    bool useSsl;
    String authUsername;
    String authPassword;

    // MQTT
    String mqttTopicPrefix;

    // Endpoints
    std::vector<EndpointConfig> endpoints;

    // Metadata
    String manufacturer;
    String model;
    String firmware;

    // State
    bool isOnline;
    unsigned long lastSeen;
};

class WiFiDeviceManager : public DeviceManager {
public:
    WiFiDeviceManager(MqttManager* mqttManager = nullptr);
    ~WiFiDeviceManager();

    // DeviceManager interface
    void begin() override;
    void loop() override;

    // Device configuration
    bool loadDeviceFromJson(const JsonObject& config);
    bool loadDeviceFromFile(const String& filepath);
    bool saveDeviceToFile(const String& deviceId, const String& filepath);
    bool removeDevice(const String& deviceId);

    // Device access
    WiFiDeviceConfig* getDevice(const String& deviceId);
    std::vector<String> getAllDeviceIds() const;

    // Endpoint operations
    bool readEndpoint(const String& deviceId, const String& endpointName);
    bool writeEndpoint(const String& deviceId, const String& endpointName, const PlcValue& value);

    // Polling configuration
    void setGlobalPollingInterval(uint32_t ms) { globalPollingInterval = ms; }
    uint32_t getGlobalPollingInterval() const { return globalPollingInterval; }

    // Testing
    bool testDeviceConnection(const WiFiDeviceConfig& config);
    bool testEndpoint(const WiFiDeviceConfig& config, const String& endpointName);

private:
    MqttManager* mqttManager;
    std::map<String, WiFiDeviceConfig> devices;
    HTTPClient httpClient;
    uint32_t globalPollingInterval;

    // HTTP operations
    bool httpRequest(const WiFiDeviceConfig& device, HttpMethod method,
                    const String& path, const String& body, String& response);
    bool extractJsonValue(const String& json, const String& path, PlcValue& value, PlcValueType type);
    String formatWriteValue(const PlcValue& value, const EndpointConfig& endpoint);

    // Endpoint management
    void pollEndpoints();
    void registerEndpointInRegistry(const String& deviceId, const EndpointConfig& endpointConfig);
    void updateEndpointValue(const String& deviceId, const String& endpointName, const PlcValue& value);

    // Parsing helpers
    ConnectionType parseConnectionType(const String& type);
    HttpMethod parseHttpMethod(const String& method);
    PlcValueType parseDatatype(const String& type);
};

#endif // WIFI_DEVICE_MANAGER_H
