#include "DeviceConfigManager.h"
#include "DeviceRegistry.h"
#include "Logger.h"
#include <LittleFS.h>

#define DEVICES_DIR "/config/devices"
#define TEMPLATES_DIR "/config/templates"

DeviceConfigManager::DeviceConfigManager() {
}

DeviceConfigManager::~DeviceConfigManager() {
    // Note: We don't own the protocol managers, just hold references
}

void DeviceConfigManager::begin() {
    LOG_INFO("DeviceConfigManager", "Initializing...");

    // Ensure config directories exist
    if (!ensureConfigDirectory()) {
        LOG_ERROR("DeviceConfigManager", "Failed to create config directories");
    }

    LOG_INFO("DeviceConfigManager", "Ready");
}

// ============================================================================
// Protocol Manager Registration
// ============================================================================

void DeviceConfigManager::registerProtocolManager(const String& protocolName, ProtocolManagerInterface* manager) {
    if (manager == nullptr) {
        LOG_ERROR("DeviceConfigManager", "Cannot register null protocol manager for: " + protocolName);
        return;
    }

    String lowerProtocol = protocolName;
    lowerProtocol.toLowerCase();

    protocolManagers[lowerProtocol] = manager;
    LOG_INFO("DeviceConfigManager", "Registered protocol manager: " + protocolName);
}

ProtocolManagerInterface* DeviceConfigManager::getProtocolManager(const String& protocolName) {
    String lowerProtocol = protocolName;
    lowerProtocol.toLowerCase();

    auto it = protocolManagers.find(lowerProtocol);
    if (it != protocolManagers.end()) {
        return it->second;
    }
    return nullptr;
}

// ============================================================================
// Device Configuration Loading
// ============================================================================

bool DeviceConfigManager::loadDevice(const JsonObject& config) {
    if (!validateDeviceConfig(config)) {
        LOG_ERROR("DeviceConfigManager", "Invalid device configuration");
        return false;
    }

    String deviceId = config["device_id"] | "";

    // Store the configuration
    JsonDocument doc;
    JsonObject root = doc.to<JsonObject>();

    // Deep copy the config
    for (JsonPair kv : config) {
        root[kv.key()] = kv.value();
    }

    deviceConfigs[deviceId] = doc;

    // Initialize device connection via protocol manager
    if (!initializeDeviceConnection(deviceId, config)) {
        LOG_ERROR("DeviceConfigManager", "Failed to initialize device: " + deviceId);
        deviceConfigs.erase(deviceId);
        return false;
    }

    LOG_INFO("DeviceConfigManager", "Loaded device: " + deviceId);
    return true;
}

bool DeviceConfigManager::loadDeviceFromFile(const String& filepath) {
    LOG_INFO("DeviceConfigManager", "Loading device from: " + filepath);

    if (!LittleFS.exists(filepath)) {
        LOG_ERROR("DeviceConfigManager", "File not found: " + filepath);
        return false;
    }

    File file = LittleFS.open(filepath, "r");
    if (!file) {
        LOG_ERROR("DeviceConfigManager", "Failed to open file: " + filepath);
        return false;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) {
        LOG_ERROR("DeviceConfigManager", "Failed to parse JSON: " + String(error.c_str()));
        return false;
    }

    return loadDevice(doc.to<JsonObject>());
}

bool DeviceConfigManager::loadAllDevices() {
    LOG_INFO("DeviceConfigManager", "Loading all devices from: " + String(DEVICES_DIR));

    if (!LittleFS.exists(DEVICES_DIR)) {
        LOG_WARN("DeviceConfigManager", "Devices directory does not exist");
        return true; // Not an error, just no devices yet
    }

    File root = LittleFS.open(DEVICES_DIR);
    if (!root || !root.isDirectory()) {
        LOG_ERROR("DeviceConfigManager", "Failed to open devices directory");
        return false;
    }

    int loaded = 0;
    int failed = 0;

    File file = root.openNextFile();
    while (file) {
        String filename = file.name();
        file.close();

        // Only process .json files
        if (filename.endsWith(".json")) {
            String fullPath = String(DEVICES_DIR) + "/" + filename;
            if (loadDeviceFromFile(fullPath)) {
                loaded++;
            } else {
                failed++;
            }
        }

        file = root.openNextFile();
    }

    root.close();

    LOG_INFO("DeviceConfigManager", "Loaded " + String(loaded) + " devices, " + String(failed) + " failed");
    return (failed == 0);
}

