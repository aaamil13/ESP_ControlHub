#ifndef ZONE_STRUCTURES_H
#define ZONE_STRUCTURES_H

#include <Arduino.h>
#include <vector>
#include <map>

// ============================================================================
// Zone Mesh Configuration
// ============================================================================

#define MAX_DEVICES_PER_ZONE 30      // Maximum devices in a single zone
#define MAX_SUBSCRIPTIONS_PER_DEVICE 10  // Max subscribers per endpoint
#define COORDINATOR_BEACON_INTERVAL 30000  // Coordinator announces every 30s
#define DEVICE_BEACON_INTERVAL 60000      // Device announces every 60s
#define COORDINATOR_ELECTION_TIMEOUT 5000 // Wait 5s for better coordinator
#define MAX_ZONE_NAME_LENGTH 32
#define MAX_DEVICE_NAME_LENGTH 64

// ============================================================================
// Device Role in Zone
// ============================================================================

enum class ZoneRole {
    UNASSIGNED,      // Not yet joined a zone
    MEMBER,          // Regular zone member
    COORDINATOR,     // Zone coordinator
    CANDIDATE        // Candidate for coordinator (during election)
};

// ============================================================================
// Zone Coordinator Capabilities
// ============================================================================

struct CoordinatorCapabilities {
    uint32_t freeRam;           // Free RAM in bytes
    uint32_t uptime;            // Uptime in seconds
    uint8_t currentLoad;        // Current CPU load percentage (0-100)
    uint8_t deviceCount;        // Number of devices currently managed
    bool hasExternalPower;      // true = AC powered, false = battery
    int8_t rssiAverage;         // Average RSSI to zone members

    CoordinatorCapabilities()
        : freeRam(0), uptime(0), currentLoad(0), deviceCount(0),
          hasExternalPower(false), rssiAverage(-80) {}

    // Calculate coordinator score (higher is better)
    uint32_t calculateScore() const {
        uint32_t score = 0;

        // RAM is most important (weight: 40%)
        score += (freeRam / 1024) * 40;

        // Uptime (weight: 20%, max 30 days)
        uint32_t uptimeScore = min(uptime, (uint32_t)2592000) / 86400; // days
        score += uptimeScore * 20;

        // Low load is good (weight: 15%)
        score += (100 - currentLoad) * 15 / 100;

        // Fewer managed devices (weight: 10%)
        score += (MAX_DEVICES_PER_ZONE - deviceCount) * 10 / MAX_DEVICES_PER_ZONE;

        // External power is critical (weight: 10%)
        if (hasExternalPower) {
            score += 1000;
        }

        // Good RSSI (weight: 5%)
        score += (100 + rssiAverage) * 5 / 100;

        return score;
    }
};

// ============================================================================
// Zone Device Entry
// ============================================================================

struct ZoneDevice {
    String deviceName;           // Full domain name (e.g., "kitchen.light.1")
    uint8_t macAddress[6];       // Device MAC address
    ZoneRole role;               // Device role in zone
    unsigned long lastSeen;      // Last beacon time (millis)
    int8_t rssi;                 // Last RSSI value
    CoordinatorCapabilities capabilities; // For coordinator election

    ZoneDevice()
        : role(ZoneRole::UNASSIGNED), lastSeen(0), rssi(-100) {
        memset(macAddress, 0, 6);
    }

    bool isOnline() const {
        return (millis() - lastSeen) < 120000; // Online if seen in last 2 minutes
    }
};

// ============================================================================
// Subscription Entry
// ============================================================================

struct SubscriptionEntry {
    String publisherEndpoint;    // Full endpoint (e.g., "kitchen.temp.value.real")
    String subscriberDevice;     // Subscriber device name (e.g., "livingroom.dashboard")
    String subscriberZone;       // Subscriber's zone (for routing)
    bool isLocal;                // true = same zone, false = different zone
    unsigned long lastUpdate;    // Last time value was sent
    uint32_t updateInterval;     // Minimum update interval (ms) - for throttling

    SubscriptionEntry()
        : isLocal(true), lastUpdate(0), updateInterval(0) {}
};

// ============================================================================
// Zone Information
// ============================================================================

