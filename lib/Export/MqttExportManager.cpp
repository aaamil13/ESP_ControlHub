#include "../Export/MqttExportManager.h"
#include "../Core/Logger.h"
#include <LittleFS.h>
#include <cfloat>  // For DBL_MAX

MqttExportManager::MqttExportManager()
    : mqttManager(nullptr),
      variableRegistry(nullptr),
      plcEngine(nullptr),
      deviceRegistry(nullptr),
      plcCommandHandler(nullptr) {
    memset(&stats, 0, sizeof(stats));
}

MqttExportManager::~MqttExportManager() {
}

void MqttExportManager::begin() {
    LOG_INFO("MqttExportManager", "Initializing...");
    exportRules.clear();
    commands.clear();
    topicToVariable.clear();
    topicToCommand.clear();
    memset(&stats, 0, sizeof(stats));
    LOG_INFO("MqttExportManager", "Initialized");
}

void MqttExportManager::loop() {
    // Periodic auto-publish of variables (optional polling mode)
    // Most variables will be published via change callbacks
    static unsigned long lastCheck = 0;
    if (millis() - lastCheck > 1000) { // Check every second
        lastCheck = millis();

        // Check if any variables need periodic publishing
        for (auto& pair : exportRules) {
            ExportRule& rule = pair.second;
            if (rule.publishOnChange && rule.minPublishInterval > 0) {
                if (millis() - rule.lastPublish >= rule.minPublishInterval) {
                    publishVariable(pair.first, false);
                }
            }
        }
    }
}

// ============================================================================
// Configuration
// ============================================================================

bool MqttExportManager::loadConfig(const JsonObject& config) {
    LOG_INFO("MqttExportManager", "Loading configuration...");

    // Load variable export rules
    if (config.containsKey("variables")) {
        JsonObject variables = config["variables"];
        for (JsonPair kv : variables) {
            String varName = kv.key().c_str();
            JsonObject varConfig = kv.value();

            ExportRule rule;
            rule.variableName = varName;
            // Handle default topic with String concatenation
            if (varConfig.containsKey("topic")) {
                rule.mqttTopic = varConfig["topic"].as<String>();
            } else {
                rule.mqttTopic = "esphub/" + varName;
            }

            String accessStr = varConfig["access"] | "read";
            if (accessStr == "read") rule.access = ExportAccessLevel::READ_ONLY;
            else if (accessStr == "write") rule.access = ExportAccessLevel::WRITE_ONLY;
            else if (accessStr == "read_write") rule.access = ExportAccessLevel::READ_WRITE;

            rule.publishOnChange = varConfig["publish_on_change"] | true;
            rule.minPublishInterval = varConfig["min_interval_ms"] | 0;
            rule.retained = varConfig["retained"] | false;
            rule.qos = varConfig["qos"] | 0;

            // Load validation rules
            if (varConfig.containsKey("validation")) {
                JsonObject val = varConfig["validation"];
                rule.validation.enabled = true;
                rule.validation.minValue = val["min"] | -DBL_MAX;
                rule.validation.maxValue = val["max"] | DBL_MAX;
                rule.validation.regex = val["regex"] | "";
            }

            exportRules[varName] = rule;

            // Map topic to variable for incoming messages
            if (rule.access == ExportAccessLevel::WRITE_ONLY ||
                rule.access == ExportAccessLevel::READ_WRITE) {
                topicToVariable[rule.mqttTopic] = varName;
            }

            LOG_INFO("MqttExportManager", "Added export rule: " + varName + " -> " + rule.mqttTopic);
        }
    }

    // Load command definitions
    if (config.containsKey("commands")) {
        JsonObject cmds = config["commands"];
        for (JsonPair kv : cmds) {
            String cmdName = kv.key().c_str();
            JsonObject cmdConfig = kv.value();

            CommandDefinition cmd;
            cmd.commandName = cmdName;
            // Handle default topic with String concatenation
            if (cmdConfig.containsKey("topic")) {
                cmd.mqttTopic = cmdConfig["topic"].as<String>();
            } else {
                cmd.mqttTopic = "esphub/commands/" + cmdName;
            }
            cmd.plcHandler = cmdConfig["handler"] | "";
            cmd.requireAuth = cmdConfig["require_auth"] | false;

            // Load parameters
            if (cmdConfig.containsKey("parameters")) {
                JsonArray params = cmdConfig["parameters"];
                for (JsonVariant v : params) {
                    cmd.parameters.push_back(v.as<String>());
                }
            }

            commands[cmdName] = cmd;
            topicToCommand[cmd.mqttTopic] = cmdName;

            LOG_INFO("MqttExportManager", "Registered command: " + cmdName + " -> " + cmd.mqttTopic);
        }
    }

    // Subscribe to MQTT topics
    subscribeToTopics();

    // Update statistics
    stats.totalExports = exportRules.size();
    stats.commands = commands.size();

    for (const auto& pair : exportRules) {
        if (pair.second.access == ExportAccessLevel::READ_ONLY) {
            stats.readOnlyVars++;
        } else if (pair.second.access == ExportAccessLevel::READ_WRITE) {
            stats.readWriteVars++;
        }
    }

    LOG_INFO("MqttExportManager", String("Loaded ") + String(stats.totalExports) +
             " exports, " + String(stats.commands) + " commands");
    return true;
}

