#include "ZoneManager.h"

ZoneManager* ZoneManager::instance = nullptr;

// ============================================================================
// Initialization and Configuration
// ============================================================================

ZoneManager::ZoneManager()
    : myRole(ZoneRole::UNASSIGNED),
      lastBeaconTime(0),
      lastElectionTime(0),
      lastCleanupTime(0),
      electionInProgress(false),
      electionStartTime(0) {
    memset(myMacAddress, 0, 6);
    instance = this;
}

ZoneManager::~ZoneManager() {
    esp_now_deinit();
    instance = nullptr;
}

void ZoneManager::begin(const String& deviceName, const String& preferredZone) {
    myDeviceName = deviceName;
    currentZone.zoneName = preferredZone;

    // Get MAC address
    WiFi.macAddress(myMacAddress);

    // Initialize ESP-NOW
    WiFi.mode(WIFI_STA);
    if (esp_now_init() != ESP_OK) {
        EspHubLog->println("ERROR: ESP-NOW init failed");
        return;
    }

    // Register receive callback
    esp_now_register_recv_cb(ZoneManager::onDataReceived);

    // Set default capabilities
    myCapabilities.freeRam = ESP.getFreeHeap();
    myCapabilities.uptime = millis() / 1000;
    myCapabilities.currentLoad = 0;
    myCapabilities.deviceCount = 0;
    myCapabilities.hasExternalPower = true; // Assume AC power by default

    EspHubLog->printf("ZoneManager: Initialized for device '%s' in zone '%s'\n",
                     myDeviceName.c_str(), currentZone.zoneName.c_str());

    // Start coordinator election
    triggerElection();
}

void ZoneManager::loop() {
    unsigned long now = millis();

    // Update capabilities
    myCapabilities.freeRam = ESP.getFreeHeap();
    myCapabilities.uptime = now / 1000;

    // Send periodic beacons
    if (myRole == ZoneRole::COORDINATOR) {
        if (now - lastBeaconTime >= COORDINATOR_BEACON_INTERVAL) {
            sendCoordinatorBeacon();
            lastBeaconTime = now;
        }
    } else if (myRole == ZoneRole::MEMBER) {
        if (now - lastBeaconTime >= DEVICE_BEACON_INTERVAL) {
            sendDeviceBeacon();
            lastBeaconTime = now;
        }
    }

    // Check election timeout
    if (electionInProgress && (now - electionStartTime > COORDINATOR_ELECTION_TIMEOUT)) {
        // Election complete - determine winner
        uint32_t bestScore = myCapabilities.calculateScore();
        bool iAmBest = true;

        for (const auto& candidate : electionCandidates) {
            if (candidate.calculateScore() > bestScore) {
                iAmBest = false;
                break;
            }
        }

        if (iAmBest) {
            becomeCoordinator();
        } else {
            becomeMember();
        }

        electionInProgress = false;
        electionCandidates.clear();
    }

    // Cleanup offline devices (every 60 seconds)
    if (myRole == ZoneRole::COORDINATOR && (now - lastCleanupTime > 60000)) {
        removeOfflineDevices();
        lastCleanupTime = now;
    }

    // Check if coordinator is still alive (members only)
    if (myRole == ZoneRole::MEMBER) {
        if (now - currentZone.lastCoordinatorBeacon > 120000) {
            EspHubLog->println("ZoneManager: Coordinator timeout, triggering election");
            triggerElection();
        }
    }
}

void ZoneManager::setCapabilities(const CoordinatorCapabilities& caps) {
    myCapabilities = caps;
}

size_t ZoneManager::getMemoryUsage() const {
    return currentZone.getMemoryUsage();
}

// ============================================================================
// Coordinator Election
// ============================================================================

void ZoneManager::triggerElection() {
    if (electionInProgress) {
        return; // Election already in progress
    }

    EspHubLog->printf("ZoneManager: Starting coordinator election for zone '%s'\n",
                     currentZone.zoneName.c_str());

    startElection();
}

