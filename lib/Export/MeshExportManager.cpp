#include "../Export/MeshExportManager.h"
#include "../Core/Logger.h"
#include "../Protocols/Mesh/mesh_protocol.h"
#include <LittleFS.h>

MeshExportManager::MeshExportManager()
    : mesh(nullptr),
      variableRegistry(nullptr),
      plcEngine(nullptr),
      localHubId("") {
    memset(&stats, 0, sizeof(stats));
}

MeshExportManager::~MeshExportManager() {
}

void MeshExportManager::begin() {
    LOG_INFO("MeshExportManager", "Initializing...");
    publishRules.clear();
    subscribeRules.clear();
    memset(&stats, 0, sizeof(stats));
    LOG_INFO("MeshExportManager", "Initialized");
}

void MeshExportManager::loop() {
    // Periodic auto-publish of variables
    static unsigned long lastCheck = 0;
    if (millis() - lastCheck > 1000) { // Check every second
        lastCheck = millis();

        // Check variables for periodic sync or change-based sync
        for (auto& pair : publishRules) {
            MeshPublishRule& rule = pair.second;

            // Check if it's time to sync
            if (millis() - rule.lastSync >= rule.syncInterval) {
                publishVariable(pair.first, false);
            }
        }

        // Check for stale subscriptions
        checkStaleSubscriptions();
    }
}

// ============================================================================
// Configuration
// ============================================================================

bool MeshExportManager::loadConfig(const JsonObject& config) {
    LOG_INFO("MeshExportManager", "Loading configuration...");

    // Load publish rules
    if (config.containsKey("publish")) {
        JsonObject publishVars = config["publish"];
        for (JsonPair kv : publishVars) {
            String varName = kv.key().c_str();
            JsonObject varConfig = kv.value();

            MeshPublishRule rule;
            rule.variableName = varName;
            rule.syncInterval = varConfig["sync_interval_ms"] | 10000;
            rule.syncOnChange = varConfig["sync_on_change"] | true;
            rule.minChangeThreshold = varConfig["min_change_threshold"] | 0.0;

            publishRules[varName] = rule;
            LOG_INFO("MeshExportManager", "Added publish rule: " + varName);
        }
    }

    // Load subscribe rules
    if (config.containsKey("subscribe")) {
        JsonObject subscribeVars = config["subscribe"];
        for (JsonPair kv : subscribeVars) {
            String remoteVar = kv.key().c_str();
            JsonObject varConfig = kv.value();

            MeshSubscribeRule rule;
            rule.remoteVariable = remoteVar;

            // Parse remote variable name (hub_id.program.variable)
            if (!parseRemoteName(remoteVar, rule.remoteHubId, rule.remoteVarName)) {
                LOG_ERROR("MeshExportManager", "Invalid remote variable format: " + remoteVar);
                continue;
            }

            if (varConfig.containsKey("local_alias")) {
                rule.localAlias = varConfig["local_alias"].as<String>();
            } else {
                rule.localAlias = "mesh_" + remoteVar;
            }

            rule.timeoutMs = varConfig["timeout_ms"] | 30000;

            subscribeRules[remoteVar] = rule;
            LOG_INFO("MeshExportManager", "Added subscribe rule: " + remoteVar + " -> " + rule.localAlias);
        }
    }

    stats.totalPublishRules = publishRules.size();
    stats.totalSubscribeRules = subscribeRules.size();

    LOG_INFO("MeshExportManager", String("Configuration loaded: ") +
             stats.totalPublishRules + " publish rules, " +
             stats.totalSubscribeRules + " subscribe rules");

    return true;
}

bool MeshExportManager::loadConfigFromFile(const String& filepath) {
    if (!LittleFS.exists(filepath)) {
        LOG_WARN("MeshExportManager", "Config file not found: " + filepath);
        return false;
    }

    File file = LittleFS.open(filepath, "r");
    if (!file) {
        LOG_ERROR("MeshExportManager", "Failed to open config file: " + filepath);
        return false;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) {
        LOG_ERROR("MeshExportManager", "Failed to parse config file: " + String(error.c_str()));
        return false;
    }

    return loadConfig(doc["mesh_export"].as<JsonObject>());
}

