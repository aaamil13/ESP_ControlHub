#include "../Protocols/Mesh/MeshDeviceManager.h"
#include "../Core/StreamLogger.h"

extern StreamLogger* EspHubLog;

// ============================================================================
// Initialization
// ============================================================================

MeshDeviceManager::MeshDeviceManager()
    : zoneManager(nullptr),
      zoneRouter(nullptr),
      deviceRegistry(nullptr) {
}

MeshDeviceManager::~MeshDeviceManager() {
    if (zoneManager) delete zoneManager;
    if (zoneRouter) delete zoneRouter;
}

void MeshDeviceManager::begin(const String& deviceName, const String& zoneName) {
    myDeviceName = deviceName;

    // Create ZoneManager and ZoneRouter
    zoneManager = new ZoneManager();
    zoneRouter = new ZoneRouter(zoneManager);

    // Initialize zone mesh
    zoneManager->begin(deviceName, zoneName);
    zoneRouter->begin();

    EspHubLog->printf("MeshDeviceManager: Initialized for device '%s' in zone '%s'\n",
                     deviceName.c_str(), zoneName.c_str());
}

void MeshDeviceManager::loop() {
    if (zoneManager) {
        zoneManager->loop();
    }
    if (zoneRouter) {
        zoneRouter->loop();
    }

    // Sync devices from zone to legacy format
    syncDevicesFromZone();
}

void MeshDeviceManager::setCapabilities(const CoordinatorCapabilities& caps) {
    if (zoneManager) {
        zoneManager->setCapabilities(caps);
    }
}

// ============================================================================
// Device Management (Zone-aware)
// ============================================================================

std::vector<MeshDevice> MeshDeviceManager::getZoneDevices() {
    std::vector<MeshDevice> result;

    if (!zoneManager) {
        return result;
    }

    const auto& zoneDevices = zoneManager->getZoneDevices();
    for (const auto& zoneDevice : zoneDevices) {
        result.push_back(zoneDeviceToMeshDevice(zoneDevice));
    }

    return result;
}

std::vector<MeshDevice> MeshDeviceManager::getAllDevices() {
    // For now, return only zone devices
    // In future, could aggregate devices from all known zones
    return getZoneDevices();
}

MeshDevice* MeshDeviceManager::getDeviceByName(const String& deviceName) {
    // Check if device exists in zone
    if (!zoneManager) {
        return nullptr;
    }

    ZoneDevice* zoneDevice = zoneManager->getDevice(deviceName);
    if (!zoneDevice) {
        return nullptr;
    }

    // Convert to MeshDevice and cache in legacy map
    MeshDevice meshDevice = zoneDeviceToMeshDevice(*zoneDevice);
    legacyDevices[0] = meshDevice; // Use nodeId 0 for zone-managed devices
    return &legacyDevices[0];
}

bool MeshDeviceManager::isDeviceOnline(const String& deviceName) {
    if (!zoneManager) {
        return false;
    }
    return zoneManager->isDeviceOnline(deviceName);
}

String MeshDeviceManager::getMyZoneName() const {
    if (zoneManager) {
        return zoneManager->getZoneName();
    }
    return "";
}

bool MeshDeviceManager::isCoordinator() const {
    if (zoneManager) {
        return zoneManager->isCoordinator();
    }
    return false;
}

// ============================================================================
// Subscription Management
// ============================================================================

bool MeshDeviceManager::subscribeToEndpoint(const String& endpoint, const String& subscriberName) {
    if (!zoneManager) {
        EspHubLog->println("ERROR: ZoneManager not initialized");
        return false;
    }

    // Parse endpoint to determine zone
    // Format: "zone.device.endpoint.type"
    int firstDot = endpoint.indexOf('.');
    if (firstDot < 0) {
        EspHubLog->printf("ERROR: Invalid endpoint format: %s\n", endpoint.c_str());
        return false;
    }

    String targetZone = endpoint.substring(0, firstDot);
    String myZone = zoneManager->getZoneName();

    // If target is in same zone, subscribe locally
    if (targetZone == myZone) {
        if (zoneManager->isCoordinator()) {
            return zoneManager->addSubscription(endpoint, subscriberName, myZone);
        } else {
            // Send subscription request to coordinator
            EspHubLog->println("TODO: Send subscription request to local coordinator");
            return false;
        }
    } else {
        // Inter-zone subscription - route through coordinators
        if (!zoneRouter) {
            EspHubLog->println("ERROR: ZoneRouter not initialized");
            return false;
        }

        // Check if we have route to target zone
        if (!zoneRouter->hasRoute(targetZone)) {
            EspHubLog->printf("ERROR: No route to zone '%s'\n", targetZone.c_str());
            return false;
        }

        // TODO: Implement inter-zone subscription via coordinators
        EspHubLog->println("TODO: Implement inter-zone subscription");
        return false;
    }
}

bool MeshDeviceManager::unsubscribeFromEndpoint(const String& endpoint, const String& subscriberName) {
    if (!zoneManager || !zoneManager->isCoordinator()) {
        return false;
    }

    return zoneManager->removeSubscription(endpoint, subscriberName);
}