void ZoneManager::startElection() {
    electionInProgress = true;
    electionStartTime = millis();
    electionCandidates.clear();
    myRole = ZoneRole::CANDIDATE;

    // Broadcast election vote with my capabilities
    JsonDocument doc;
    doc["deviceName"] = myDeviceName;
    doc["freeRam"] = myCapabilities.freeRam;
    doc["uptime"] = myCapabilities.uptime;
    doc["load"] = myCapabilities.currentLoad;
    doc["deviceCount"] = myCapabilities.deviceCount;
    doc["externalPower"] = myCapabilities.hasExternalPower;
    doc["rssiAvg"] = myCapabilities.rssiAverage;
    doc["score"] = myCapabilities.calculateScore();

    String payload;
    serializeJson(doc, payload);

    ZoneMeshHeader header;
    fillHeader(header, ZoneMeshPacketType::ELECTION_VOTE, nullptr, nullptr);

    broadcastToZone(ZoneMeshPacketType::ELECTION_VOTE,
                   (const uint8_t*)payload.c_str(), payload.length());

    stats.lastElection = millis();
}

void ZoneManager::processElectionVote(const JsonObject& payload) {
    if (!electionInProgress) {
        return;
    }

    CoordinatorCapabilities candidate;
    candidate.freeRam = payload["freeRam"] | 0;
    candidate.uptime = payload["uptime"] | 0;
    candidate.currentLoad = payload["load"] | 0;
    candidate.deviceCount = payload["deviceCount"] | 0;
    candidate.hasExternalPower = payload["externalPower"] | false;
    candidate.rssiAverage = payload["rssiAvg"] | -80;

    electionCandidates.push_back(candidate);

    EspHubLog->printf("ZoneManager: Election vote received, score=%u, candidates=%d\n",
                     candidate.calculateScore(), electionCandidates.size());
}

void ZoneManager::becomeCoordinator() {
    if (myRole == ZoneRole::COORDINATOR) {
        return; // Already coordinator
    }

    myRole = ZoneRole::COORDINATOR;
    currentZone.coordinatorDevice = myDeviceName;
    memcpy(currentZone.coordinatorMac, myMacAddress, 6);
    currentZone.lastCoordinatorBeacon = millis();

    EspHubLog->printf("ZoneManager: Became COORDINATOR for zone '%s'\n",
                     currentZone.zoneName.c_str());

    // Send election result to all members
    JsonDocument doc;
    doc["coordinator"] = myDeviceName;
    doc["zoneName"] = currentZone.zoneName;

    String payload;
    serializeJson(doc, payload);

    broadcastToZone(ZoneMeshPacketType::ELECTION_RESULT,
                   (const uint8_t*)payload.c_str(), payload.length());

    // Send immediate beacon
    sendCoordinatorBeacon();

    stats.coordinatorChanges++;
}

void ZoneManager::becomeMember() {
    myRole = ZoneRole::MEMBER;

    EspHubLog->printf("ZoneManager: Became MEMBER of zone '%s'\n",
                     currentZone.zoneName.c_str());

    // Send device beacon
    sendDeviceBeacon();
}

void ZoneManager::processElectionResult(const JsonObject& payload) {
    String coordinatorName = payload["coordinator"] | "";
    String zoneName = payload["zoneName"] | "";

    if (coordinatorName.isEmpty()) {
        return;
    }

    currentZone.coordinatorDevice = coordinatorName;
    currentZone.zoneName = zoneName;
    currentZone.lastCoordinatorBeacon = millis();

    if (coordinatorName != myDeviceName && myRole != ZoneRole::MEMBER) {
        becomeMember();
    }

    EspHubLog->printf("ZoneManager: Election result - coordinator is '%s'\n",
                     coordinatorName.c_str());
}

// ============================================================================
// Device Discovery and Beacons
// ============================================================================

void ZoneManager::sendBeacon() {
    if (myRole == ZoneRole::COORDINATOR) {
        sendCoordinatorBeacon();
    } else {
        sendDeviceBeacon();
    }
}