bool MqttExportManager::loadConfigFromFile(const String& filepath) {
    if (!LittleFS.exists(filepath)) {
        LOG_ERROR("MqttExportManager", "Config file not found: " + filepath);
        return false;
    }

    File file = LittleFS.open(filepath, "r");
    if (!file) {
        LOG_ERROR("MqttExportManager", "Failed to open: " + filepath);
        return false;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) {
        LOG_ERROR("MqttExportManager", "JSON parse error: " + String(error.c_str()));
        return false;
    }

    return loadConfig(doc.as<JsonObject>());
}

bool MqttExportManager::saveConfig(const String& filepath) {
    JsonDocument doc;
    JsonObject config = doc.to<JsonObject>();

    // Save export rules
    JsonObject variables = config["variables"].to<JsonObject>();
    for (const auto& pair : exportRules) {
        JsonObject varConfig = variables[pair.first].to<JsonObject>();
        varConfig["topic"] = pair.second.mqttTopic;

        switch (pair.second.access) {
            case ExportAccessLevel::READ_ONLY:
                varConfig["access"] = "read";
                break;
            case ExportAccessLevel::WRITE_ONLY:
                varConfig["access"] = "write";
                break;
            case ExportAccessLevel::READ_WRITE:
                varConfig["access"] = "read_write";
                break;
            default:
                break;
        }

        varConfig["publish_on_change"] = pair.second.publishOnChange;
        varConfig["min_interval_ms"] = pair.second.minPublishInterval;
        varConfig["retained"] = pair.second.retained;
        varConfig["qos"] = pair.second.qos;

        if (pair.second.validation.enabled) {
            JsonObject val = varConfig["validation"].to<JsonObject>();
            val["min"] = pair.second.validation.minValue;
            val["max"] = pair.second.validation.maxValue;
            if (!pair.second.validation.regex.isEmpty()) {
                val["regex"] = pair.second.validation.regex;
            }
        }
    }

    // Save commands
    JsonObject cmds = config["commands"].to<JsonObject>();
    for (const auto& pair : commands) {
        JsonObject cmdConfig = cmds[pair.first].to<JsonObject>();
        cmdConfig["topic"] = pair.second.mqttTopic;
        cmdConfig["handler"] = pair.second.plcHandler;
        cmdConfig["require_auth"] = pair.second.requireAuth;

        if (!pair.second.parameters.empty()) {
            JsonArray params = cmdConfig["parameters"].to<JsonArray>();
            for (const String& p : pair.second.parameters) {
                params.add(p);
            }
        }
    }

    File file = LittleFS.open(filepath, "w");
    if (!file) {
        LOG_ERROR("MqttExportManager", "Failed to open for writing: " + filepath);
        return false;
    }

    serializeJson(doc, file);
    file.close();

    LOG_INFO("MqttExportManager", "Config saved to: " + filepath);
    return true;
}