// ============================================================================
// Device Configuration Saving
// ============================================================================

bool DeviceConfigManager::saveDevice(const String& deviceId) {
    String filepath = getDeviceFilePath(deviceId);
    return saveDeviceToFile(deviceId, filepath);
}

bool DeviceConfigManager::saveDeviceToFile(const String& deviceId, const String& filepath) {
    if (!hasDevice(deviceId)) {
        LOG_ERROR("DeviceConfigManager", "Device not found: " + deviceId);
        return false;
    }

    // Ensure directory exists
    if (!ensureConfigDirectory()) {
        LOG_ERROR("DeviceConfigManager", "Failed to create config directory");
        return false;
    }

    File file = LittleFS.open(filepath, "w");
    if (!file) {
        LOG_ERROR("DeviceConfigManager", "Failed to open file for writing: " + filepath);
        return false;
    }

    size_t written = serializeJsonPretty(deviceConfigs[deviceId], file);
    file.close();

    if (written == 0) {
        LOG_ERROR("DeviceConfigManager", "Failed to write JSON to file");
        return false;
    }

    LOG_INFO("DeviceConfigManager", "Saved device: " + deviceId + " to " + filepath);
    return true;
}

bool DeviceConfigManager::saveAllDevices() {
    LOG_INFO("DeviceConfigManager", "Saving all devices...");

    int saved = 0;
    int failed = 0;

    for (const auto& pair : deviceConfigs) {
        if (saveDevice(pair.first)) {
            saved++;
        } else {
            failed++;
        }
    }

    LOG_INFO("DeviceConfigManager", "Saved " + String(saved) + " devices, " + String(failed) + " failed");
    return (failed == 0);
}

// ============================================================================
// Device Lifecycle
// ============================================================================

bool DeviceConfigManager::createDevice(const JsonObject& config) {
    if (!loadDevice(config)) {
        return false;
    }

    String deviceId = config["device_id"] | "";
    return saveDevice(deviceId);
}

bool DeviceConfigManager::updateDevice(const String& deviceId, const JsonObject& config) {
    if (!hasDevice(deviceId)) {
        LOG_ERROR("DeviceConfigManager", "Device not found: " + deviceId);
        return false;
    }

    // Remove old device from protocol manager
    String oldProtocol = getDeviceProtocol(deviceId);
    ProtocolManagerInterface* oldManager = getProtocolManager(oldProtocol);
    if (oldManager) {
        oldManager->removeDevice(deviceId);
    }

    // Load new configuration
    if (!loadDevice(config)) {
        // Try to restore old device
        JsonDocument& oldConfig = deviceConfigs[deviceId];
        initializeDeviceConnection(deviceId, oldConfig.to<JsonObject>());
        return false;
    }

    return saveDevice(deviceId);
}

bool DeviceConfigManager::deleteDevice(const String& deviceId) {
    if (!hasDevice(deviceId)) {
        LOG_ERROR("DeviceConfigManager", "Device not found: " + deviceId);
        return false;
    }

    // Remove from protocol manager
    String protocol = getDeviceProtocol(deviceId);
    ProtocolManagerInterface* manager = getProtocolManager(protocol);
    if (manager) {
        manager->removeDevice(deviceId);
    }

    // Remove from DeviceRegistry
    DeviceRegistry& registry = DeviceRegistry::getInstance();
    std::vector<String> endpoints = getDeviceEndpoints(deviceId);
    for (const String& endpoint : endpoints) {
        String fullEndpointName = deviceId + "." + endpoint;
        registry.removeEndpoint(fullEndpointName);
    }

    // Remove config file
    String filepath = getDeviceFilePath(deviceId);
    if (LittleFS.exists(filepath)) {
        LittleFS.remove(filepath);
    }

    // Remove from memory
    deviceConfigs.erase(deviceId);

    LOG_INFO("DeviceConfigManager", "Deleted device: " + deviceId);
    return true;
}