bool MeshExportManager::saveConfig(const String& filepath) {
    JsonDocument doc;
    JsonObject meshExport = doc["mesh_export"].to<JsonObject>();

    // Save publish rules
    JsonObject publishObj = meshExport["publish"].to<JsonObject>();
    for (const auto& pair : publishRules) {
        JsonObject ruleObj = publishObj[pair.first].to<JsonObject>();
        ruleObj["sync_interval_ms"] = pair.second.syncInterval;
        ruleObj["sync_on_change"] = pair.second.syncOnChange;
        ruleObj["min_change_threshold"] = pair.second.minChangeThreshold;
    }

    // Save subscribe rules
    JsonObject subscribeObj = meshExport["subscribe"].to<JsonObject>();
    for (const auto& pair : subscribeRules) {
        JsonObject ruleObj = subscribeObj[pair.first].to<JsonObject>();
        ruleObj["local_alias"] = pair.second.localAlias;
        ruleObj["timeout_ms"] = pair.second.timeoutMs;
    }

    File file = LittleFS.open(filepath, "w");
    if (!file) {
        LOG_ERROR("MeshExportManager", "Failed to create config file: " + filepath);
        return false;
    }

    serializeJsonPretty(doc, file);
    file.close();

    LOG_INFO("MeshExportManager", "Configuration saved to: " + filepath);
    return true;
}

// ============================================================================
// Publish Rules
// ============================================================================

bool MeshExportManager::addPublishRule(const String& varName, unsigned long syncInterval,
                                      bool syncOnChange) {
    if (!variableRegistry) {
        LOG_ERROR("MeshExportManager", "VariableRegistry not set");
        return false;
    }

    // Check if variable exists
    if (!variableRegistry->hasVariable(varName)) {
        LOG_ERROR("MeshExportManager", "Variable not found: " + varName);
        return false;
    }

    MeshPublishRule rule;
    rule.variableName = varName;
    rule.syncInterval = syncInterval;
    rule.syncOnChange = syncOnChange;
    rule.minChangeThreshold = 0.0;
    rule.lastSync = 0;

    publishRules[varName] = rule;
    stats.totalPublishRules = publishRules.size();

    LOG_INFO("MeshExportManager", "Added publish rule: " + varName);
    return true;
}

bool MeshExportManager::removePublishRule(const String& varName) {
    auto it = publishRules.find(varName);
    if (it == publishRules.end()) {
        return false;
    }

    publishRules.erase(it);
    stats.totalPublishRules = publishRules.size();

    LOG_INFO("MeshExportManager", "Removed publish rule: " + varName);
    return true;
}

bool MeshExportManager::publishVariable(const String& varName, bool force) {
    if (!mesh || !variableRegistry) {
        LOG_ERROR("MeshExportManager", "Mesh or VariableRegistry not set");
        return false;
    }

    auto it = publishRules.find(varName);
    if (it == publishRules.end()) {
        LOG_WARN("MeshExportManager", "No publish rule for variable: " + varName);
        return false;
    }

    MeshPublishRule& rule = it->second;

    // Read current value
    PlcValue currentValue(PlcValueType::BOOL);
    if (!variableRegistry->readVariable(varName, currentValue)) {
        LOG_ERROR("MeshExportManager", "Failed to read variable: " + varName);
        return false;
    }

    // Check if should publish
    if (!force && !shouldPublish(rule, currentValue)) {
        return false;
    }

    // Send to mesh network
    sendVariableToMesh(varName, currentValue);

    // Update rule
    rule.lastSync = millis();
    rule.lastValue = currentValue;
    stats.totalPublishes++;
    stats.lastActivity = millis();

    LOG_INFO("MeshExportManager", "Published variable: " + varName);
    return true;
}

void MeshExportManager::publishAllVariables(bool force) {
    for (const auto& pair : publishRules) {
        publishVariable(pair.first, force);
    }
}

