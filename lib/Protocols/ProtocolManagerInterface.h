#ifndef PROTOCOL_MANAGER_INTERFACE_H
#define PROTOCOL_MANAGER_INTERFACE_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include "../PlcCore/PlcMemory.h"

/**
 * ProtocolManagerInterface - Base interface for all protocol managers
 *
 * Each protocol manager (WiFi, BLE, RF433, Zigbee, Mesh) implements this
 * interface to provide a unified way to communicate with devices.
 *
 * Protocol managers are responsible ONLY for the transport layer:
 * - Sending/receiving data via their specific protocol
 * - Protocol-specific connection management
 * - Raw data exchange
 *
 * They do NOT:
 * - Define device types
 * - Parse device configurations (done by DeviceConfigManager)
 * - Manage device lifecycle (done by DeviceConfigManager)
 */
class ProtocolManagerInterface {
public:
    virtual ~ProtocolManagerInterface() {}

    /**
     * Initialize the protocol manager with global settings
     * Called once during hub startup
     */
    virtual void begin() = 0;

    /**
     * Main loop - called regularly to handle protocol-specific tasks
     * (polling, connection maintenance, etc.)
     */
    virtual void loop() = 0;

    /**
     * Initialize a device connection based on JSON config
     *
     * @param deviceId Unique device identifier
     * @param connectionConfig JSON object with protocol-specific connection settings
     * @return true if connection initialized successfully
     *
     * Example for WiFi:
     * {
     *   "type": "http",
     *   "host": "192.168.1.100",
     *   "port": 80
     * }
     *
     * Example for BLE:
     * {
     *   "mac_address": "AA:BB:CC:DD:EE:FF",
     *   "service_uuid": "..."
     * }
     */
    virtual bool initializeDevice(const String& deviceId, const JsonObject& connectionConfig) = 0;

    /**
     * Remove a device and clean up its resources
     *
     * @param deviceId Device to remove
     * @return true if successfully removed
     */
    virtual bool removeDevice(const String& deviceId) = 0;

    /**
     * Read value from a device endpoint
     *
     * @param deviceId Device identifier
     * @param endpointConfig JSON object with endpoint-specific read config
     * @param value Output parameter - will be filled with read value
     * @return true if read successful
     *
     * Example endpoint config for WiFi:
     * {
     *   "name": "temperature",
     *   "type": "real",
     *   "read": {
     *     "method": "GET",
     *     "path": "/api/temp",
     *     "value_path": "temperature"
     *   }
     * }
     *
     * Example for BLE:
     * {
     *   "name": "temperature",
     *   "type": "real",
     *   "characteristic_uuid": "...",
     *   "format": "int16",
     *   "scale": 0.01
     * }
     */
    virtual bool readEndpoint(const String& deviceId, const JsonObject& endpointConfig, PlcValue& value) = 0;

    /**
     * Write value to a device endpoint
     *
     * @param deviceId Device identifier
     * @param endpointConfig JSON object with endpoint-specific write config
     * @param value Value to write
     * @return true if write successful
     */
    virtual bool writeEndpoint(const String& deviceId, const JsonObject& endpointConfig, const PlcValue& value) = 0;

    /**
     * Test connection to a device
     *
     * @param connectionConfig JSON object with connection settings
     * @return true if connection test successful
     */
    virtual bool testConnection(const JsonObject& connectionConfig) = 0;

    /**
     * Test reading from a specific endpoint
     *
     * @param deviceId Device identifier
     * @param endpointConfig Endpoint configuration
     * @return true if test successful
     */
    virtual bool testEndpoint(const String& deviceId, const JsonObject& endpointConfig) = 0;

    /**
     * Get protocol name (e.g., "wifi", "ble", "rf433")
     */
    virtual String getProtocolName() const = 0;

    /**
     * Check if device is online/reachable
     *
     * @param deviceId Device identifier
     * @return true if device is online
     */
    virtual bool isDeviceOnline(const String& deviceId) = 0;
};

#endif // PROTOCOL_MANAGER_INTERFACE_H
