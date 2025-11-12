#include "ZoneRouter.h"
#include <StreamLogger.h>

extern StreamLogger* EspHubLog;

#define ROUTE_DISCOVERY_INTERVAL 60000  // Discover routes every 60 seconds
#define ROUTE_CLEANUP_INTERVAL 30000    // Cleanup stale routes every 30 seconds
#define ROUTE_TIMEOUT 300000            // Routes expire after 5 minutes

// ============================================================================
// Initialization
// ============================================================================

ZoneRouter::ZoneRouter(ZoneManager* manager)
    : zoneManager(manager),
      lastDiscoveryTime(0) {
    memset(&stats, 0, sizeof(stats));
}

ZoneRouter::~ZoneRouter() {
    routingTable.clear();
}

void ZoneRouter::begin() {
    EspHubLog->println("ZoneRouter: Initialized");

    // Perform initial route discovery
    discoverRoutes();
}

void ZoneRouter::loop() {
    unsigned long now = millis();

    // Periodic route discovery (only if coordinator)
    if (zoneManager->isCoordinator()) {
        if (now - lastDiscoveryTime >= ROUTE_DISCOVERY_INTERVAL) {
            discoverRoutes();
            lastDiscoveryTime = now;
        }
    }

    // Cleanup stale routes
    static unsigned long lastCleanup = 0;
    if (now - lastCleanup >= ROUTE_CLEANUP_INTERVAL) {
        cleanupStaleRoutes();
        lastCleanup = now;
    }
}

// ============================================================================
// Route Management
// ============================================================================

bool ZoneRouter::addRoute(const String& zoneName, const uint8_t* coordinatorMac, uint8_t hopCount) {
    if (zoneName.isEmpty() || !coordinatorMac) {
        return false;
    }

    // Don't add route to own zone
    if (zoneName == zoneManager->getZoneName()) {
        return false;
    }

    // Check if route exists and is better
    auto it = routingTable.find(zoneName);
    if (it != routingTable.end()) {
        // Only update if new route is better (lower hop count)
        if (hopCount >= it->second.hopCount) {
            return false; // Existing route is better
        }
    }

    // Add or update route
    RouteEntry entry;
    entry.zoneName = zoneName;
    memcpy(entry.coordinatorMac, coordinatorMac, 6);
    entry.hopCount = hopCount;
    entry.lastUpdate = millis();
    entry.rssi = -50; // Placeholder
    entry.isActive = true;

    routingTable[zoneName] = entry;
    stats.routeUpdates++;

    char macStr[18];
    sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X",
            coordinatorMac[0], coordinatorMac[1], coordinatorMac[2],
            coordinatorMac[3], coordinatorMac[4], coordinatorMac[5]);

    EspHubLog->printf("ZoneRouter: Added route to zone '%s' via %s (hops=%d)\n",
                     zoneName.c_str(), macStr, hopCount);

    return true;
}

bool ZoneRouter::removeRoute(const String& zoneName) {
    auto it = routingTable.find(zoneName);
    if (it != routingTable.end()) {
        routingTable.erase(it);
        EspHubLog->printf("ZoneRouter: Removed route to zone '%s'\n", zoneName.c_str());
        return true;
    }
    return false;
}

const uint8_t* ZoneRouter::getRoute(const String& zoneName) {
    auto it = routingTable.find(zoneName);
    if (it != routingTable.end() && it->second.isValid()) {
        return it->second.coordinatorMac;
    }
    return nullptr;
}

bool ZoneRouter::hasRoute(const String& zoneName) {
    return getRoute(zoneName) != nullptr;
}

std::vector<String> ZoneRouter::getKnownZones() {
    std::vector<String> zones;
    for (const auto& pair : routingTable) {
        if (pair.second.isValid()) {
            zones.push_back(pair.first);
        }
    }
    return zones;
}

// ============================================================================
// Packet Routing
// ============================================================================

bool ZoneRouter::routePacket(const String& targetZone, ZoneMeshHeader& header,
                             const uint8_t* payload, size_t length) {
    if (!zoneManager->isCoordinator()) {
        EspHubLog->println("ERROR: Only coordinators can route packets");
        stats.routingErrors++;
        return false;
    }

    // Check if destination is local zone
    if (targetZone == zoneManager->getZoneName()) {
        EspHubLog->println("ERROR: Cannot route to local zone");
        stats.routingErrors++;
        return false;
    }

    // Get route to target zone
    const uint8_t* nextHopMac = getRoute(targetZone);
    if (!nextHopMac) {
        EspHubLog->printf("ERROR: No route to zone '%s'\n", targetZone.c_str());
        stats.routingErrors++;
        return false;
    }

    // Decrement TTL
    if (header.ttl == 0) {
        EspHubLog->println("ERROR: Packet TTL expired");
        stats.routingErrors++;
        return false;
    }
    header.ttl--;

    // Update header destination
    memcpy(header.destMac, nextHopMac, 6);
    strncpy(header.destZone, targetZone.c_str(), MAX_ZONE_NAME_LENGTH - 1);

    // Forward packet
    bool success = zoneManager->sendPacket(header, payload, length);

    if (success) {
        stats.packetsRouted++;
        EspHubLog->printf("ZoneRouter: Routed packet to zone '%s' (TTL=%d)\n",
                         targetZone.c_str(), header.ttl);
    } else {
        stats.routingErrors++;
    }

    return success;
}

// ============================================================================
// Route Discovery
// ============================================================================

