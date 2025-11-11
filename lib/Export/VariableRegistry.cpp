#include "VariableRegistry.h"
#include "Logger.h"
#include "../PlcCore/PlcEngine.h"
#include "DeviceConfigManager.h"
#include "MqttManager.h"

VariableRegistry::VariableRegistry()
    : plcEngine(nullptr),
      deviceConfigManager(nullptr),
      mqttManager(nullptr),
      localHubId("hub_0"),
      changeCallback(nullptr) {
}

VariableRegistry::~VariableRegistry() {
}

void VariableRegistry::begin() {
    LOG_INFO("VariableRegistry", "Initializing...");
    variables.clear();
    readCallbacks.clear();
    writeCallbacks.clear();
    LOG_INFO("VariableRegistry", "Initialized");
}

// ============================================================================
// Variable Registration
// ============================================================================

bool VariableRegistry::registerPlcVariable(const String& programName, const String& varName,
                                          PlcValueType type, bool exportToMqtt) {
    String fullName = buildFullName(programName, varName);

    if (hasVariable(fullName)) {
        LOG_WARN("VariableRegistry", "Variable already registered: " + fullName);
        return false;
    }

    VariableMetadata meta;
    meta.fullName = fullName;
    meta.namespace_ = programName;
    meta.localName = varName;
    meta.source = VariableSource::PLC_MEMORY;
    meta.type = type;
    meta.readable = true;
    meta.writable = true;
    meta.exportToMqtt = exportToMqtt;
    meta.mqttTopic = "esphub/" + localHubId + "/plc/" + programName + "/" + varName;
    meta.description = "PLC variable: " + programName + "." + varName;
    meta.lastUpdate = 0;

    variables[fullName] = meta;
    LOG_INFO("VariableRegistry", "Registered PLC variable: " + fullName);
    return true;
}

bool VariableRegistry::registerDeviceEndpoint(const String& deviceId, const String& endpointName,
                                             PlcValueType type, bool readable, bool writable,
                                             bool exportToMqtt) {
    String fullName = buildFullName(deviceId, endpointName);

    if (hasVariable(fullName)) {
        LOG_WARN("VariableRegistry", "Variable already registered: " + fullName);
        return false;
    }

    VariableMetadata meta;
    meta.fullName = fullName;
    meta.namespace_ = deviceId;
    meta.localName = endpointName;
    meta.source = VariableSource::DEVICE_ENDPOINT;
    meta.type = type;
    meta.readable = readable;
    meta.writable = writable;
    meta.exportToMqtt = exportToMqtt;
    meta.mqttTopic = "esphub/" + localHubId + "/device/" + deviceId + "/" + endpointName;
    meta.description = "Device endpoint: " + deviceId + "." + endpointName;
    meta.lastUpdate = 0;

    variables[fullName] = meta;
    LOG_INFO("VariableRegistry", "Registered device endpoint: " + fullName);
    return true;
}

bool VariableRegistry::registerMeshVariable(const String& hubId, const String& programName,
                                           const String& varName, PlcValueType type) {
    String namespace_ = hubId + "." + programName;
    String fullName = buildFullName(namespace_, varName);

    if (hasVariable(fullName)) {
        LOG_WARN("VariableRegistry", "Variable already registered: " + fullName);
        return false;
    }

    VariableMetadata meta;
    meta.fullName = fullName;
    meta.namespace_ = namespace_;
    meta.localName = varName;
    meta.source = VariableSource::MESH_REMOTE;
    meta.type = type;
    meta.readable = true;
    meta.writable = true;
    meta.exportToMqtt = false; // Remote variables not auto-exported
    meta.mqttTopic = "";
    meta.description = "Mesh variable: " + hubId + "." + programName + "." + varName;
    meta.lastUpdate = 0;

    variables[fullName] = meta;
    LOG_INFO("VariableRegistry", "Registered mesh variable: " + fullName);
    return true;
}

bool VariableRegistry::registerMqttVariable(const String& topicPath, const String& varName, PlcValueType type) {
    String fullName = buildFullName("mqtt", varName);

    if (hasVariable(fullName)) {
        LOG_WARN("VariableRegistry", "Variable already registered: " + fullName);
        return false;
    }

    VariableMetadata meta;
    meta.fullName = fullName;
    meta.namespace_ = "mqtt";
    meta.localName = varName;
    meta.source = VariableSource::MQTT_TOPIC;
    meta.type = type;
    meta.readable = true;
    meta.writable = true;
    meta.exportToMqtt = false; // MQTT variables are not re-exported
    meta.mqttTopic = topicPath;
    meta.description = "MQTT topic: " + topicPath;
    meta.lastUpdate = 0;

    variables[fullName] = meta;
    LOG_INFO("VariableRegistry", "Registered MQTT variable: " + fullName);
    return true;
}

