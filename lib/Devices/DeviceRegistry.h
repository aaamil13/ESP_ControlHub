#ifndef DEVICE_REGISTRY_H
#define DEVICE_REGISTRY_H

#include <Arduino.h>
#include <vector>
#include <map>
#include <functional>
#include "../PlcEngine/Engine/PlcMemory.h"

// Protocol types
enum class ProtocolType {
    MESH,
    ZIGBEE,
    BLE,
    WIFI,
    RF433,
    MODBUS,
    UNKNOWN
};

// IO Direction for PLC mapping
enum class IODirection {
    IO_INPUT,    // Read from external device
    IO_OUTPUT    // Write to external device
};

// Endpoint definition - represents a single data point from a device
struct Endpoint {
    String fullName;           // kitchen.zigbee.relay.switch1.bool
    String location;           // kitchen
    ProtocolType protocol;     // ZIGBEE
    String deviceId;           // relay
    String endpoint;           // switch1
    PlcValueType datatype;     // BOOL, REAL, INT, etc.
    bool isOnline;             // Device online status
    unsigned long lastSeen;    // Last communication timestamp (millis)
    bool isWritable;           // Input (read-only) vs Output (writable)
    String mqttTopic;          // Auto-generated MQTT topic
    PlcValue currentValue;     // Current value

    Endpoint() :
        protocol(ProtocolType::UNKNOWN),
        datatype(PlcValueType::BOOL),
        isOnline(false),
        lastSeen(0),
        isWritable(false) {}
};

// Device status - represents the overall device state
struct DeviceStatus {
    String deviceId;           // Full device path (e.g., kitchen.zigbee.relay)
    ProtocolType protocol;     // Protocol type
    bool isOnline;             // Overall device status
    unsigned long lastSeen;    // Last seen timestamp
    uint32_t offlineThreshold; // Timeout before marking offline (ms)
    std::vector<String> endpoints; // List of endpoint names

    DeviceStatus() :
        protocol(ProtocolType::UNKNOWN),
        isOnline(false),
        lastSeen(0),
        offlineThreshold(60000) {} // Default 60 seconds
};

// PLC IO Point - maps endpoint to PLC variable
struct PlcIOPoint {
    String plcVarName;         // PLC variable name
    String mappedEndpoint;     // kitchen.zigbee.relay.switch1.bool
    IODirection direction;     // INPUT or OUTPUT
    bool requiresFunction;     // For outputs - requires function call
    String functionName;       // Required function for this output
    bool autoSync;             // Auto-sync with endpoint
    String ownerProgram;       // Name of PLC program that owns this IO point (for output ownership tracking)

    PlcIOPoint() :
        direction(IODirection::IO_INPUT),
        requiresFunction(false),
        autoSync(true),
        ownerProgram("") {}
};

// Forward declaration
class PlcMemory;

// DeviceRegistry - Singleton manager for all endpoints and devices
class DeviceRegistry {
public:
    static DeviceRegistry& getInstance();

    // Device management
    bool registerEndpoint(const Endpoint& endpoint);
    bool removeEndpoint(const String& fullName);
    Endpoint* getEndpoint(const String& fullName);
    std::vector<Endpoint*> getAllEndpoints();
    std::vector<Endpoint*> getEndpointsByProtocol(ProtocolType protocol);
    std::vector<Endpoint*> getEndpointsByLocation(const String& location);
    std::vector<Endpoint*> getEndpointsByDevice(const String& deviceId);

    // Status management
    void updateEndpointStatus(const String& fullName, bool isOnline);
    void updateEndpointValue(const String& fullName, const PlcValue& value);
    void checkOfflineDevices(uint32_t timeout_ms);

    // Device status
    bool registerDevice(const DeviceStatus& device);
    DeviceStatus* getDevice(const String& deviceId);
    std::vector<DeviceStatus*> getAllDevices();
    void updateDeviceStatus(const String& deviceId, bool isOnline);

    // PLC Integration
    bool registerIOPoint(const PlcIOPoint& ioPoint);
    bool unregisterIOPoint(const String& plcVarName);
    PlcIOPoint* getIOPoint(const String& plcVarName);
    std::vector<PlcIOPoint*> getAllIOPoints();

    void setPlcMemory(PlcMemory* memory) { plcMemory = memory; }
    void syncToPLC();    // Update PLC variables from endpoints
    void syncFromPLC();  // Update endpoints from PLC outputs

    // Callbacks
    typedef std::function<void(const String&, bool)> StatusCallback;
    typedef std::function<void(const String&, const PlcValue&)> ValueCallback;

    void onStatusChange(StatusCallback callback);
    void onValueChange(ValueCallback callback);

    // Utility functions
    String protocolToString(ProtocolType protocol);
    ProtocolType stringToProtocol(const String& protocolStr);

    // Parse endpoint name: "location.protocol.device.endpoint.datatype"
    static bool parseEndpointName(const String& fullName,
                                  String& location,
                                  String& protocol,
                                  String& device,
                                  String& endpoint,
                                  String& datatype);

    // Build endpoint name from components
    static String buildEndpointName(const String& location,
                                    const String& protocol,
                                    const String& device,
                                    const String& endpoint,
                                    PlcValueType datatype);

    // Clear all data
    void clear();

private:
    DeviceRegistry();
    ~DeviceRegistry();
    DeviceRegistry(const DeviceRegistry&) = delete;
    DeviceRegistry& operator=(const DeviceRegistry&) = delete;

    std::map<String, Endpoint> endpoints;
    std::map<String, DeviceStatus> devices;
    std::map<String, PlcIOPoint> ioPoints;

    std::vector<StatusCallback> statusCallbacks;
    std::vector<ValueCallback> valueCallbacks;

    PlcMemory* plcMemory;

    void triggerStatusCallbacks(const String& fullName, bool isOnline);
    void triggerValueCallbacks(const String& fullName, const PlcValue& value);
};

#endif // DEVICE_REGISTRY_H