void ZoneManager::sendCoordinatorBeacon() {
    JsonDocument doc;
    doc["type"] = "coordinator_beacon";
    doc["coordinator"] = myDeviceName;
    doc["zoneName"] = currentZone.zoneName;
    doc["deviceCount"] = currentZone.devices.size();
    doc["subscriptionCount"] = currentZone.subscriptionCount;
    doc["freeRam"] = myCapabilities.freeRam;

    String payload;
    serializeJson(doc, payload);

    ZoneMeshHeader header;
    fillHeader(header, ZoneMeshPacketType::COORDINATOR_BEACON, nullptr, nullptr);

    broadcastToZone(ZoneMeshPacketType::COORDINATOR_BEACON,
                   (const uint8_t*)payload.c_str(), payload.length());

    currentZone.lastCoordinatorBeacon = millis();
    stats.packetsSent++;
}

void ZoneManager::sendDeviceBeacon() {
    JsonDocument doc;
    doc["type"] = "device_beacon";
    doc["deviceName"] = myDeviceName;
    doc["zoneName"] = currentZone.zoneName;
    doc["role"] = "member";

    String payload;
    serializeJson(doc, payload);

    ZoneMeshHeader header;
    fillHeader(header, ZoneMeshPacketType::DEVICE_BEACON, nullptr, nullptr);

    broadcastToZone(ZoneMeshPacketType::DEVICE_BEACON,
                   (const uint8_t*)payload.c_str(), payload.length());

    stats.packetsSent++;
}

void ZoneManager::processBeacon(const ZoneMeshHeader& header, const JsonObject& payload) {
    String deviceName = payload["deviceName"] | payload["coordinator"] | "";
    String zoneName = payload["zoneName"] | "";

    if (deviceName.isEmpty()) {
        return;
    }

    // Calculate RSSI (approximation - would need actual RSSI from ESP-NOW)
    int8_t rssi = -50; // Placeholder

    // Add or update device
    addOrUpdateDevice(deviceName, header.sourceMac, rssi);

    // Update coordinator info if this is coordinator beacon
    if (payload["type"] == "coordinator_beacon") {
        currentZone.coordinatorDevice = deviceName;
        memcpy(currentZone.coordinatorMac, header.sourceMac, 6);
        currentZone.lastCoordinatorBeacon = millis();
    }
}

// ============================================================================
// Device Management
// ============================================================================

void ZoneManager::addOrUpdateDevice(const String& deviceName, const uint8_t* mac, int8_t rssi) {
    // Find existing device
    for (auto& device : currentZone.devices) {
        if (device.deviceName == deviceName) {
            // Update existing
            memcpy(device.macAddress, mac, 6);
            device.lastSeen = millis();
            device.rssi = rssi;
            return;
        }
    }

    // Add new device
    if (currentZone.devices.size() >= MAX_DEVICES_PER_ZONE) {
        EspHubLog->printf("WARNING: Zone '%s' is full, cannot add device '%s'\n",
                         currentZone.zoneName.c_str(), deviceName.c_str());
        return;
    }

    ZoneDevice newDevice;
    newDevice.deviceName = deviceName;
    memcpy(newDevice.macAddress, mac, 6);
    newDevice.role = ZoneRole::MEMBER;
    newDevice.lastSeen = millis();
    newDevice.rssi = rssi;

    currentZone.devices.push_back(newDevice);

    EspHubLog->printf("ZoneManager: Added device '%s' to zone '%s'\n",
                     deviceName.c_str(), currentZone.zoneName.c_str());
}

void ZoneManager::removeOfflineDevices() {
    size_t initialCount = currentZone.devices.size();

    currentZone.devices.erase(
        std::remove_if(currentZone.devices.begin(), currentZone.devices.end(),
                      [](const ZoneDevice& dev) { return !dev.isOnline(); }),
        currentZone.devices.end()
    );

    size_t removed = initialCount - currentZone.devices.size();
    if (removed > 0) {
        EspHubLog->printf("ZoneManager: Removed %d offline devices from zone '%s'\n",
                         removed, currentZone.zoneName.c_str());
    }
}

