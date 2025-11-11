#ifndef WIFI_DEVICE_MANAGER_H
#define WIFI_DEVICE_MANAGER_H

#include "ProtocolManagerInterface.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <map>
#include <functional>

/**
 * WiFiDeviceManager - Protocol manager for WiFi-connected devices
 *
 * Implements the ProtocolManagerInterface to provide transport layer
 * for WiFi/HTTP/HTTPS device communication.
 *
 * Responsibilities:
 * - HTTP/HTTPS request handling
 * - MQTT integration (via MqttManager)
 * - Connection management
 * - Value parsing and formatting
 *
 * Does NOT:
 * - Store device configurations (done by DeviceConfigManager)
 * - Register IO points in DeviceRegistry (done by DeviceConfigManager)
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

class WiFiDeviceManager : public ProtocolManagerInterface {
public:
    WiFiDeviceManager(MqttManager* mqttManager = nullptr);
    ~WiFiDeviceManager();

    // ProtocolManagerInterface implementation
    void begin() override;
    void loop() override;

    bool initializeDevice(const String& deviceId, const JsonObject& connectionConfig) override;
    bool removeDevice(const String& deviceId) override;

    bool readEndpoint(const String& deviceId, const JsonObject& endpointConfig, PlcValue& value) override;
    bool writeEndpoint(const String& deviceId, const JsonObject& endpointConfig, const PlcValue& value) override;

    bool testConnection(const JsonObject& connectionConfig) override;
    bool testEndpoint(const String& deviceId, const JsonObject& endpointConfig) override;

    String getProtocolName() const override { return "wifi"; }
    bool isDeviceOnline(const String& deviceId) override;

    // Polling configuration
    void setGlobalPollingInterval(uint32_t ms) { globalPollingInterval = ms; }
    uint32_t getGlobalPollingInterval() const { return globalPollingInterval; }

private:
    MqttManager* mqttManager;
    std::map<String, WiFiDeviceConfig> devices;
    HTTPClient httpClient;
    uint32_t globalPollingInterval;

    // Internal device management
    WiFiDeviceConfig* getDevice(const String& deviceId);
    EndpointConfig* getEndpointConfig(const String& deviceId, const String& endpointName);

    // HTTP operations
    bool httpRequest(const String& host, int port, bool useSsl,
                    const String& authUsername, const String& authPassword,
                    HttpMethod method, const String& path, const String& body, String& response);
    bool extractJsonValue(const String& json, const String& path, PlcValue& value, PlcValueType type);
    String formatWriteValue(const PlcValue& value, PlcValueType datatype, const JsonObject& endpointConfig);

    // Endpoint management
    void pollEndpoints();

    // Parsing helpers
    ConnectionType parseConnectionType(const String& type);
    HttpMethod parseHttpMethod(const String& method);
    PlcValueType parseDatatype(const String& type);

    // Configuration parsing from JSON
    bool parseEndpointConfig(const JsonObject& json, EndpointConfig& endpoint);
};

#endif // WIFI_DEVICE_MANAGER_H