bool MeshDeviceManager::publishToSubscribers(const String& endpoint, const PlcValue& value) {
    if (!zoneManager || !zoneManager->isCoordinator()) {
        return false;
    }

    // Get subscribers for this endpoint
    auto subscribers = zoneManager->getSubscribers(endpoint);
    if (subscribers.empty()) {
        return true; // No subscribers, nothing to do
    }

    // Serialize value to JSON
    JsonDocument doc;
    doc["endpoint"] = endpoint;
    doc["timestamp"] = millis();

    // Add value based on type
    switch (value.type) {
        case PlcValueType::BOOL:
            doc["value"] = value.value.bVal;
            break;
        case PlcValueType::INT:
            doc["value"] = value.value.i16Val;
            break;
        case PlcValueType::REAL:
            doc["value"] = value.value.fVal;
            break;
        case PlcValueType::STRING_TYPE:
            doc["value"] = value.value.sVal;
            break;
        default:
            doc["value"] = nullptr;
            break;
    }

    String payload;
    serializeJson(doc, payload);

    // Send to each subscriber
    for (const auto& sub : subscribers) {
        if (sub.isLocal) {
            // Local subscriber - direct delivery
            // TODO: Implement local delivery mechanism
            EspHubLog->printf("Publishing to local subscriber: %s\n", sub.subscriberDevice.c_str());
        } else {
            // Remote subscriber - route through zone coordinator
            if (zoneRouter && zoneRouter->hasRoute(sub.subscriberZone)) {
                // TODO: Route packet to subscriber's zone
                EspHubLog->printf("Publishing to remote subscriber: %s (zone: %s)\n",
                                 sub.subscriberDevice.c_str(), sub.subscriberZone.c_str());
            }
        }
    }

    return true;
}

// ============================================================================
// Zone and Routing Information
// ============================================================================

const ZoneInfo& MeshDeviceManager::getZoneInfo() const {
    static ZoneInfo emptyZone;
    if (zoneManager) {
        return zoneManager->getZoneInfo();
    }
    return emptyZone;
}

ZoneRouter::RouterStatistics MeshDeviceManager::getRouterStats() const {
    if (zoneRouter) {
        return zoneRouter->getStatistics();
    }
    return ZoneRouter::RouterStatistics();
}

const ZoneStatistics& MeshDeviceManager::getZoneStats() const {
    static ZoneStatistics emptyStats;
    if (zoneManager) {
        return zoneManager->getStatistics();
    }
    return emptyStats;
}

std::vector<String> MeshDeviceManager::getKnownZones() {
    if (zoneRouter) {
        return zoneRouter->getKnownZones();
    }
    return std::vector<String>();
}

size_t MeshDeviceManager::getMemoryUsage() const {
    size_t total = 0;
    if (zoneManager) {
        total += zoneManager->getMemoryUsage();
    }
    if (zoneRouter) {
        total += zoneRouter->getRouteCount() * 50; // Approximate size per route
    }
    return total;
}

// ============================================================================
// Legacy API (for backward compatibility)
// ============================================================================

void MeshDeviceManager::addDevice(uint32_t nodeId, const String& name) {
    if (legacyDevices.count(nodeId)) {
        EspHubLog->printf("Device %u already registered.\n", nodeId);
        return;
    }
    MeshDevice newDevice;
    newDevice.nodeId = nodeId;
    newDevice.name = name;
    newDevice.lastSeen = millis();
    newDevice.isOnline = true;
    legacyDevices[nodeId] = newDevice;
    EspHubLog->printf("Registered legacy mesh device: %u (%s)\n", nodeId, name.c_str());
}

void MeshDeviceManager::updateDeviceLastSeen(uint32_t nodeId) {
    if (legacyDevices.count(nodeId)) {
        legacyDevices[nodeId].lastSeen = millis();
        if (!legacyDevices[nodeId].isOnline) {
            legacyDevices[nodeId].isOnline = true;
            EspHubLog->printf("Device %u (%s) is back online.\n",
                             nodeId, legacyDevices[nodeId].name.c_str());
        }
    } else {
        EspHubLog->printf("WARNING: Heartbeat from unknown device %u\n", nodeId);
    }
}

MeshDevice* MeshDeviceManager::getDevice(uint32_t nodeId) {
    if (legacyDevices.count(nodeId)) {
        return &legacyDevices[nodeId];
    }
    return nullptr;
}

void MeshDeviceManager::checkOfflineDevices(unsigned long offlineTimeoutMs) {
    unsigned long currentMillis = millis();
    for (auto& pair : legacyDevices) {
        MeshDevice& device = pair.second;
        if (device.isOnline && (currentMillis - device.lastSeen > offlineTimeoutMs)) {
            device.isOnline = false;
            EspHubLog->printf("Device %u (%s) is offline.\n", device.nodeId, device.name.c_str());
        }
    }
}

// ============================================================================
// Integration Hooks
// ============================================================================

void MeshDeviceManager::setDeviceRegistry(DeviceRegistry* registry) {
    deviceRegistry = registry;
}

// ============================================================================
// Helper Methods
// ============================================================================

void MeshDeviceManager::syncDevicesFromZone() {
    // Periodically sync zone devices to legacy format for backward compatibility
    // This is a lightweight operation, only needed for legacy API support
}

MeshDevice MeshDeviceManager::zoneDeviceToMeshDevice(const ZoneDevice& zoneDevice) {
    MeshDevice meshDevice;
    meshDevice.nodeId = 0; // Zone mesh doesn't use nodeId
    meshDevice.name = zoneDevice.deviceName;
    meshDevice.lastSeen = zoneDevice.lastSeen;
    meshDevice.isOnline = zoneDevice.isOnline();
    meshDevice.zoneName = zoneManager ? zoneManager->getZoneName() : "";
    return meshDevice;
}
