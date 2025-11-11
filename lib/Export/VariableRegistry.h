#ifndef VARIABLE_REGISTRY_H
#define VARIABLE_REGISTRY_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <map>
#include <vector>
#include <functional>
#include "../PlcEngine/Engine/PlcMemory.h"

/**
 * VariableRegistry - Unified variable access system
 *
 * Provides centralized access to variables from multiple sources:
 * - PLC memory (PlcProgram variables)
 * - Device endpoints (WiFi, RF433, Zigbee devices)
 * - Mesh network variables (from remote hubs)
 * - MQTT topics
 *
 * Features:
 * - Unified read/write interface
 * - Namespace support (hub_id.program.variable)
 * - Change callbacks for monitoring
 * - Automatic MQTT export
 * - Type conversion and validation
 *
 * Variable naming convention:
 * - Local PLC: "program_name.variable_name"
 * - Device endpoint: "device_id.endpoint_name"
 * - Mesh remote: "hub_id.program.variable"
 * - MQTT: "mqtt/topic/path"
 */

enum class VariableSource {
    PLC_MEMORY,      // PlcProgram memory
    DEVICE_ENDPOINT, // Device endpoint (WiFi, RF433, Zigbee)
    MESH_REMOTE,     // Remote hub variable via mesh
    MQTT_TOPIC,      // MQTT topic
    VIRTUAL          // Virtual computed variable
};

struct VariableMetadata {
    String fullName;          // Full qualified name (e.g., "main.sensor1" or "device1.temperature")
    String namespace_;        // Namespace part (e.g., "main" or "device1")
    String localName;         // Local name (e.g., "sensor1" or "temperature")
    VariableSource source;    // Source of the variable
    PlcValueType type;        // Data type
    bool readable;            // Can be read
    bool writable;            // Can be written
    bool exportToMqtt;        // Should be exported to MQTT
    String mqttTopic;         // MQTT topic for export (if enabled)
    String description;       // Human-readable description
    unsigned long lastUpdate; // Timestamp of last update
};

// Callback types
using VariableChangeCallback = std::function<void(const String& varName, const PlcValue& oldValue, const PlcValue& newValue)>;
using VariableReadCallback = std::function<bool(const String& varName, PlcValue& value)>;
using VariableWriteCallback = std::function<bool(const String& varName, const PlcValue& value)>;

class VariableRegistry {
public:
    VariableRegistry();
    ~VariableRegistry();

    void begin();

    // ============================================================================
    // Variable Registration
    // ============================================================================

    /**
     * Register a PLC variable
     * @param programName Name of PLC program
     * @param varName Variable name in PLC memory
     * @param type Variable type
     * @param exportToMqtt Auto-export to MQTT
     */
    bool registerPlcVariable(const String& programName, const String& varName,
                            PlcValueType type, bool exportToMqtt = true);

    /**
     * Register a device endpoint
     * @param deviceId Device ID
     * @param endpointName Endpoint name
     * @param type Data type
     * @param readable Can be read
     * @param writable Can be written
     * @param exportToMqtt Auto-export to MQTT
     */
    bool registerDeviceEndpoint(const String& deviceId, const String& endpointName,
                               PlcValueType type, bool readable, bool writable,
                               bool exportToMqtt = true);

    /**
     * Register a mesh remote variable
     * @param hubId Remote hub ID
     * @param programName Program name on remote hub
     * @param varName Variable name
     * @param type Data type
     */
    bool registerMeshVariable(const String& hubId, const String& programName,
                             const String& varName, PlcValueType type);

    /**
     * Register an MQTT topic as variable
     * @param topicPath MQTT topic path
     * @param varName Local variable name
     * @param type Data type
     */
    bool registerMqttVariable(const String& topicPath, const String& varName, PlcValueType type);

    /**
     * Unregister a variable
     */
    bool unregisterVariable(const String& fullName);

    // ============================================================================
    // Variable Access
    // ============================================================================

    /**
     * Read variable value
     * @param fullName Full qualified variable name
     * @param value Output value
     * @return true if successful
     */
    bool readVariable(const String& fullName, PlcValue& value);

    /**
     * Write variable value
     * @param fullName Full qualified variable name
     * @param value Value to write
     * @return true if successful
     */
    bool writeVariable(const String& fullName, const PlcValue& value);

    /**
     * Check if variable exists
     */
    bool hasVariable(const String& fullName) const;

    /**
     * Get variable metadata
     */
    bool getMetadata(const String& fullName, VariableMetadata& metadata);

    // ============================================================================
    // Callbacks
    // ============================================================================

    /**
     * Set callback for variable changes
     * Called after variable value changes
     */
    void onVariableChange(VariableChangeCallback callback);

    /**
     * Set custom read callback for a specific variable
     * Overrides default read behavior
     */
    void setReadCallback(const String& fullName, VariableReadCallback callback);

    /**
     * Set custom write callback for a specific variable
     * Overrides default write behavior
     */
    void setWriteCallback(const String& fullName, VariableWriteCallback callback);

    // ============================================================================
    // Query and Export
    // ============================================================================

    /**
     * Get all variable names
     */
    std::vector<String> getAllVariables() const;

    /**
     * Get variables by source
     */
    std::vector<String> getVariablesBySource(VariableSource source) const;

    /**
     * Get variables by namespace
     */
    std::vector<String> getVariablesByNamespace(const String& namespace_) const;

    /**
     * Get all variables that should be exported to MQTT
     */
    std::vector<String> getMqttExportVariables() const;

    /**
     * Export variable state to JSON
     */
    void exportToJson(JsonObject& json, const String& namespace_ = "");

    /**
     * Import variable state from JSON
     */
    void importFromJson(const JsonObject& json, const String& namespace_ = "");

    // ============================================================================
    // Integration Hooks
    // ============================================================================

    /**
     * Set PlcEngine reference for PLC variable access
     */
    void setPlcEngine(class PlcEngine* engine);

    /**
     * Set DeviceConfigManager reference for device endpoint access
     */
    void setDeviceConfigManager(class DeviceConfigManager* manager);

    /**
     * Set MqttManager reference for MQTT operations
     */
    void setMqttManager(class MqttManager* manager);

    /**
     * Set local hub ID for namespace
     */
    void setLocalHubId(const String& hubId);

    String getLocalHubId() const { return localHubId; }

private:
    // Variable storage
    std::map<String, VariableMetadata> variables;

    // Custom callbacks
    std::map<String, VariableReadCallback> readCallbacks;
    std::map<String, VariableWriteCallback> writeCallbacks;
    VariableChangeCallback changeCallback;

    // Integration references
    class PlcEngine* plcEngine;
    class DeviceConfigManager* deviceConfigManager;
    class MqttManager* mqttManager;

    String localHubId;

    // Helper methods
    String buildFullName(const String& namespace_, const String& localName);
    void parseFullName(const String& fullName, String& namespace_, String& localName);
    bool readFromPlc(const VariableMetadata& meta, PlcValue& value);
    bool writeToPlc(const VariableMetadata& meta, const PlcValue& value);
    bool readFromDevice(const VariableMetadata& meta, PlcValue& value);
    bool writeToDevice(const VariableMetadata& meta, const PlcValue& value);
    void notifyChange(const String& varName, const PlcValue& oldValue, const PlcValue& newValue);
};

#endif // VARIABLE_REGISTRY_H