ZoneDevice* ZoneManager::getDevice(const String& deviceName) {
    for (auto& device : currentZone.devices) {
        if (device.deviceName == deviceName) {
            return &device;
        }
    }
    return nullptr;
}

ZoneDevice* ZoneManager::findDeviceByMac(const uint8_t* mac) {
    for (auto& device : currentZone.devices) {
        if (memcmp(device.macAddress, mac, 6) == 0) {
            return &device;
        }
    }
    return nullptr;
}

bool ZoneManager::isDeviceOnline(const String& deviceName) {
    ZoneDevice* device = getDevice(deviceName);
    return device && device->isOnline();
}

// ============================================================================
// Subscription Management (Coordinator Only)
// ============================================================================

bool ZoneManager::addSubscription(const String& publisherEndpoint,
                                  const String& subscriberDevice,
                                  const String& subscriberZone) {
    if (myRole != ZoneRole::COORDINATOR) {
        EspHubLog->println("ERROR: Only coordinator can manage subscriptions");
        return false;
    }

    // Check if subscription already exists
    auto& subscribers = currentZone.subscriptions[publisherEndpoint];
    for (const auto& sub : subscribers) {
        if (sub.subscriberDevice == subscriberDevice) {
            // Already subscribed
            return true;
        }
    }

    // Check subscription limit
    if (subscribers.size() >= MAX_SUBSCRIPTIONS_PER_DEVICE) {
        EspHubLog->printf("ERROR: Subscription limit reached for endpoint '%s'\n",
                         publisherEndpoint.c_str());
        return false;
    }

    // Add new subscription
    SubscriptionEntry newSub;
    newSub.publisherEndpoint = publisherEndpoint;
    newSub.subscriberDevice = subscriberDevice;
    newSub.subscriberZone = subscriberZone;
    newSub.isLocal = (subscriberZone == currentZone.zoneName);
    newSub.lastUpdate = 0;
    newSub.updateInterval = 0;

    subscribers.push_back(newSub);
    currentZone.subscriptionCount++;
    stats.subscriptionChanges++;

    EspHubLog->printf("ZoneManager: Added subscription '%s' -> '%s' (zone: %s)\n",
                     publisherEndpoint.c_str(), subscriberDevice.c_str(),
                     subscriberZone.c_str());

    return true;
}

bool ZoneManager::removeSubscription(const String& publisherEndpoint,
                                     const String& subscriberDevice) {
    if (myRole != ZoneRole::COORDINATOR) {
        return false;
    }

    auto it = currentZone.subscriptions.find(publisherEndpoint);
    if (it == currentZone.subscriptions.end()) {
        return false;
    }

    auto& subscribers = it->second;
    size_t initialSize = subscribers.size();

    subscribers.erase(
        std::remove_if(subscribers.begin(), subscribers.end(),
                      [&subscriberDevice](const SubscriptionEntry& sub) {
                          return sub.subscriberDevice == subscriberDevice;
                      }),
        subscribers.end()
    );

    if (subscribers.size() < initialSize) {
        currentZone.subscriptionCount--;
        stats.subscriptionChanges++;

        // Remove endpoint if no subscribers left
        if (subscribers.empty()) {
            currentZone.subscriptions.erase(it);
        }

        EspHubLog->printf("ZoneManager: Removed subscription '%s' -> '%s'\n",
                         publisherEndpoint.c_str(), subscriberDevice.c_str());
        return true;
    }

    return false;
}

std::vector<SubscriptionEntry> ZoneManager::getSubscribers(const String& publisherEndpoint) {
    auto it = currentZone.subscriptions.find(publisherEndpoint);
    if (it != currentZone.subscriptions.end()) {
        return it->second;
    }
    return std::vector<SubscriptionEntry>();
}

// ============================================================================
// Packet Handling
// ============================================================================