// ============================================================================
// Subscribe Rules
// ============================================================================

bool MeshExportManager::addSubscribeRule(const String& remoteVariable, const String& localAlias,
                                        unsigned long timeoutMs) {
    if (!variableRegistry) {
        LOG_ERROR("MeshExportManager", "VariableRegistry not set");
        return false;
    }

    MeshSubscribeRule rule;
    rule.remoteVariable = remoteVariable;
    rule.localAlias = localAlias;
    rule.timeoutMs = timeoutMs;
    rule.lastReceived = 0;
    rule.isStale = true;

    // Parse remote variable name
    if (!parseRemoteName(remoteVariable, rule.remoteHubId, rule.remoteVarName)) {
        LOG_ERROR("MeshExportManager", "Invalid remote variable format: " + remoteVariable);
        return false;
    }

    subscribeRules[remoteVariable] = rule;
    stats.totalSubscribeRules = subscribeRules.size();

    LOG_INFO("MeshExportManager", "Added subscribe rule: " + remoteVariable + " -> " + localAlias);
    return true;
}

bool MeshExportManager::removeSubscribeRule(const String& remoteVariable) {
    auto it = subscribeRules.find(remoteVariable);
    if (it == subscribeRules.end()) {
        return false;
    }

    subscribeRules.erase(it);
    stats.totalSubscribeRules = subscribeRules.size();

    LOG_INFO("MeshExportManager", "Removed subscribe rule: " + remoteVariable);
    return true;
}

bool MeshExportManager::requestRemoteVariable(const String& remoteVariable) {
    if (!mesh) {
        LOG_ERROR("MeshExportManager", "Mesh not set");
        return false;
    }

    auto it = subscribeRules.find(remoteVariable);
    if (it == subscribeRules.end()) {
        LOG_WARN("MeshExportManager", "No subscribe rule for: " + remoteVariable);
        return false;
    }

    const MeshSubscribeRule& rule = it->second;

    // TODO: Find node ID from hub ID
    // For now, broadcast request
    JsonDocument doc;
    doc["type"] = MESH_MSG_TYPE_VARIABLE_REQUEST;
    doc["hub_id"] = localHubId;
    doc["variable"] = rule.remoteVarName;

    String msg = serializeMeshMessage(doc);
    mesh->sendBroadcast(msg);

    stats.totalRequests++;
    stats.lastActivity = millis();

    LOG_INFO("MeshExportManager", "Requested variable: " + remoteVariable);
    return true;
}

// ============================================================================
// Mesh Message Handling
// ============================================================================

void MeshExportManager::handleVariableSync(uint32_t fromNodeId, const JsonObject& payload) {
    if (!payload.containsKey("hub_id") || !payload.containsKey("variable") || !payload.containsKey("value")) {
        LOG_WARN("MeshExportManager", "Invalid variable sync payload");
        return;
    }

    String remoteHubId = payload["hub_id"].as<String>();
    String varName = payload["variable"].as<String>();
    String fullRemoteName = buildFullRemoteName(remoteHubId, varName);

    // Check if we're subscribed to this variable
    auto it = subscribeRules.find(fullRemoteName);
    if (it == subscribeRules.end()) {
        // Not subscribed
        return;
    }

    // Parse value
    PlcValue value(PlcValueType::BOOL);
    JsonVariant valueVar = payload["value"];

    if (payload.containsKey("type")) {
        String typeStr = payload["type"].as<String>();
        if (typeStr == "bool") {
            value.type = PlcValueType::BOOL;
            value.value.bVal = valueVar.as<bool>();
        } else if (typeStr == "int") {
            value.type = PlcValueType::INT;
            value.value.i16Val = valueVar.as<int16_t>();
        } else if (typeStr == "float") {
            value.type = PlcValueType::REAL;
            value.value.fVal = valueVar.as<float>();
        } else if (typeStr == "string") {
            value.type = PlcValueType::STRING_TYPE;
            String str = valueVar.as<String>();
            strncpy(value.value.sVal, str.c_str(), sizeof(value.value.sVal) - 1);
            value.value.sVal[sizeof(value.value.sVal) - 1] = '\0';
        }
    }

    // Update subscribed variable
    updateSubscribedVariable(fullRemoteName, value);

    stats.totalReceived++;
    stats.lastActivity = millis();

    LOG_INFO("MeshExportManager", "Received variable sync: " + fullRemoteName);
}