bool VariableRegistry::unregisterVariable(const String& fullName) {
    auto it = variables.find(fullName);
    if (it == variables.end()) {
        return false;
    }

    variables.erase(it);
    readCallbacks.erase(fullName);
    writeCallbacks.erase(fullName);

    LOG_INFO("VariableRegistry", "Unregistered variable: " + fullName);
    return true;
}

// ============================================================================
// Variable Access
// ============================================================================

bool VariableRegistry::readVariable(const String& fullName, PlcValue& value) {
    auto it = variables.find(fullName);
    if (it == variables.end()) {
        LOG_ERROR("VariableRegistry", "Variable not found: " + fullName);
        return false;
    }

    const VariableMetadata& meta = it->second;

    if (!meta.readable) {
        LOG_ERROR("VariableRegistry", "Variable not readable: " + fullName);
        return false;
    }

    // Check for custom read callback
    auto callbackIt = readCallbacks.find(fullName);
    if (callbackIt != readCallbacks.end()) {
        return callbackIt->second(fullName, value);
    }

    // Default read based on source
    bool success = false;
    switch (meta.source) {
        case VariableSource::PLC_MEMORY:
            success = readFromPlc(meta, value);
            break;
        case VariableSource::DEVICE_ENDPOINT:
            success = readFromDevice(meta, value);
            break;
        case VariableSource::MESH_REMOTE:
            LOG_WARN("VariableRegistry", "Mesh remote read not yet implemented: " + fullName);
            success = false;
            break;
        case VariableSource::MQTT_TOPIC:
            LOG_WARN("VariableRegistry", "MQTT topic read not yet implemented: " + fullName);
            success = false;
            break;
        case VariableSource::VIRTUAL:
            LOG_WARN("VariableRegistry", "Virtual variable requires read callback: " + fullName);
            success = false;
            break;
    }

    if (success) {
        // Update timestamp
        variables[fullName].lastUpdate = millis();
    }

    return success;
}

bool VariableRegistry::writeVariable(const String& fullName, const PlcValue& value) {
    auto it = variables.find(fullName);
    if (it == variables.end()) {
        LOG_ERROR("VariableRegistry", "Variable not found: " + fullName);
        return false;
    }

    const VariableMetadata& meta = it->second;

    if (!meta.writable) {
        LOG_ERROR("VariableRegistry", "Variable not writable: " + fullName);
        return false;
    }

    // Type check
    if (value.type != meta.type) {
        LOG_ERROR("VariableRegistry", "Type mismatch for " + fullName);
        return false;
    }

    // Read old value for change notification
    PlcValue oldValue(meta.type);
    readVariable(fullName, oldValue);

    // Check for custom write callback
    auto callbackIt = writeCallbacks.find(fullName);
    if (callbackIt != writeCallbacks.end()) {
        bool success = callbackIt->second(fullName, value);
        if (success) {
            notifyChange(fullName, oldValue, value);
            variables[fullName].lastUpdate = millis();
        }
        return success;
    }

    // Default write based on source
    bool success = false;
    switch (meta.source) {
        case VariableSource::PLC_MEMORY:
            success = writeToPlc(meta, value);
            break;
        case VariableSource::DEVICE_ENDPOINT:
            success = writeToDevice(meta, value);
            break;
        case VariableSource::MESH_REMOTE:
            LOG_WARN("VariableRegistry", "Mesh remote write not yet implemented: " + fullName);
            success = false;
            break;
        case VariableSource::MQTT_TOPIC:
            LOG_WARN("VariableRegistry", "MQTT topic write not yet implemented: " + fullName);
            success = false;
            break;
        case VariableSource::VIRTUAL:
            LOG_WARN("VariableRegistry", "Virtual variable requires write callback: " + fullName);
            success = false;
            break;
    }

    if (success) {
        notifyChange(fullName, oldValue, value);
        variables[fullName].lastUpdate = millis();
    }

    return success;
}

bool VariableRegistry::hasVariable(const String& fullName) const {
    return variables.find(fullName) != variables.end();
}

bool VariableRegistry::getMetadata(const String& fullName, VariableMetadata& metadata) {
    auto it = variables.find(fullName);
    if (it == variables.end()) {
        return false;
    }
    metadata = it->second;
    return true;
}