// ============================================================================
// Variable Export Rules
// ============================================================================

bool MqttExportManager::addExportRule(const String& varName, const String& mqttTopic,
                                      ExportAccessLevel access) {
    ExportRule rule;
    rule.variableName = varName;
    rule.mqttTopic = mqttTopic;
    rule.access = access;
    return configureExportRule(varName, rule);
}

bool MqttExportManager::configureExportRule(const String& varName, const ExportRule& rule) {
    // Check if this variable is a PLC-controlled output
    if (isPlcControlledOutput(varName)) {
        LOG_ERROR("MqttExportManager", "Cannot export PLC-controlled output: " + varName);
        return false;
    }

    exportRules[varName] = rule;

    // Subscribe to MQTT topic if writable
    if ((rule.access == ExportAccessLevel::WRITE_ONLY ||
         rule.access == ExportAccessLevel::READ_WRITE) && mqttManager) {
        mqttManager->subscribe(rule.mqttTopic.c_str());
        topicToVariable[rule.mqttTopic] = varName;
    }

    LOG_INFO("MqttExportManager", "Configured export: " + varName);
    return true;
}

bool MqttExportManager::removeExportRule(const String& varName) {
    auto it = exportRules.find(varName);
    if (it == exportRules.end()) {
        return false;
    }

    // Remove from topic mapping
    topicToVariable.erase(it->second.mqttTopic);
    exportRules.erase(it);

    LOG_INFO("MqttExportManager", "Removed export: " + varName);
    return true;
}

bool MqttExportManager::getExportRule(const String& varName, ExportRule& rule) {
    auto it = exportRules.find(varName);
    if (it == exportRules.end()) {
        return false;
    }
    rule = it->second;
    return true;
}

// ============================================================================
// Validation Rules
// ============================================================================

bool MqttExportManager::setValidationRange(const String& varName, double min, double max) {
    auto it = exportRules.find(varName);
    if (it == exportRules.end()) {
        return false;
    }

    it->second.validation.enabled = true;
    it->second.validation.minValue = min;
    it->second.validation.maxValue = max;

    LOG_INFO("MqttExportManager", "Set validation range for " + varName +
             ": " + String(min) + " - " + String(max));
    return true;
}

bool MqttExportManager::setValidationRegex(const String& varName, const String& regex) {
    auto it = exportRules.find(varName);
    if (it == exportRules.end()) {
        return false;
    }

    it->second.validation.enabled = true;
    it->second.validation.regex = regex;
    return true;
}

bool MqttExportManager::setCustomValidation(const String& varName,
                                            std::function<bool(const PlcValue&)> validator) {
    auto it = exportRules.find(varName);
    if (it == exportRules.end()) {
        return false;
    }

    it->second.validation.enabled = true;
    it->second.validation.customValidator = validator;
    return true;
}

// ============================================================================
// Command Definitions
// ============================================================================

bool MqttExportManager::registerCommand(const String& commandName, const String& mqttTopic,
                                        const String& plcHandler, const std::vector<String>& parameters) {
    CommandDefinition cmd;
    cmd.commandName = commandName;
    cmd.mqttTopic = mqttTopic;
    cmd.plcHandler = plcHandler;
    cmd.parameters = parameters;

    commands[commandName] = cmd;
    topicToCommand[mqttTopic] = commandName;

    // Subscribe to command topic
    if (mqttManager) {
        mqttManager->subscribe(mqttTopic.c_str());
    }

    LOG_INFO("MqttExportManager", "Registered command: " + commandName);
    return true;
}

