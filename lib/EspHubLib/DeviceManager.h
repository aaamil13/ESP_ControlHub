#ifndef DEVICE_MANAGER_H
#define DEVICE_MANAGER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>
#include <map>
#include "DeviceRegistry.h"

// Abstract base class for all protocol-specific device managers
class DeviceManager {
public:
    DeviceManager(const String& protocolName, ProtocolType protocolType);
    virtual ~DeviceManager();

    // Must be implemented by derived classes
    virtual void begin() = 0;
    virtual void loop() = 0;

    // Device registration (can be overridden)
    virtual bool registerDevice(const String& deviceId, const JsonObject& config);
    virtual void removeDevice(const String& deviceId);

    // Status management
    virtual void updateDeviceStatus(const String& deviceId, bool isOnline);
    virtual std::vector<DeviceStatus*> getAllDevices();
    virtual DeviceStatus* getDevice(const String& deviceId);

    // Protocol info
    String getProtocolName() const { return protocolName; }
    ProtocolType getProtocolType() const { return protocolType; }

    // Offline checking
    void checkOfflineDevices(uint32_t timeout_ms);

protected:
    String protocolName;
    ProtocolType protocolType;
    DeviceRegistry* registry;  // Reference to global registry

    // Helper functions for derived classes
    bool registerEndpointHelper(const Endpoint& endpoint);
    void updateEndpointStatusHelper(const String& fullName, bool isOnline);
    void updateEndpointValueHelper(const String& fullName, const PlcValue& value);

    // Build device ID from components
    String buildDeviceId(const String& location, const String& deviceName);
};

#endif // DEVICE_MANAGER_H
