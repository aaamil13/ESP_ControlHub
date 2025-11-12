#ifndef MESH_DEVICE_MANAGER_H
#define MESH_DEVICE_MANAGER_H

#include <Arduino.h>
#include <vector>
#include <map>
#include "ZoneManager.h"
#include "ZoneRouter.h"
#include "../../Devices/DeviceRegistry.h"

// Legacy structure for backward compatibility
struct MeshDevice {
    uint32_t nodeId;        // Legacy: painlessMesh node ID (deprecated)
    String name;            // Device domain name (e.g., "kitchen.light.1")
    unsigned long lastSeen;
    bool isOnline;
    String zoneName;        // Zone membership (e.g., "kitchen")

    MeshDevice() : nodeId(0), lastSeen(0), isOnline(false) {}
};

/**
 * MeshDeviceManager - Zone Mesh Integration Layer
 *
 * Integrates ZoneManager and ZoneRouter with the rest of EspHub.
 * Provides high-level API for:
 * - Device discovery and management
 * - Subscriptions to remote devices
 * - Publishing data to subscribers
 * - Zone mesh status and statistics
 */
class MeshDeviceManager {
public:
    MeshDeviceManager();
    ~MeshDeviceManager();

    // ============================================================================
    // Initialization
    // ============================================================================

    /**
     * Initialize mesh device manager
     * @param deviceName - This device's full domain name (e.g., "kitchen.esphub")
     * @param zoneName - Zone to join (e.g., "kitchen")
     */
    void begin(const String& deviceName, const String& zoneName);

    /**
     * Main loop - call periodically
     */
    void loop();

    /**
     * Set device capabilities (for coordinator election)
     */
    void setCapabilities(const CoordinatorCapabilities& caps);

    // ============================================================================
    // Device Management (Zone-aware)
    // ============================================================================

    /**
     * Get all devices in current zone
     */
    std::vector<MeshDevice> getZoneDevices();

    /**
     * Get all devices across all known zones
     */
    std::vector<MeshDevice> getAllDevices();

    /**
     * Get device by name
     */
    MeshDevice* getDeviceByName(const String& deviceName);

    /**
     * Check if device is online
     */
    bool isDeviceOnline(const String& deviceName);

    /**
     * Get zone name for this device
     */
    String getMyZoneName() const;

    /**
     * Check if this device is zone coordinator
     */
    bool isCoordinator() const;

    // ============================================================================
    // Subscription Management
    // ============================================================================

    /**
     * Subscribe to remote endpoint
     * @param endpoint - Full endpoint path (e.g., "kitchen.temp.value.real")
     * @param subscriberName - This device's name (or specific component)
     * @return true if subscription successful
     */
    bool subscribeToEndpoint(const String& endpoint, const String& subscriberName);

    /**
     * Unsubscribe from remote endpoint
     */
    bool unsubscribeFromEndpoint(const String& endpoint, const String& subscriberName);

    /**
     * Publish data to subscribers (coordinator only)
     * @param endpoint - Endpoint that changed
     * @param value - New value to publish
     */
    bool publishToSubscribers(const String& endpoint, const PlcValue& value);

    // ============================================================================
    // Zone and Routing Information
    // ============================================================================

    /**
     * Get current zone information
     */
    const ZoneInfo& getZoneInfo() const;

    /**
     * Get routing statistics
     */
    ZoneRouter::RouterStatistics getRouterStats() const;

    /**
     * Get zone statistics
     */
    const ZoneStatistics& getZoneStats() const;

    /**
     * Get known zones (from routing table)
     */
    std::vector<String> getKnownZones();

    /**
     * Get memory usage
     */
    size_t getMemoryUsage() const;

    // ============================================================================
    // Legacy API (for backward compatibility with painlessMesh)
    // ============================================================================

    void addDevice(uint32_t nodeId, const String& name);
    void updateDeviceLastSeen(uint32_t nodeId);
    MeshDevice* getDevice(uint32_t nodeId);
    void checkOfflineDevices(unsigned long offlineTimeoutMs);

    // ============================================================================
    // Integration Hooks
    // ============================================================================

    /**
     * Set device registry for endpoint management
     */
    void setDeviceRegistry(DeviceRegistry* registry);

    /**
     * Get ZoneManager instance (for advanced usage)
     */
    ZoneManager* getZoneManager() { return zoneManager; }

    /**
     * Get ZoneRouter instance (for advanced usage)
     */
    ZoneRouter* getZoneRouter() { return zoneRouter; }

private:
    // Zone mesh components
    ZoneManager* zoneManager;
    ZoneRouter* zoneRouter;
    DeviceRegistry* deviceRegistry;

    // Legacy device map (for backward compatibility)
    std::map<uint32_t, MeshDevice> legacyDevices;

    String myDeviceName;

    // Helper methods
    void syncDevicesFromZone();
    MeshDevice zoneDeviceToMeshDevice(const ZoneDevice& zoneDevice);
};

#endif // MESH_DEVICE_MANAGER_H