bool MqttExportManager::registerCommand(const String& commandName, const String& mqttTopic,
                                        std::function<bool(const JsonObject&)> handler) {
    CommandDefinition cmd;
    cmd.commandName = commandName;
    cmd.mqttTopic = mqttTopic;
    cmd.handler = handler;

    commands[commandName] = cmd;
    topicToCommand[mqttTopic] = commandName;

    if (mqttManager) {
        mqttManager->subscribe(mqttTopic.c_str());
    }

    LOG_INFO("MqttExportManager", "Registered command with custom handler: " + commandName);
    return true;
}

bool MqttExportManager::unregisterCommand(const String& commandName) {
    auto it = commands.find(commandName);
    if (it == commands.end()) {
        return false;
    }

    topicToCommand.erase(it->second.mqttTopic);
    commands.erase(it);

    LOG_INFO("MqttExportManager", "Unregistered command: " + commandName);
    return true;
}

void MqttExportManager::setPlcCommandHandler(PlcCommandHandler handler) {
    plcCommandHandler = handler;
}

// ============================================================================
// Publishing
// ============================================================================

bool MqttExportManager::publishVariable(const String& varName, bool force) {
    if (!mqttManager || !variableRegistry) {
        return false;
    }

    auto it = exportRules.find(varName);
    if (it == exportRules.end()) {
        LOG_WARN("MqttExportManager", "No export rule for: " + varName);
        return false;
    }

    ExportRule& rule = it->second;

    // Check if variable is readable
    if (rule.access == ExportAccessLevel::WRITE_ONLY) {
        return false;
    }

    // Read current value
    VariableMetadata meta;
    if (!variableRegistry->getMetadata(varName, meta)) {
        return false;
    }

    PlcValue currentValue(meta.type);
    if (!variableRegistry->readVariable(varName, currentValue)) {
        return false;
    }

    // Check if should publish
    if (!force && !shouldPublish(rule, currentValue)) {
        return false;
    }

    // Convert value to MQTT payload
    String payload = valueToMqttPayload(currentValue);

    // Publish to MQTT
    // Note: MqttManager doesn't currently support retained/QoS parameters
    mqttManager->publish(rule.mqttTopic.c_str(), payload.c_str());
    bool success = true;  // Assume success since publish returns void

    if (success) {
        rule.lastPublish = millis();
        rule.lastValue = currentValue;
        stats.totalPublishes++;
        stats.lastActivity = millis();

        LOG_INFO("MqttExportManager", "Published " + varName + " = " + payload +
                 " to " + rule.mqttTopic);
    }

    return success;
}

void MqttExportManager::publishAllVariables(bool force) {
    for (const auto& pair : exportRules) {
        if (pair.second.access != ExportAccessLevel::WRITE_ONLY) {
            publishVariable(pair.first, force);
        }
    }
}

bool MqttExportManager::setAutoPublish(const String& varName, bool enabled) {
    auto it = exportRules.find(varName);
    if (it == exportRules.end()) {
        return false;
    }

    it->second.publishOnChange = enabled;
    return true;
}

// ============================================================================
// MQTT Message Handling
// ============================================================================

void MqttExportManager::handleMqttMessage(const String& topic, const String& payload) {
    stats.lastActivity = millis();

    // Check if this is a variable write
    auto varIt = topicToVariable.find(topic);
    if (varIt != topicToVariable.end()) {
        handleVariableWrite(varIt->second, payload);
        return;
    }

    // Check if this is a command
    auto cmdIt = topicToCommand.find(topic);
    if (cmdIt != topicToCommand.end()) {
        handleCommandExecution(cmdIt->second, payload);
        return;
    }

    LOG_WARN("MqttExportManager", "Unhandled MQTT topic: " + topic);
}

// ============================================================================
// Statistics
// ============================================================================

MqttExportManager::ExportStats MqttExportManager::getStatistics() const {
    return stats;
}