void MeshExportManager::handleVariableRequest(uint32_t fromNodeId, const JsonObject& payload) {
    if (!payload.containsKey("hub_id") || !payload.containsKey("variable")) {
        LOG_WARN("MeshExportManager", "Invalid variable request payload");
        return;
    }

    String requestingHubId = payload["hub_id"].as<String>();
    String varName = payload["variable"].as<String>();

    // Check if we have this variable in our publish rules
    auto it = publishRules.find(varName);
    if (it == publishRules.end()) {
        // We don't publish this variable
        return;
    }

    // Publish the variable immediately to the requesting node
    publishVariable(varName, true);

    LOG_INFO("MeshExportManager", "Received variable request from " + requestingHubId + " for: " + varName);
}

// ============================================================================
// Statistics and Monitoring
// ============================================================================

MeshExportManager::MeshExportStats MeshExportManager::getStatistics() const {
    return stats;
}

std::vector<String> MeshExportManager::getPublishedVariables() const {
    std::vector<String> result;
    for (const auto& pair : publishRules) {
        result.push_back(pair.first);
    }
    return result;
}

std::vector<String> MeshExportManager::getSubscribedVariables() const {
    std::vector<String> result;
    for (const auto& pair : subscribeRules) {
        result.push_back(pair.first);
    }
    return result;
}

bool MeshExportManager::isSubscriptionStale(const String& remoteVariable) const {
    auto it = subscribeRules.find(remoteVariable);
    if (it == subscribeRules.end()) {
        return true;
    }
    return it->second.isStale;
}

// ============================================================================
// Integration Hooks
// ============================================================================

void MeshExportManager::setMesh(painlessMesh* meshPtr) {
    mesh = meshPtr;
}

void MeshExportManager::setVariableRegistry(VariableRegistry* registry) {
    variableRegistry = registry;
}

void MeshExportManager::setPlcEngine(PlcEngine* engine) {
    plcEngine = engine;
}

void MeshExportManager::setLocalHubId(const String& hubId) {
    localHubId = hubId;
    LOG_INFO("MeshExportManager", "Local hub ID set to: " + hubId);
}

// ============================================================================
// Helper Methods
// ============================================================================

bool MeshExportManager::shouldPublish(const MeshPublishRule& rule, const PlcValue& currentValue) {
    // Check if enough time has passed
    if (millis() - rule.lastSync < rule.syncInterval) {
        return false;
    }

    // If sync on change is enabled, check if value changed
    if (rule.syncOnChange) {
        // Check if value changed significantly
        if (currentValue.type != rule.lastValue.type) {
            return true;
        }

        switch (currentValue.type) {
            case PlcValueType::BOOL:
                return currentValue.value.bVal != rule.lastValue.value.bVal;

            case PlcValueType::INT:
                return currentValue.value.i16Val != rule.lastValue.value.i16Val;

            case PlcValueType::REAL:
                return abs(currentValue.value.fVal - rule.lastValue.value.fVal) >= rule.minChangeThreshold;

            case PlcValueType::STRING_TYPE:
                return strcmp(currentValue.value.sVal, rule.lastValue.value.sVal) != 0;

            default:
                return false;
        }
    }

    return true; // Time interval passed
}