bool ZoneManager::sendPacket(const ZoneMeshHeader& header, const uint8_t* payload, size_t length) {
    // Allocate buffer for header + payload
    size_t totalSize = sizeof(ZoneMeshHeader) + length;
    uint8_t* buffer = new uint8_t[totalSize];

    // Copy header and payload
    memcpy(buffer, &header, sizeof(ZoneMeshHeader));
    memcpy(buffer + sizeof(ZoneMeshHeader), payload, length);

    // Send via ESP-NOW
    esp_err_t result = esp_now_send(header.destMac, buffer, totalSize);

    delete[] buffer;

    if (result == ESP_OK) {
        stats.packetsSent++;
        return true;
    } else {
        stats.packetsDropped++;
        return false;
    }
}

bool ZoneManager::broadcastToZone(ZoneMeshPacketType type, const uint8_t* payload, size_t length) {
    ZoneMeshHeader header;
    fillHeader(header, type, nullptr, nullptr);

    return sendPacket(header, payload, length);
}

bool ZoneManager::routeToZone(const String& targetZone, const ZoneMeshHeader& header,
                              const uint8_t* payload, size_t length) {
    // TODO: Implement inter-zone routing via coordinators
    EspHubLog->printf("ZoneManager: Routing to zone '%s' not yet implemented\n",
                     targetZone.c_str());
    return false;
}

// ============================================================================
// ESP-NOW Callbacks
// ============================================================================

void ZoneManager::onDataReceived(const uint8_t* macAddr, const uint8_t* data, int len) {
    if (instance) {
        instance->handleReceivedPacket(macAddr, data, len);
    }
}

void ZoneManager::handleReceivedPacket(const uint8_t* macAddr, const uint8_t* data, size_t len) {
    if (len < sizeof(ZoneMeshHeader)) {
        stats.packetsDropped++;
        return;
    }

    // Parse header
    ZoneMeshHeader header;
    memcpy(&header, data, sizeof(ZoneMeshHeader));

    // Verify checksum
    const uint8_t* payload = data + sizeof(ZoneMeshHeader);
    size_t payloadLength = len - sizeof(ZoneMeshHeader);

    if (!verifyChecksum(header, payload, payloadLength)) {
        EspHubLog->println("ERROR: Packet checksum mismatch");
        stats.packetsDropped++;
        return;
    }

    stats.packetsReceived++;

    // Parse JSON payload (for most packet types)
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload, payloadLength);

    // Handle packet based on type
    switch (header.type) {
        case ZoneMeshPacketType::COORDINATOR_BEACON:
        case ZoneMeshPacketType::DEVICE_BEACON:
            if (!error) processBeacon(header, doc.as<JsonObject>());
            break;

        case ZoneMeshPacketType::ELECTION_VOTE:
            if (!error) processElectionVote(doc.as<JsonObject>());
            break;

        case ZoneMeshPacketType::ELECTION_RESULT:
            if (!error) processElectionResult(doc.as<JsonObject>());
            break;

        case ZoneMeshPacketType::SUBSCRIBE_REQUEST:
            if (!error) processSubscribeRequest(header, doc.as<JsonObject>());
            break;

        case ZoneMeshPacketType::UNSUBSCRIBE_REQUEST:
            if (!error) processUnsubscribeRequest(header, doc.as<JsonObject>());
            break;

        case ZoneMeshPacketType::DATA_PUBLISH:
            processDataPublish(header, payload, payloadLength);
            break;

        case ZoneMeshPacketType::ZONE_ROUTE:
            processZoneRoute(header, payload, payloadLength);
            break;

        default:
            EspHubLog->printf("WARNING: Unknown packet type: 0x%02X\n", (uint8_t)header.type);
            break;
    }
}

