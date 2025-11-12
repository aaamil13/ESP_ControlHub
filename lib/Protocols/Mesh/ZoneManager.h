#ifndef ZONE_MANAGER_H
#define ZONE_MANAGER_H

#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include "ZoneStructures.h"
#include "../../Core/StreamLogger.h"

extern StreamLogger* EspHubLog;

/**
 * ZoneManager - Zone-based Mesh Network Manager
 *
 * Responsibilities:
 * - Zone discovery and joining
 * - Coordinator election based on capabilities
 * - Zone member management
 * - Beacon transmission and processing
 * - Inter-zone coordinator communication
 *
 * Memory footprint: ~1-2KB per zone (vs 8KB for full mesh DHT)
 */
class ZoneManager {
public:
    ZoneManager();
    ~ZoneManager();

    // ============================================================================
    // Initialization and Configuration
    // ============================================================================

    /**
     * Initialize zone manager
     * @param deviceName - Full device name (e.g., "kitchen.light.1")
     * @param preferredZone - Preferred zone name (can be overridden by discovery)
     */
    void begin(const String& deviceName, const String& preferredZone);

    /**
     * Main loop - call periodically (every 100ms recommended)
     */
    void loop();

    /**
     * Set coordinator capabilities for this device
     */
    void setCapabilities(const CoordinatorCapabilities& caps);

    // ============================================================================
    // Zone Information
    // ============================================================================

    /**
     * Get current zone name
     */
    String getZoneName() const { return currentZone.zoneName; }

    /**
     * Get current role in zone
     */
    ZoneRole getRole() const { return myRole; }

    /**
     * Check if device is zone coordinator
     */
    bool isCoordinator() const { return myRole == ZoneRole::COORDINATOR; }

    /**
     * Get zone information
     */
    const ZoneInfo& getZoneInfo() const { return currentZone; }

    /**
     * Get zone statistics
     */
    const ZoneStatistics& getStatistics() const { return stats; }

    /**
     * Get memory usage in bytes
     */
    size_t getMemoryUsage() const;

    // ============================================================================
    // Coordinator Election
    // ============================================================================

    /**
     * Trigger coordinator election
     * Called automatically on:
     * - Zone initialization
     * - Coordinator timeout
     * - Better coordinator available
     */
    void triggerElection();

    /**
     * Handle election vote from another device
     */
    void handleElectionVote(const ZoneDevice& candidate);

    // ============================================================================
    // Device Discovery and Management
    // ============================================================================

    /**
     * Send beacon to announce presence
     */
    void sendBeacon();

    /**
     * Handle received beacon
     */
    void handleBeacon(const ZoneMeshHeader& header, const uint8_t* payload, size_t length);

    /**
     * Get all devices in current zone
     */
    const std::vector<ZoneDevice>& getZoneDevices() const { return currentZone.devices; }

    /**
     * Get device by name
     */
    ZoneDevice* getDevice(const String& deviceName);

    /**
     * Check if device is online
     */
    bool isDeviceOnline(const String& deviceName);

    // ============================================================================
    // Subscription Management (Coordinator Only)
    // ============================================================================

    /**
     * Add subscription (coordinator only)
     * @param publisherEndpoint - Endpoint to subscribe to (e.g., "kitchen.temp.value.real")
     * @param subscriberDevice - Device requesting subscription
     * @param subscriberZone - Subscriber's zone
     * @return true if subscription added successfully
     */
    bool addSubscription(const String& publisherEndpoint,
                        const String& subscriberDevice,
                        const String& subscriberZone);

    /**
     * Remove subscription (coordinator only)
     */
    bool removeSubscription(const String& publisherEndpoint, const String& subscriberDevice);

    /**
     * Get all subscribers for an endpoint (coordinator only)
     */
    std::vector<SubscriptionEntry> getSubscribers(const String& publisherEndpoint);

    /**
     * Get subscription count
     */
    uint32_t getSubscriptionCount() const { return currentZone.subscriptionCount; }

    // ============================================================================
    // Packet Handling
    // ============================================================================

    /**
     * Send packet to specific device
     */
    bool sendPacket(const ZoneMeshHeader& header, const uint8_t* payload, size_t length);

    /**
     * Broadcast packet to all zone members
     */
    bool broadcastToZone(ZoneMeshPacketType type, const uint8_t* payload, size_t length);

    /**
     * Route packet to another zone (via coordinators)
     */
    bool routeToZone(const String& targetZone, const ZoneMeshHeader& header,
                     const uint8_t* payload, size_t length);

    /**
     * ESP-NOW receive callback (static)
     */
    static void onDataReceived(const uint8_t* macAddr, const uint8_t* data, int len);

private:
    // ============================================================================
    // Internal State
    // ============================================================================

    String myDeviceName;                    // This device's name
    uint8_t myMacAddress[6];                // This device's MAC
    ZoneRole myRole;                        // Current role
    CoordinatorCapabilities myCapabilities; // This device's capabilities
    ZoneInfo currentZone;                   // Current zone information
    ZoneStatistics stats;                   // Statistics

    // Timing
    unsigned long lastBeaconTime;
    unsigned long lastElectionTime;
    unsigned long lastCleanupTime;

    // Election state
    bool electionInProgress;
    std::vector<CoordinatorCapabilities> electionCandidates;
    unsigned long electionStartTime;

    static ZoneManager* instance;           // Singleton for ESP-NOW callback

    // ============================================================================
    // Internal Methods
    // ============================================================================

    // Discovery and beacons
    void processBeacon(const ZoneMeshHeader& header, const JsonObject& payload);
    void sendCoordinatorBeacon();
    void sendDeviceBeacon();

    // Election
    void startElection();
    void processElectionVote(const JsonObject& payload);
    void processElectionResult(const JsonObject& payload);
    void becomeCoordinator();
    void becomeMember();

    // Packet processing
    void handleReceivedPacket(const uint8_t* macAddr, const uint8_t* data, size_t len);
    void processSubscribeRequest(const ZoneMeshHeader& header, const JsonObject& payload);
    void processUnsubscribeRequest(const ZoneMeshHeader& header, const JsonObject& payload);
    void processDataPublish(const ZoneMeshHeader& header, const uint8_t* payload, size_t length);
    void processZoneRoute(const ZoneMeshHeader& header, const uint8_t* payload, size_t length);

    // Device management
    void addOrUpdateDevice(const String& deviceName, const uint8_t* mac, int8_t rssi);
    void removeOfflineDevices();
    ZoneDevice* findDeviceByMac(const uint8_t* mac);

    // Utilities
    void fillHeader(ZoneMeshHeader& header, ZoneMeshPacketType type,
                   const uint8_t* destMac, const char* destZone);
    uint16_t calculateChecksum(const ZoneMeshHeader& header, const uint8_t* payload, size_t length);
    bool verifyChecksum(const ZoneMeshHeader& header, const uint8_t* payload, size_t length);
    void macToString(const uint8_t* mac, char* str);
    void printPacketInfo(const ZoneMeshHeader& header, const char* direction);
};

#endif // ZONE_MANAGER_H