void MeshExportManager::sendVariableToMesh(const String& varName, const PlcValue& value) {
    if (!mesh) {
        return;
    }

    JsonDocument doc;
    doc["type"] = MESH_MSG_TYPE_VARIABLE_SYNC;
    doc["hub_id"] = localHubId;
    doc["variable"] = varName;

    // Add value and type
    switch (value.type) {
        case PlcValueType::BOOL:
            doc["type_str"] = "bool";
            doc["value"] = value.value.bVal;
            break;
        case PlcValueType::INT:
            doc["type_str"] = "int";
            doc["value"] = value.value.i16Val;
            break;
        case PlcValueType::REAL:
            doc["type_str"] = "float";
            doc["value"] = value.value.fVal;
            break;
        case PlcValueType::STRING_TYPE:
            doc["type_str"] = "string";
            doc["value"] = value.value.sVal;
            break;
        default:
            return;
    }

    String msg = serializeMeshMessage(doc);
    mesh->sendBroadcast(msg);
}

void MeshExportManager::sendVariableRequest(uint32_t targetNodeId, const String& varName) {
    if (!mesh) {
        return;
    }

    JsonDocument doc;
    doc["type"] = MESH_MSG_TYPE_VARIABLE_REQUEST;
    doc["hub_id"] = localHubId;
    doc["variable"] = varName;

    String msg = serializeMeshMessage(doc);
    mesh->sendSingle(targetNodeId, msg);
}

void MeshExportManager::updateSubscribedVariable(const String& remoteVariable, const PlcValue& value) {
    auto it = subscribeRules.find(remoteVariable);
    if (it == subscribeRules.end()) {
        return;
    }

    MeshSubscribeRule& rule = it->second;

    // Update local variable (alias)
    if (variableRegistry) {
        // Register as mesh remote variable if doesn't exist
        if (!variableRegistry->hasVariable(rule.localAlias)) {
            // Parse remoteVariable: hub_id.program.variable
            int firstDot = remoteVariable.indexOf('.');
            int secondDot = remoteVariable.indexOf('.', firstDot + 1);

            if (firstDot > 0 && secondDot > firstDot) {
                String hubId = remoteVariable.substring(0, firstDot);
                String programName = remoteVariable.substring(firstDot + 1, secondDot);
                String varName = remoteVariable.substring(secondDot + 1);

                variableRegistry->registerMeshVariable(hubId, programName, varName, value.type);
            }
        }

        // Write value using the local alias
        variableRegistry->writeVariable(rule.localAlias, value);
    }

    // Update subscription state
    rule.lastReceived = millis();
    rule.isStale = false;
}

void MeshExportManager::checkStaleSubscriptions() {
    int staleCount = 0;
    for (auto& pair : subscribeRules) {
        MeshSubscribeRule& rule = pair.second;

        if (!rule.isStale && (millis() - rule.lastReceived > rule.timeoutMs)) {
            rule.isStale = true;
            LOG_WARN("MeshExportManager", "Subscription stale: " + pair.first);
        }

        if (rule.isStale) {
            staleCount++;
        }
    }
    stats.staleSubscriptions = staleCount;
}

String MeshExportManager::buildFullRemoteName(const String& hubId, const String& varName) {
    return hubId + "." + varName;
}

bool MeshExportManager::parseRemoteName(const String& fullName, String& hubId, String& varName) {
    int dotIndex = fullName.indexOf('.');
    if (dotIndex <= 0 || dotIndex >= fullName.length() - 1) {
        return false;
    }

    hubId = fullName.substring(0, dotIndex);
    varName = fullName.substring(dotIndex + 1);
    return true;
}

String MeshExportManager::getHubIdFromNodeId(uint32_t nodeId) {
    // TODO: Implement mapping from mesh node ID to hub ID
    // This may require MeshDeviceManager integration
    return String(nodeId, HEX);
}

void MeshExportManager::onVariableChanged(const String& varName, const PlcValue& oldValue,
                                         const PlcValue& newValue, void* context) {
    MeshExportManager* manager = static_cast<MeshExportManager*>(context);
    if (!manager) {
        return;
    }

    // Check if this variable has a publish rule with sync on change
    auto it = manager->publishRules.find(varName);
    if (it != manager->publishRules.end() && it->second.syncOnChange) {
        manager->publishVariable(varName, false);
    }
}