// ============================================================================
// Device Access
// ============================================================================

bool DeviceConfigManager::hasDevice(const String& deviceId) const {
    return deviceConfigs.find(deviceId) != deviceConfigs.end();
}

JsonDocument DeviceConfigManager::getDeviceConfig(const String& deviceId) {
    JsonDocument doc;

    if (!hasDevice(deviceId)) {
        return doc;
    }

    // Create a copy
    JsonObject root = doc.to<JsonObject>();
    JsonObject source = deviceConfigs[deviceId].to<JsonObject>();

    for (JsonPair kv : source) {
        root[kv.key()] = kv.value();
    }

    return doc;
}

String DeviceConfigManager::getDeviceProtocol(const String& deviceId) {
    if (!hasDevice(deviceId)) {
        return "";
    }

    JsonObject config = deviceConfigs[deviceId].to<JsonObject>();
    return config["protocol"] | "";
}

String DeviceConfigManager::getDeviceLocation(const String& deviceId) {
    if (!hasDevice(deviceId)) {
        return "";
    }

    JsonObject config = deviceConfigs[deviceId].to<JsonObject>();
    return config["location"] | "";
}

String DeviceConfigManager::getDeviceFriendlyName(const String& deviceId) {
    if (!hasDevice(deviceId)) {
        return "";
    }

    JsonObject config = deviceConfigs[deviceId].to<JsonObject>();
    return config["friendly_name"] | deviceId;
}

// ============================================================================
// Device Queries
// ============================================================================

std::vector<String> DeviceConfigManager::getAllDeviceIds() const {
    std::vector<String> ids;
    ids.reserve(deviceConfigs.size());

    for (const auto& pair : deviceConfigs) {
        ids.push_back(pair.first);
    }

    return ids;
}

std::vector<String> DeviceConfigManager::getDevicesByProtocol(const String& protocol) const {
    std::vector<String> result;

    String lowerProtocol = protocol;
    lowerProtocol.toLowerCase();

    for (const auto& pair : deviceConfigs) {
        JsonObjectConst config = pair.second.as<JsonObjectConst>();
        String deviceProtocol = config["protocol"] | "";
        deviceProtocol.toLowerCase();

        if (deviceProtocol == lowerProtocol) {
            result.push_back(pair.first);
        }
    }

    return result;
}

std::vector<String> DeviceConfigManager::getDevicesByLocation(const String& location) const {
    std::vector<String> result;

    String lowerLocation = location;
    lowerLocation.toLowerCase();

    for (const auto& pair : deviceConfigs) {
        JsonObjectConst config = pair.second.as<JsonObjectConst>();
        String deviceLocation = config["location"] | "";
        deviceLocation.toLowerCase();

        if (deviceLocation == lowerLocation) {
            result.push_back(pair.first);
        }
    }

    return result;
}

std::vector<String> DeviceConfigManager::getDevicesByTag(const String& tag) const {
    std::vector<String> result;

    String lowerTag = tag;
    lowerTag.toLowerCase();

    for (const auto& pair : deviceConfigs) {
        JsonObjectConst config = pair.second.as<JsonObjectConst>();
        JsonObjectConst metadata = config["metadata"];
        JsonArrayConst tags = metadata["tags"];

        if (tags) {
            for (JsonVariantConst v : tags) {
                String deviceTag = v.as<String>();
                deviceTag.toLowerCase();

                if (deviceTag == lowerTag) {
                    result.push_back(pair.first);
                    break;
                }
            }
        }
    }

    return result;
}

// ============================================================================
// Endpoint Queries
// ============================================================================

std::vector<String> DeviceConfigManager::getDeviceEndpoints(const String& deviceId) const {
    std::vector<String> result;

    if (!hasDevice(deviceId)) {
        return result;
    }

    JsonObjectConst config = deviceConfigs.at(deviceId).as<JsonObjectConst>();
    JsonArrayConst endpoints = config["endpoints"];

    if (endpoints) {
        for (JsonObjectConst ep : endpoints) {
            String name = ep["name"] | "";
            if (name.length() > 0) {
                result.push_back(name);
            }
        }
    }

    return result;
}