// ============================================================================
// Callbacks
// ============================================================================

void VariableRegistry::onVariableChange(VariableChangeCallback callback) {
    changeCallback = callback;
}

void VariableRegistry::setReadCallback(const String& fullName, VariableReadCallback callback) {
    readCallbacks[fullName] = callback;
}

void VariableRegistry::setWriteCallback(const String& fullName, VariableWriteCallback callback) {
    writeCallbacks[fullName] = callback;
}

// ============================================================================
// Query and Export
// ============================================================================

std::vector<String> VariableRegistry::getAllVariables() const {
    std::vector<String> result;
    for (const auto& pair : variables) {
        result.push_back(pair.first);
    }
    return result;
}

std::vector<String> VariableRegistry::getVariablesBySource(VariableSource source) const {
    std::vector<String> result;
    for (const auto& pair : variables) {
        if (pair.second.source == source) {
            result.push_back(pair.first);
        }
    }
    return result;
}

std::vector<String> VariableRegistry::getVariablesByNamespace(const String& namespace_) const {
    std::vector<String> result;
    for (const auto& pair : variables) {
        if (pair.second.namespace_ == namespace_) {
            result.push_back(pair.first);
        }
    }
    return result;
}

std::vector<String> VariableRegistry::getMqttExportVariables() const {
    std::vector<String> result;
    for (const auto& pair : variables) {
        if (pair.second.exportToMqtt) {
            result.push_back(pair.first);
        }
    }
    return result;
}

void VariableRegistry::exportToJson(JsonObject& json, const String& namespace_) {
    for (auto& pair : variables) {
        const VariableMetadata& meta = pair.second;

        // Filter by namespace if specified
        if (!namespace_.isEmpty() && meta.namespace_ != namespace_) {
            continue;
        }

        // Read current value
        PlcValue value(meta.type);
        if (!readVariable(pair.first, value)) {
            continue;
        }

        // Add to JSON
        switch (value.type) {
            case PlcValueType::BOOL:
                json[pair.first] = value.value.bVal;
                break;
            case PlcValueType::INT:
                json[pair.first] = value.value.i16Val;
                break;
            case PlcValueType::REAL:
                json[pair.first] = value.value.fVal;
                break;
            case PlcValueType::STRING_TYPE:
                json[pair.first] = value.value.sVal;
                break;
            default:
                break;
        }
    }
}

void VariableRegistry::importFromJson(const JsonObject& json, const String& namespace_) {
    for (JsonPair kv : json) {
        String varName = kv.key().c_str();

        // If namespace specified, prepend it
        String fullName = namespace_.isEmpty() ? varName : buildFullName(namespace_, varName);

        // Check if variable exists
        if (!hasVariable(fullName)) {
            LOG_WARN("VariableRegistry", "Variable not found during import: " + fullName);
            continue;
        }

        // Get metadata
        VariableMetadata meta;
        if (!getMetadata(fullName, meta)) {
            continue;
        }

        // Create value from JSON
        PlcValue value(meta.type);
        JsonVariantConst variant = kv.value();

        switch (meta.type) {
            case PlcValueType::BOOL:
                if (variant.is<bool>()) {
                    value.value.bVal = variant.as<bool>();
                }
                break;
            case PlcValueType::INT:
                if (variant.is<int>()) {
                    value.value.i16Val = variant.as<int>();
                }
                break;
            case PlcValueType::REAL:
                if (variant.is<float>()) {
                    value.value.fVal = variant.as<float>();
                }
                break;
            case PlcValueType::STRING_TYPE:
                if (variant.is<const char*>()) {
                    strncpy(value.value.sVal, variant.as<const char*>(), sizeof(value.value.sVal) - 1);
                }
                break;
            default:
                continue;
        }

        // Write value
        writeVariable(fullName, value);
    }
}

// ============================================================================
// Integration Hooks
// ============================================================================

void VariableRegistry::setPlcEngine(PlcEngine* engine) {
    plcEngine = engine;
}

void VariableRegistry::setDeviceConfigManager(DeviceConfigManager* manager) {
    deviceConfigManager = manager;
}

void VariableRegistry::setMqttManager(MqttManager* manager) {
    mqttManager = manager;
}

void VariableRegistry::setLocalHubId(const String& hubId) {
    localHubId = hubId;
}

// ============================================================================
// Private Helper Methods
// ============================================================================

String VariableRegistry::buildFullName(const String& namespace_, const String& localName) {
    return namespace_ + "." + localName;
}