struct ZoneInfo {
    String zoneName;                          // Zone name (e.g., "kitchen")
    String coordinatorDevice;                 // Current coordinator device name
    uint8_t coordinatorMac[6];                // Coordinator MAC address
    std::vector<ZoneDevice> devices;          // All devices in zone
    std::map<String, std::vector<SubscriptionEntry>> subscriptions; // endpoint -> subscribers
    unsigned long lastCoordinatorBeacon;      // Last coordinator beacon time
    uint32_t subscriptionCount;               // Total active subscriptions

    ZoneInfo() : lastCoordinatorBeacon(0), subscriptionCount(0) {
        memset(coordinatorMac, 0, 6);
    }

    size_t getMemoryUsage() const {
        // Calculate approximate memory usage
        size_t usage = sizeof(ZoneInfo);
        usage += devices.size() * sizeof(ZoneDevice);
        usage += subscriptionCount * sizeof(SubscriptionEntry);
        usage += zoneName.length() + coordinatorDevice.length();
        return usage;
    }
};

// ============================================================================
// Zone Mesh Packet Types
// ============================================================================

enum class ZoneMeshPacketType : uint8_t {
    // Discovery and Management
    COORDINATOR_BEACON = 0x01,      // Coordinator announces presence
    DEVICE_BEACON = 0x02,           // Device announces presence
    ELECTION_VOTE = 0x03,           // Vote during coordinator election
    ELECTION_RESULT = 0x04,         // New coordinator announcement

    // Subscription Management
    SUBSCRIBE_REQUEST = 0x10,       // Subscribe to endpoint
    SUBSCRIBE_ACK = 0x11,           // Subscription confirmed
    UNSUBSCRIBE_REQUEST = 0x12,     // Unsubscribe from endpoint

    // Data Transfer
    DATA_PUBLISH = 0x20,            // Publish data to subscribers
    DATA_UNICAST = 0x21,            // Unicast data to specific device

    // Inter-Zone Routing
    ZONE_ROUTE = 0x30,              // Route packet to another zone
    ZONE_QUERY = 0x31,              // Query coordinator of another zone
    ZONE_RESPONSE = 0x32,           // Response from another coordinator

    // Diagnostics
    PING = 0xF0,                    // Ping request
    PONG = 0xF1,                    // Ping response
    STATUS_QUERY = 0xF2,            // Query zone status
    STATUS_RESPONSE = 0xF3          // Zone status response
};

// ============================================================================
// Zone Mesh Packet Header (Binary for efficiency)
// ============================================================================

struct __attribute__((packed)) ZoneMeshHeader {
    uint8_t version;                // Protocol version (current: 1)
    ZoneMeshPacketType type;        // Packet type
    uint8_t ttl;                    // Time to live (hops remaining)
    uint8_t flags;                  // Packet flags
    uint8_t sourceMac[6];           // Source device MAC
    uint8_t destMac[6];             // Destination MAC (FF:FF:FF:FF:FF:FF for broadcast)
    char sourceZone[MAX_ZONE_NAME_LENGTH];  // Source zone name
    char destZone[MAX_ZONE_NAME_LENGTH];    // Destination zone name
    uint16_t payloadLength;         // Payload length in bytes
    uint16_t checksum;              // Header + payload checksum

    ZoneMeshHeader() {
        memset(this, 0, sizeof(ZoneMeshHeader));
        version = 1;
        ttl = 10;
    }
};

// Packet flags
#define ZONE_FLAG_ACK_REQUIRED    0x01
#define ZONE_FLAG_CRITICAL        0x02
#define ZONE_FLAG_COMPRESSED      0x04
#define ZONE_FLAG_ENCRYPTED       0x08

// ============================================================================
// Zone Statistics
// ============================================================================

struct ZoneStatistics {
    uint32_t packetsReceived;
    uint32_t packetsSent;
    uint32_t packetsDropped;
    uint32_t packetsRouted;
    uint32_t subscriptionChanges;
    uint32_t coordinatorChanges;
    unsigned long lastElection;
    size_t currentMemoryUsage;

    ZoneStatistics() {
        memset(this, 0, sizeof(ZoneStatistics));
    }
};

#endif // ZONE_STRUCTURES_H