JsonObject DeviceConfigManager::getEndpointConfig(const String& deviceId, const String& endpointName) {
    JsonDocument emptyDoc;
    JsonObject empty = emptyDoc.to<JsonObject>();

    if (!hasDevice(deviceId)) {
        return empty;
    }

    return findEndpointInConfig(deviceConfigs[deviceId], endpointName);
}

// ============================================================================
// IO Operations
// ============================================================================

bool DeviceConfigManager::readEndpoint(const String& deviceId, const String& endpointName, PlcValue& value) {
    if (!hasDevice(deviceId)) {
        LOG_ERROR("DeviceConfigManager", "Device not found: " + deviceId);
        return false;
    }

    // Get protocol manager
    String protocol = getDeviceProtocol(deviceId);
    ProtocolManagerInterface* manager = getProtocolManager(protocol);
    if (!manager) {
        LOG_ERROR("DeviceConfigManager", "No protocol manager for: " + protocol);
        return false;
    }

    // Get endpoint config
    JsonObject endpointConfig = getEndpointConfig(deviceId, endpointName);
    if (endpointConfig.isNull()) {
        LOG_ERROR("DeviceConfigManager", "Endpoint not found: " + endpointName);
        return false;
    }

    // Delegate to protocol manager
    return manager->readEndpoint(deviceId, endpointConfig, value);
}

bool DeviceConfigManager::writeEndpoint(const String& deviceId, const String& endpointName, const PlcValue& value) {
    if (!hasDevice(deviceId)) {
        LOG_ERROR("DeviceConfigManager", "Device not found: " + deviceId);
        return false;
    }

    // Get protocol manager
    String protocol = getDeviceProtocol(deviceId);
    ProtocolManagerInterface* manager = getProtocolManager(protocol);
    if (!manager) {
        LOG_ERROR("DeviceConfigManager", "No protocol manager for: " + protocol);
        return false;
    }

    // Get endpoint config
    JsonObject endpointConfig = getEndpointConfig(deviceId, endpointName);
    if (endpointConfig.isNull()) {
        LOG_ERROR("DeviceConfigManager", "Endpoint not found: " + endpointName);
        return false;
    }

    // Check if endpoint is writable
    String access = endpointConfig["access"] | "r";
    if (access != "w" && access != "rw") {
        LOG_ERROR("DeviceConfigManager", "Endpoint not writable: " + endpointName);
        return false;
    }

    // Delegate to protocol manager
    return manager->writeEndpoint(deviceId, endpointConfig, value);
}

bool DeviceConfigManager::readAllEndpoints(const String& deviceId) {
    if (!hasDevice(deviceId)) {
        LOG_ERROR("DeviceConfigManager", "Device not found: " + deviceId);
        return false;
    }

    std::vector<String> endpoints = getDeviceEndpoints(deviceId);
    bool allSuccess = true;

    for (const String& endpointName : endpoints) {
        JsonObject endpointConfig = getEndpointConfig(deviceId, endpointName);
        String access = endpointConfig["access"] | "r";

        // Only read readable endpoints
        if (access == "r" || access == "rw") {
            PlcValue value;
            if (!readEndpoint(deviceId, endpointName, value)) {
                allSuccess = false;
            }
        }
    }

    return allSuccess;
}

// ============================================================================
// Testing
// ============================================================================

bool DeviceConfigManager::testDeviceConnection(const String& deviceId) {
    if (!hasDevice(deviceId)) {
        LOG_ERROR("DeviceConfigManager", "Device not found: " + deviceId);
        return false;
    }

    String protocol = getDeviceProtocol(deviceId);
    ProtocolManagerInterface* manager = getProtocolManager(protocol);
    if (!manager) {
        LOG_ERROR("DeviceConfigManager", "No protocol manager for: " + protocol);
        return false;
    }

    JsonObject config = deviceConfigs[deviceId].to<JsonObject>();
    JsonObject connectionConfig = config["connection"];

    return manager->testConnection(connectionConfig);
}