void ZoneRouter::discoverRoutes() {
    if (!zoneManager->isCoordinator()) {
        return; // Only coordinators participate in route discovery
    }

    stats.discoveryAttempts++;

    // Broadcast route discovery query
    JsonDocument doc;
    doc["type"] = "route_discovery";
    doc["sourceZone"] = zoneManager->getZoneName();
    doc["hopCount"] = 0;
    doc["timestamp"] = millis();

    String payload;
    serializeJson(doc, payload);

    // Send as ZONE_QUERY packet type
    zoneManager->broadcastToZone(ZoneMeshPacketType::ZONE_QUERY,
                                (const uint8_t*)payload.c_str(),
                                payload.length());

    EspHubLog->printf("ZoneRouter: Broadcasting route discovery from zone '%s'\n",
                     zoneManager->getZoneName().c_str());
}

void ZoneRouter::handleRouteDiscovery(const ZoneMeshHeader& header, const JsonObject& payload) {
    if (!zoneManager->isCoordinator()) {
        return; // Only coordinators process route discovery
    }

    String sourceZone = payload["sourceZone"] | "";
    uint8_t hopCount = payload["hopCount"] | 0;

    if (sourceZone.isEmpty() || sourceZone == zoneManager->getZoneName()) {
        return; // Ignore invalid or own zone
    }

    // Add route to source zone (increment hop count)
    addRoute(sourceZone, header.sourceMac, hopCount + 1);

    // Respond with our zone information
    respondToDiscovery(header.sourceMac, sourceZone);

    // Rebroadcast discovery with incremented hop count (if TTL allows)
    if (header.ttl > 1 && hopCount < 5) {
        JsonDocument rebroadcastDoc;
        rebroadcastDoc["type"] = "route_discovery";
        rebroadcastDoc["sourceZone"] = sourceZone;
        rebroadcastDoc["hopCount"] = hopCount + 1;
        rebroadcastDoc["relayedBy"] = zoneManager->getZoneName();
        rebroadcastDoc["timestamp"] = millis();

        String rebroadcastPayload;
        serializeJson(rebroadcastDoc, rebroadcastPayload);

        zoneManager->broadcastToZone(ZoneMeshPacketType::ZONE_QUERY,
                                    (const uint8_t*)rebroadcastPayload.c_str(),
                                    rebroadcastPayload.length());

        EspHubLog->printf("ZoneRouter: Relayed route discovery from '%s' (hops=%d)\n",
                         sourceZone.c_str(), hopCount + 1);
    }
}

void ZoneRouter::respondToDiscovery(const uint8_t* sourceCoordinatorMac, const String& sourceZone) {
    // Send route response back to source coordinator
    JsonDocument doc;
    doc["type"] = "route_response";
    doc["sourceZone"] = zoneManager->getZoneName();
    doc["deviceCount"] = zoneManager->getZoneInfo().devices.size();
    doc["subscriptionCount"] = zoneManager->getSubscriptionCount();
    doc["timestamp"] = millis();

    String payload;
    serializeJson(doc, payload);

    // Create header for unicast response
    ZoneMeshHeader header;
    memset(&header, 0, sizeof(header));
    header.version = 1;
    header.type = ZoneMeshPacketType::ZONE_RESPONSE;
    header.ttl = 10;

    // Get source MAC and zone from discovery
    memcpy(header.destMac, sourceCoordinatorMac, 6);
    strncpy(header.destZone, sourceZone.c_str(), MAX_ZONE_NAME_LENGTH - 1);

    uint8_t myMac[6];
    WiFi.macAddress(myMac);
    memcpy(header.sourceMac, myMac, 6);
    strncpy(header.sourceZone, zoneManager->getZoneName().c_str(), MAX_ZONE_NAME_LENGTH - 1);

    header.payloadLength = payload.length();
    header.checksum = 0; // Calculate if needed

    zoneManager->sendPacket(header, (const uint8_t*)payload.c_str(), payload.length());

    EspHubLog->printf("ZoneRouter: Sent route response to zone '%s'\n", sourceZone.c_str());
}

void ZoneRouter::handleRouteResponse(const ZoneMeshHeader& header, const JsonObject& payload) {
    String sourceZone = payload["sourceZone"] | "";

    if (sourceZone.isEmpty()) {
        return;
    }

    // Add route based on response
    addRoute(sourceZone, header.sourceMac, 1); // Direct response = 1 hop

    EspHubLog->printf("ZoneRouter: Received route response from zone '%s'\n",
                     sourceZone.c_str());
}

// ============================================================================
// Internal Methods
// ============================================================================

void ZoneRouter::cleanupStaleRoutes() {
    std::vector<String> toRemove;

    // Find stale routes
    for (auto& pair : routingTable) {
        if (!pair.second.isValid()) {
            toRemove.push_back(pair.first);
        }
    }

    // Remove stale routes
    for (const String& zoneName : toRemove) {
        removeRoute(zoneName);
    }

    if (!toRemove.empty()) {
        EspHubLog->printf("ZoneRouter: Cleaned up %d stale routes\n", toRemove.size());
    }
}

void ZoneRouter::updateRoute(const String& zoneName, const uint8_t* coordinatorMac,
                            uint8_t hopCount, int8_t rssi) {
    auto it = routingTable.find(zoneName);
    if (it != routingTable.end()) {
        // Update existing route
        it->second.hopCount = hopCount;
        it->second.lastUpdate = millis();
        it->second.rssi = rssi;
        memcpy(it->second.coordinatorMac, coordinatorMac, 6);
        stats.routeUpdates++;
    } else {
        // Add new route
        addRoute(zoneName, coordinatorMac, hopCount);
    }
}