void ZoneManager::processSubscribeRequest(const ZoneMeshHeader& header, const JsonObject& payload) {
    if (myRole != ZoneRole::COORDINATOR) {
        return; // Only coordinator handles subscription requests
    }

    String endpoint = payload["endpoint"] | "";
    String subscriber = payload["subscriber"] | "";
    String subscriberZone = payload["subscriberZone"] | "";

    if (endpoint.isEmpty() || subscriber.isEmpty()) {
        return;
    }

    bool success = addSubscription(endpoint, subscriber, subscriberZone);

    // Send acknowledgment
    JsonDocument ackDoc;
    ackDoc["endpoint"] = endpoint;
    ackDoc["subscriber"] = subscriber;
    ackDoc["success"] = success;

    String ackPayload;
    serializeJson(ackDoc, ackPayload);

    ZoneMeshHeader ackHeader;
    fillHeader(ackHeader, ZoneMeshPacketType::SUBSCRIBE_ACK, header.sourceMac, header.sourceZone);

    sendPacket(ackHeader, (const uint8_t*)ackPayload.c_str(), ackPayload.length());
}

void ZoneManager::processUnsubscribeRequest(const ZoneMeshHeader& header, const JsonObject& payload) {
    if (myRole != ZoneRole::COORDINATOR) {
        return;
    }

    String endpoint = payload["endpoint"] | "";
    String subscriber = payload["subscriber"] | "";

    if (endpoint.isEmpty() || subscriber.isEmpty()) {
        return;
    }

    removeSubscription(endpoint, subscriber);
}

void ZoneManager::processDataPublish(const ZoneMeshHeader& header, const uint8_t* payload, size_t length) {
    // TODO: Forward to subscribers
    // This will be implemented when integrating with MeshDeviceManager
}

void ZoneManager::processZoneRoute(const ZoneMeshHeader& header, const uint8_t* payload, size_t length) {
    // TODO: Handle inter-zone routing
    stats.packetsRouted++;
}

// ============================================================================
// Utilities
// ============================================================================

void ZoneManager::fillHeader(ZoneMeshHeader& header, ZoneMeshPacketType type,
                             const uint8_t* destMac, const char* destZone) {
    header.version = 1;
    header.type = type;
    header.ttl = 10;
    header.flags = 0;

    memcpy(header.sourceMac, myMacAddress, 6);
    strncpy(header.sourceZone, currentZone.zoneName.c_str(), MAX_ZONE_NAME_LENGTH - 1);

    if (destMac) {
        memcpy(header.destMac, destMac, 6);
    } else {
        // Broadcast
        memset(header.destMac, 0xFF, 6);
    }

    if (destZone) {
        strncpy(header.destZone, destZone, MAX_ZONE_NAME_LENGTH - 1);
    } else {
        strncpy(header.destZone, currentZone.zoneName.c_str(), MAX_ZONE_NAME_LENGTH - 1);
    }
}

uint16_t ZoneManager::calculateChecksum(const ZoneMeshHeader& header,
                                       const uint8_t* payload, size_t length) {
    uint16_t checksum = 0;

    // Simple checksum of header + payload
    const uint8_t* headerBytes = (const uint8_t*)&header;
    for (size_t i = 0; i < sizeof(ZoneMeshHeader) - 2; i++) { // Exclude checksum field
        checksum += headerBytes[i];
    }

    for (size_t i = 0; i < length; i++) {
        checksum += payload[i];
    }

    return checksum;
}

bool ZoneManager::verifyChecksum(const ZoneMeshHeader& header,
                                 const uint8_t* payload, size_t length) {
    uint16_t calculated = calculateChecksum(header, payload, length);
    return calculated == header.checksum;
}

void ZoneManager::macToString(const uint8_t* mac, char* str) {
    sprintf(str, "%02X:%02X:%02X:%02X:%02X:%02X",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void ZoneManager::printPacketInfo(const ZoneMeshHeader& header, const char* direction) {
    char srcMac[18], dstMac[18];
    macToString(header.sourceMac, srcMac);
    macToString(header.destMac, dstMac);

    EspHubLog->printf("ZoneMesh %s: type=0x%02X, src=%s (%s), dst=%s (%s), len=%u\n",
                     direction,
                     (uint8_t)header.type,
                     srcMac, header.sourceZone,
                     dstMac, header.destZone,
                     header.payloadLength);
}
