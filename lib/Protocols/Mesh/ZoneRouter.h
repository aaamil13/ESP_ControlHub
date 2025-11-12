#ifndef ZONE_ROUTER_H
#define ZONE_ROUTER_H

#include <Arduino.h>
#include <map>
#include <vector>
#include "ZoneStructures.h"
#include "ZoneManager.h"

/**
 * ZoneRouter - Inter-Zone Routing Protocol
 *
 * Handles routing of packets between different zones via coordinators.
 *
 * Architecture:
 * Zone A (kitchen)          Zone B (livingroom)
 *    [Device] ---\              /--- [Device]
 *    [Device] ----[Coord A]----[Coord B]---- [Device]
 *    [Device] ---/              \--- [Device]
 *
 * Routing table maintained at each coordinator:
 * - Known zones and their coordinator MACs
 * - Path costs (hop count)
 * - Last update time
 *
 * Memory footprint: ~500 bytes per zone router
 */
class ZoneRouter {
public:
    ZoneRouter(ZoneManager* zoneManager);
    ~ZoneRouter();

    // ============================================================================
    // Initialization
    // ============================================================================

    void begin();
    void loop();

    // ============================================================================
    // Route Management
    // ============================================================================

    /**
     * Add known coordinator route
     * @param zoneName - Target zone name
     * @param coordinatorMac - Coordinator MAC address
     * @param hopCount - Number of hops to reach coordinator
     */
    bool addRoute(const String& zoneName, const uint8_t* coordinatorMac, uint8_t hopCount);

    /**
     * Remove route to zone
     */
    bool removeRoute(const String& zoneName);

    /**
     * Get route to zone
     * @return MAC address of next hop coordinator (nullptr if not found)
     */
    const uint8_t* getRoute(const String& zoneName);

    /**
     * Check if route to zone exists
     */
    bool hasRoute(const String& zoneName);

    /**
     * Get all known zones
     */
    std::vector<String> getKnownZones();

    /**
     * Get routing table size
     */
    size_t getRouteCount() const { return routingTable.size(); }

    // ============================================================================
    // Packet Routing
    // ============================================================================

    /**
     * Route packet to another zone
     * @param targetZone - Destination zone name
     * @param header - Packet header
     * @param payload - Packet payload
     * @param length - Payload length
     * @return true if successfully routed
     */
    bool routePacket(const String& targetZone, ZoneMeshHeader& header,
                    const uint8_t* payload, size_t length);

    /**
     * Handle route discovery request
     */
    void handleRouteDiscovery(const ZoneMeshHeader& header, const JsonObject& payload);

    /**
     * Handle route discovery response
     */
    void handleRouteResponse(const ZoneMeshHeader& header, const JsonObject& payload);

    // ============================================================================
    // Route Discovery
    // ============================================================================

    /**
     * Broadcast route discovery to find coordinators of other zones
     * Called periodically to discover new zones
     */
    void discoverRoutes();

    /**
     * Respond to route discovery from another coordinator
     */
    void respondToDiscovery(const uint8_t* sourceCoordinatorMac, const String& sourceZone);

    // ============================================================================
    // Statistics
    // ============================================================================

    struct RouterStatistics {
        uint32_t packetsRouted;
        uint32_t routingErrors;
        uint32_t routeUpdates;
        uint32_t discoveryAttempts;
        unsigned long lastDiscovery;
    };

    const RouterStatistics& getStatistics() const { return stats; }

private:
    // ============================================================================
    // Route Entry
    // ============================================================================

    struct RouteEntry {
        String zoneName;            // Target zone name
        uint8_t coordinatorMac[6];  // Coordinator MAC address
        uint8_t hopCount;           // Hops to reach (1 = direct neighbor)
        unsigned long lastUpdate;   // Last route update time
        int8_t rssi;                // Link quality (RSSI)
        bool isActive;              // Route is active

        RouteEntry() : hopCount(255), lastUpdate(0), rssi(-100), isActive(false) {
            memset(coordinatorMac, 0, 6);
        }

        bool isValid() const {
            return isActive && (millis() - lastUpdate < 300000); // Valid for 5 minutes
        }
    };

    // ============================================================================
    // Internal State
    // ============================================================================

    ZoneManager* zoneManager;
    std::map<String, RouteEntry> routingTable;  // zoneName -> route
    RouterStatistics stats;
    unsigned long lastDiscoveryTime;

    // ============================================================================
    // Internal Methods
    // ============================================================================

    void cleanupStaleRoutes();
    void updateRoute(const String& zoneName, const uint8_t* coordinatorMac,
                    uint8_t hopCount, int8_t rssi);
};

#endif // ZONE_ROUTER_H