std::vector<String> MqttExportManager::getExportedVariables() const {
    std::vector<String> result;
    for (const auto& pair : exportRules) {
        result.push_back(pair.first);
    }
    return result;
}

std::vector<String> MqttExportManager::getRegisteredCommands() const {
    std::vector<String> result;
    for (const auto& pair : commands) {
        result.push_back(pair.first);
    }
    return result;
}

// ============================================================================
// Integration Hooks
// ============================================================================

void MqttExportManager::setMqttManager(MqttManager* manager) {
    mqttManager = manager;
}

void MqttExportManager::setVariableRegistry(VariableRegistry* registry) {
    variableRegistry = registry;

    // Setup change callback
    if (variableRegistry) {
        variableRegistry->onVariableChange([this](const String& varName,
                                                   const PlcValue& oldValue,
                                                   const PlcValue& newValue) {
            // Auto-publish if configured
            auto it = exportRules.find(varName);
            if (it != exportRules.end() && it->second.publishOnChange) {
                publishVariable(varName, false);
            }
        });
    }
}

void MqttExportManager::setPlcEngine(PlcEngine* engine) {
    plcEngine = engine;
}

void MqttExportManager::setDeviceRegistry(DeviceRegistry* registry) {
    deviceRegistry = registry;
}

// ============================================================================
// Private Helper Methods
// ============================================================================

bool MqttExportManager::isPlcControlledOutput(const String& varName) {
    if (!deviceRegistry) {
        return false; // No device registry, can't check
    }

    // Check all IO points to see if this variable is registered as an OUTPUT
    auto allIOPoints = deviceRegistry->getAllIOPoints();
    for (PlcIOPoint* ioPoint : allIOPoints) {
        if (ioPoint &&
            ioPoint->plcVarName == varName &&
            ioPoint->direction == IODirection::IO_OUTPUT &&
            !ioPoint->ownerProgram.isEmpty()) {
            // This variable is a PLC-controlled output
            return true;
        }
    }
    return false;
}

bool MqttExportManager::validateValue(const ExportRule& rule, const PlcValue& value) {
    if (!rule.validation.enabled) {
        return true;
    }

    // Custom validator takes precedence
    if (rule.validation.customValidator) {
        return rule.validation.customValidator(value);
    }

    // Numeric range validation
    if (value.type == PlcValueType::INT || value.type == PlcValueType::REAL) {
        double numValue = (value.type == PlcValueType::INT) ?
                         value.value.i16Val : value.value.fVal;

        if (numValue < rule.validation.minValue || numValue > rule.validation.maxValue) {
            LOG_WARN("MqttExportManager", "Validation failed: value out of range");
            return false;
        }
    }

    // TODO: Regex validation for strings

    return true;
}

bool MqttExportManager::executeCommand(const CommandDefinition& cmd, const JsonObject& params) {
    LOG_INFO("MqttExportManager", "Executing command: " + cmd.commandName);

    // Use custom handler if available
    if (cmd.handler) {
        return cmd.handler(params);
    }

    // Use PLC handler
    if (!cmd.plcHandler.isEmpty() && plcCommandHandler) {
        bool success = plcCommandHandler(cmd.plcHandler, params);
        if (success) {
            stats.totalCommandExecutions++;
        }
        return success;
    }

    LOG_ERROR("MqttExportManager", "No handler for command: " + cmd.commandName);
    return false;
}

void MqttExportManager::subscribeToTopics() {
    if (!mqttManager) {
        return;
    }

    // Subscribe to writable variable topics
    for (const auto& pair : exportRules) {
        if (pair.second.access == ExportAccessLevel::WRITE_ONLY ||
            pair.second.access == ExportAccessLevel::READ_WRITE) {
            mqttManager->subscribe(pair.second.mqttTopic.c_str());
        }
    }

    // Subscribe to command topics
    for (const auto& pair : commands) {
        mqttManager->subscribe(pair.second.mqttTopic.c_str());
    }
}