void VariableRegistry::parseFullName(const String& fullName, String& namespace_, String& localName) {
    int dotPos = fullName.indexOf('.');
    if (dotPos > 0) {
        namespace_ = fullName.substring(0, dotPos);
        localName = fullName.substring(dotPos + 1);
    } else {
        namespace_ = "";
        localName = fullName;
    }
}

bool VariableRegistry::readFromPlc(const VariableMetadata& meta, PlcValue& value) {
    if (!plcEngine) {
        LOG_ERROR("VariableRegistry", "PlcEngine not set");
        return false;
    }

    PlcProgram* program = plcEngine->getProgram(meta.namespace_);
    if (!program) {
        LOG_ERROR("VariableRegistry", "PLC program not found: " + meta.namespace_);
        return false;
    }

    // Get value from PLC memory
    PlcMemory& memory = program->getMemory();

    // Convert Arduino String to std::string for PlcMemory
    std::string varName = meta.localName.c_str();

    switch (meta.type) {
        case PlcValueType::BOOL:
            value.value.bVal = memory.getValue<bool>(varName);
            return true;
        case PlcValueType::INT:
            value.value.i16Val = memory.getValue<int16_t>(varName);
            return true;
        case PlcValueType::REAL:
            value.value.fVal = memory.getValue<float>(varName);
            return true;
        case PlcValueType::STRING_TYPE: {
            std::string str = memory.getValue<std::string>(varName);
            strncpy(value.value.sVal, str.c_str(), sizeof(value.value.sVal) - 1);
            value.value.sVal[sizeof(value.value.sVal) - 1] = '\0';
            return true;
        }
        default:
            return false;
    }
}

bool VariableRegistry::writeToPlc(const VariableMetadata& meta, const PlcValue& value) {
    if (!plcEngine) {
        LOG_ERROR("VariableRegistry", "PlcEngine not set");
        return false;
    }

    PlcProgram* program = plcEngine->getProgram(meta.namespace_);
    if (!program) {
        LOG_ERROR("VariableRegistry", "PLC program not found: " + meta.namespace_);
        return false;
    }

    // Write value to PLC memory
    PlcMemory& memory = program->getMemory();

    // Convert Arduino String to std::string for PlcMemory
    std::string varName = meta.localName.c_str();

    switch (meta.type) {
        case PlcValueType::BOOL:
            memory.setValue<bool>(varName, value.value.bVal);
            return true;
        case PlcValueType::INT:
            memory.setValue<int16_t>(varName, value.value.i16Val);
            return true;
        case PlcValueType::REAL:
            memory.setValue<float>(varName, value.value.fVal);
            return true;
        case PlcValueType::STRING_TYPE: {
            std::string str = value.value.sVal;
            memory.setValue<std::string>(varName, str);
            return true;
        }
        default:
            return false;
    }
}

bool VariableRegistry::readFromDevice(const VariableMetadata& meta, PlcValue& value) {
    if (!deviceConfigManager) {
        LOG_ERROR("VariableRegistry", "DeviceConfigManager not set");
        return false;
    }

    return deviceConfigManager->readEndpoint(meta.namespace_, meta.localName, value);
}

bool VariableRegistry::writeToDevice(const VariableMetadata& meta, const PlcValue& value) {
    if (!deviceConfigManager) {
        LOG_ERROR("VariableRegistry", "DeviceConfigManager not set");
        return false;
    }

    return deviceConfigManager->writeEndpoint(meta.namespace_, meta.localName, value);
}

void VariableRegistry::notifyChange(const String& varName, const PlcValue& oldValue, const PlcValue& newValue) {
    if (changeCallback) {
        changeCallback(varName, oldValue, newValue);
    }

    // Auto-export to MQTT if enabled
    auto it = variables.find(varName);
    if (it != variables.end() && it->second.exportToMqtt && mqttManager) {
        const VariableMetadata& meta = it->second;

        // Format value as string for MQTT
        String valueStr;
        switch (newValue.type) {
            case PlcValueType::BOOL:
                valueStr = newValue.value.bVal ? "true" : "false";
                break;
            case PlcValueType::INT:
                valueStr = String(newValue.value.i16Val);
                break;
            case PlcValueType::REAL:
                valueStr = String(newValue.value.fVal, 2);
                break;
            case PlcValueType::STRING_TYPE:
                valueStr = newValue.value.sVal;
                break;
        }

        mqttManager->publish(meta.mqttTopic.c_str(), valueStr.c_str());
    }
}