bool DeviceConfigManager::testDeviceConnection(const JsonObject& config) {
    if (!validateDeviceConfig(config)) {
        LOG_ERROR("DeviceConfigManager", "Invalid device configuration");
        return false;
    }

    String protocol = config["protocol"] | "";
    ProtocolManagerInterface* manager = getProtocolManager(protocol);
    if (!manager) {
        LOG_ERROR("DeviceConfigManager", "No protocol manager for: " + protocol);
        return false;
    }

    JsonObject connectionConfig = config["connection"];
    return manager->testConnection(connectionConfig);
}

bool DeviceConfigManager::testEndpoint(const String& deviceId, const String& endpointName) {
    if (!hasDevice(deviceId)) {
        LOG_ERROR("DeviceConfigManager", "Device not found: " + deviceId);
        return false;
    }

    String protocol = getDeviceProtocol(deviceId);
    ProtocolManagerInterface* manager = getProtocolManager(protocol);
    if (!manager) {
        LOG_ERROR("DeviceConfigManager", "No protocol manager for: " + protocol);
        return false;
    }

    JsonObject endpointConfig = getEndpointConfig(deviceId, endpointName);
    if (endpointConfig.isNull()) {
        LOG_ERROR("DeviceConfigManager", "Endpoint not found: " + endpointName);
        return false;
    }

    return manager->testEndpoint(deviceId, endpointConfig);
}

// ============================================================================
// Device Status
// ============================================================================

bool DeviceConfigManager::isDeviceOnline(const String& deviceId) {
    if (!hasDevice(deviceId)) {
        return false;
    }

    String protocol = getDeviceProtocol(deviceId);
    ProtocolManagerInterface* manager = getProtocolManager(protocol);
    if (!manager) {
        return false;
    }

    return manager->isDeviceOnline(deviceId);
}

void DeviceConfigManager::updateDeviceStatus(const String& deviceId, bool online) {
    if (!hasDevice(deviceId)) {
        return;
    }

    // Update status in config
    JsonObject config = deviceConfigs[deviceId].to<JsonObject>();
    if (!config["status"]) {
        config.createNestedObject("status");
    }

    JsonObject status = config["status"];
    status["online"] = online;
    status["last_seen"] = millis();
}

// ============================================================================
// Template Management
// ============================================================================

bool DeviceConfigManager::loadTemplate(const String& templateId, JsonDocument& templateDoc) {
    String filepath = String(TEMPLATES_DIR) + "/" + templateId + ".json";

    if (!LittleFS.exists(filepath)) {
        LOG_ERROR("DeviceConfigManager", "Template not found: " + templateId);
        return false;
    }

    File file = LittleFS.open(filepath, "r");
    if (!file) {
        LOG_ERROR("DeviceConfigManager", "Failed to open template: " + filepath);
        return false;
    }

    DeserializationError error = deserializeJson(templateDoc, file);
    file.close();

    if (error) {
        LOG_ERROR("DeviceConfigManager", "Failed to parse template JSON: " + String(error.c_str()));
        return false;
    }

    return true;
}

std::vector<String> DeviceConfigManager::getAvailableTemplates() {
    std::vector<String> templates;

    if (!LittleFS.exists(TEMPLATES_DIR)) {
        return templates;
    }

    File root = LittleFS.open(TEMPLATES_DIR);
    if (!root || !root.isDirectory()) {
        return templates;
    }

    File file = root.openNextFile();
    while (file) {
        String filename = file.name();
        file.close();

        if (filename.endsWith(".json")) {
            // Remove .json extension
            String templateId = filename.substring(0, filename.length() - 5);
            templates.push_back(templateId);
        }

        file = root.openNextFile();
    }

    root.close();
    return templates;
}

// ============================================================================
// Statistics
// ============================================================================

DeviceConfigManager::DeviceStats DeviceConfigManager::getStatistics() {
    DeviceStats stats;
    stats.totalDevices = deviceConfigs.size();
    stats.onlineDevices = 0;
    stats.offlineDevices = 0;

    for (const auto& pair : deviceConfigs) {
        String deviceId = pair.first;
        JsonObjectConst config = pair.second.as<JsonObjectConst>();

        // Count online/offline
        if (isDeviceOnline(deviceId)) {
            stats.onlineDevices++;
        } else {
            stats.offlineDevices++;
        }

        // Count by protocol
        String protocol = config["protocol"] | "unknown";
        stats.devicesByProtocol[protocol]++;

        // Count by location
        String location = config["location"] | "unknown";
        stats.devicesByLocation[location]++;
    }

    return stats;
}