void MqttExportManager::handleVariableWrite(const String& varName, const String& payload) {
    LOG_INFO("MqttExportManager", "Variable write: " + varName + " = " + payload);

    if (!variableRegistry) {
        return;
    }

    // Get export rule
    auto it = exportRules.find(varName);
    if (it == exportRules.end()) {
        return;
    }

    const ExportRule& rule = it->second;

    // Get variable metadata
    VariableMetadata meta;
    if (!variableRegistry->getMetadata(varName, meta)) {
        LOG_ERROR("MqttExportManager", "Variable not found: " + varName);
        return;
    }

    // Parse payload to value
    PlcValue value(meta.type);
    if (!mqttPayloadToValue(payload, meta.type, value)) {
        LOG_ERROR("MqttExportManager", "Failed to parse payload");
        return;
    }

    // Validate
    if (!validateValue(rule, value)) {
        LOG_ERROR("MqttExportManager", "Validation failed for: " + varName);
        return;
    }

    // Write to variable
    if (variableRegistry->writeVariable(varName, value)) {
        stats.totalWrites++;
        LOG_INFO("MqttExportManager", "Successfully wrote: " + varName);
    }
}

void MqttExportManager::handleCommandExecution(const String& commandName, const String& payload) {
    LOG_INFO("MqttExportManager", "Command execution: " + commandName);

    auto it = commands.find(commandName);
    if (it == commands.end()) {
        return;
    }

    // Parse JSON payload
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);
    if (error) {
        LOG_ERROR("MqttExportManager", "JSON parse error: " + String(error.c_str()));
        return;
    }

    executeCommand(it->second, doc.as<JsonObject>());
}

String MqttExportManager::valueToMqttPayload(const PlcValue& value) {
    switch (value.type) {
        case PlcValueType::BOOL:
            return value.value.bVal ? "true" : "false";
        case PlcValueType::INT:
            return String(value.value.i16Val);
        case PlcValueType::REAL:
            return String(value.value.fVal, 2);
        case PlcValueType::STRING_TYPE:
            return String(value.value.sVal);
        default:
            return "";
    }
}

bool MqttExportManager::mqttPayloadToValue(const String& payload, PlcValueType type, PlcValue& value) {
    value.type = type;

    switch (type) {
        case PlcValueType::BOOL:
            value.value.bVal = (payload == "true" || payload == "1" || payload == "ON");
            return true;
        case PlcValueType::INT:
            value.value.i16Val = payload.toInt();
            return true;
        case PlcValueType::REAL:
            value.value.fVal = payload.toFloat();
            return true;
        case PlcValueType::STRING_TYPE:
            strncpy(value.value.sVal, payload.c_str(), sizeof(value.value.sVal) - 1);
            value.value.sVal[sizeof(value.value.sVal) - 1] = '\0';
            return true;
        default:
            return false;
    }
}

bool MqttExportManager::shouldPublish(const ExportRule& rule, const PlcValue& currentValue) {
    // Check throttling
    if (rule.minPublishInterval > 0) {
        if (millis() - rule.lastPublish < rule.minPublishInterval) {
            return false;
        }
    }

    // Check if value changed (simple comparison)
    if (rule.lastValue.type == currentValue.type) {
        switch (currentValue.type) {
            case PlcValueType::BOOL:
                if (rule.lastValue.value.bVal == currentValue.value.bVal) {
                    return false;
                }
                break;
            case PlcValueType::INT:
                if (rule.lastValue.value.i16Val == currentValue.value.i16Val) {
                    return false;
                }
                break;
            case PlcValueType::REAL:
                if (abs(rule.lastValue.value.fVal - currentValue.value.fVal) < 0.01) {
                    return false;
                }
                break;
            case PlcValueType::STRING_TYPE:
                if (strcmp(rule.lastValue.value.sVal, currentValue.value.sVal) == 0) {
                    return false;
                }
                break;
            default:
                break;
        }
    }

    return true;
}