// ============================================================================
// Helper Methods
// ============================================================================

bool DeviceConfigManager::validateDeviceConfig(const JsonObject& config) {
    // Required fields
    if (!config.containsKey("device_id") || !config.containsKey("protocol")) {
        LOG_ERROR("DeviceConfigManager", "Missing required fields: device_id or protocol");
        return false;
    }

    String deviceId = config["device_id"] | "";
    String protocol = config["protocol"] | "";

    if (deviceId.length() == 0 || protocol.length() == 0) {
        LOG_ERROR("DeviceConfigManager", "Empty device_id or protocol");
        return false;
    }

    // Check if protocol manager exists
    if (getProtocolManager(protocol) == nullptr) {
        LOG_ERROR("DeviceConfigManager", "No protocol manager registered for: " + protocol);
        return false;
    }

    // Must have connection config
    if (!config.containsKey("connection")) {
        LOG_ERROR("DeviceConfigManager", "Missing connection configuration");
        return false;
    }

    // Must have at least one endpoint
    JsonArray endpoints = config["endpoints"];
    if (!endpoints || endpoints.size() == 0) {
        LOG_ERROR("DeviceConfigManager", "No endpoints defined");
        return false;
    }

    // Validate endpoints
    for (JsonObject ep : endpoints) {
        if (!ep.containsKey("name") || !ep.containsKey("type") || !ep.containsKey("access")) {
            LOG_ERROR("DeviceConfigManager", "Endpoint missing required fields");
            return false;
        }
    }

    return true;
}

String DeviceConfigManager::generateDeviceFilename(const String& deviceId) {
    String filename = deviceId;
    filename.replace("/", "_");
    filename.replace("\\", "_");
    filename.replace(" ", "_");
    return filename + ".json";
}

bool DeviceConfigManager::initializeDeviceConnection(const String& deviceId, const JsonObject& config) {
    String protocol = config["protocol"] | "";
    ProtocolManagerInterface* manager = getProtocolManager(protocol);

    if (!manager) {
        LOG_ERROR("DeviceConfigManager", "No protocol manager for: " + protocol);
        return false;
    }

    JsonObject connectionConfig = config["connection"];
    if (!manager->initializeDevice(deviceId, connectionConfig)) {
        LOG_ERROR("DeviceConfigManager", "Failed to initialize device connection: " + deviceId);
        return false;
    }

    return true;
}

JsonObject DeviceConfigManager::findEndpointInConfig(JsonDocument& deviceConfig, const String& endpointName) {
    JsonDocument emptyDoc;
    JsonObject empty = emptyDoc.to<JsonObject>();

    JsonObject config = deviceConfig.to<JsonObject>();
    JsonArray endpoints = config["endpoints"];

    if (!endpoints) {
        return empty;
    }

    for (JsonObject ep : endpoints) {
        String name = ep["name"] | "";
        if (name == endpointName) {
            return ep;
        }
    }

    return empty;
}

// ============================================================================
// File Operations
// ============================================================================

bool DeviceConfigManager::ensureConfigDirectory() {
    if (!LittleFS.exists("/config")) {
        if (!LittleFS.mkdir("/config")) {
            LOG_ERROR("DeviceConfigManager", "Failed to create /config directory");
            return false;
        }
    }

    if (!LittleFS.exists(DEVICES_DIR)) {
        if (!LittleFS.mkdir(DEVICES_DIR)) {
            LOG_ERROR("DeviceConfigManager", "Failed to create devices directory");
            return false;
        }
    }

    if (!LittleFS.exists(TEMPLATES_DIR)) {
        if (!LittleFS.mkdir(TEMPLATES_DIR)) {
            LOG_ERROR("DeviceConfigManager", "Failed to create templates directory");
            return false;
        }
    }

    return true;
}

String DeviceConfigManager::getDeviceFilePath(const String& deviceId) {
    String filename = generateDeviceFilename(deviceId);
    return String(DEVICES_DIR) + "/" + filename;
}
