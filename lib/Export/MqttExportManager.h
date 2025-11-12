#ifndef MQTT_EXPORT_MANAGER_H
#define MQTT_EXPORT_MANAGER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <map>
#include <vector>
#include <functional>
#include "../Protocols/Mqtt/MqttManager.h"
#include "../Export/VariableRegistry.h"
#include "../PlcEngine/Engine/PlcEngine.h"
#include "../Devices/DeviceRegistry.h"

/**
 * MqttExportManager - Hybrid MQTT variable and command export
 *
 * Provides three levels of MQTT integration:
 * 1. Read-only variables (sensors, status) - auto-publish on change
 * 2. Read-write variables (setpoints, config) - bidirectional sync with validation
 * 3. Commands (PLC functions) - execute business logic via MQTT
 *
 * Features:
 * - Configurable via JSON
 * - Access control (read/write/execute)
 * - Value validation (min/max, custom rules)
 * - Publish throttling and filtering
 * - Command execution with parameters
 * - Retained messages support
 * - QoS configuration
 *
 * Configuration example:
 * {
 *   "mqtt_export": {
 *     "variables": {
 *       "main.temperature": {
 *         "topic": "sensors/temperature",
 *         "access": "read",
 *         "publish_on_change": true,
 *         "min_interval_ms": 5000
 *       }
 *     },
 *     "commands": {
 *       "activateScene": {
 *         "topic": "commands/scene",
 *         "handler": "plc_activateScene",
 *         "parameters": ["scene_name"]
 *       }
 *     }
 *   }
 * }
 */

enum class ExportAccessLevel {
    READ_ONLY,      // Variable is published to MQTT, cannot be written from MQTT
    WRITE_ONLY,     // Variable can be written from MQTT, not published
    READ_WRITE,     // Bidirectional sync
    COMMAND         // Execute PLC function
};

struct ValidationRule {
    bool enabled;
    double minValue;
    double maxValue;
    String regex;
    std::function<bool(const PlcValue&)> customValidator;

    ValidationRule() : enabled(false), minValue(0), maxValue(0) {}
};

struct ExportRule {
    String variableName;       // Full variable name (e.g., "main.temperature")
    String mqttTopic;          // MQTT topic for this variable
    ExportAccessLevel access;  // Access level
    bool publishOnChange;      // Auto-publish when value changes
    unsigned long minPublishInterval; // Minimum time between publishes (ms)
    bool retained;             // MQTT retained flag
    int qos;                   // MQTT QoS (0, 1, 2)
    ValidationRule validation; // Validation rules for writes
    unsigned long lastPublish; // Timestamp of last publish
    PlcValue lastValue;        // Last published value (for change detection)

    ExportRule()
        : access(ExportAccessLevel::READ_ONLY),
          publishOnChange(true),
          minPublishInterval(0),
          retained(false),
          qos(0),
          lastPublish(0),
          lastValue(PlcValueType::BOOL) {}
};

struct CommandDefinition {
    String commandName;        // Command name
    String mqttTopic;          // MQTT topic to subscribe to
    String plcHandler;         // PLC function name to call
    std::vector<String> parameters; // Expected parameter names
    bool requireAuth;          // Require authentication
    std::function<bool(const JsonObject&)> handler; // Custom handler function

    CommandDefinition() : requireAuth(false) {}
};

// Command handler callback type
using PlcCommandHandler = std::function<bool(const String& command, const JsonObject& params)>;

class MqttExportManager {
public:
    MqttExportManager();
    ~MqttExportManager();

    void begin();
    void loop();

    // ============================================================================
    // Configuration
    // ============================================================================

    /**
     * Load export configuration from JSON
     * @param config JSON object with export rules
     */
    bool loadConfig(const JsonObject& config);

    /**
     * Load configuration from file
     * @param filepath Path to JSON config file
     */
    bool loadConfigFromFile(const String& filepath);

    /**
     * Save current configuration to file
     */
    bool saveConfig(const String& filepath);

    // ============================================================================
    // Variable Export Rules
    // ============================================================================

    /**
     * Add export rule for a variable
     * @param varName Variable name (e.g., "main.temperature")
     * @param mqttTopic MQTT topic
     * @param access Access level
     */
    bool addExportRule(const String& varName, const String& mqttTopic,
                      ExportAccessLevel access = ExportAccessLevel::READ_ONLY);

    /**
     * Configure export rule with full options
     */
    bool configureExportRule(const String& varName, const ExportRule& rule);

    /**
     * Remove export rule
     */
    bool removeExportRule(const String& varName);

    /**
     * Get export rule for variable
     */
    bool getExportRule(const String& varName, ExportRule& rule);

    // ============================================================================
    // Validation Rules
    // ============================================================================

    /**
     * Set numeric validation range for variable
     */
    bool setValidationRange(const String& varName, double min, double max);

    /**
     * Set regex validation for string variable
     */
    bool setValidationRegex(const String& varName, const String& regex);

    /**
     * Set custom validation function
     */
    bool setCustomValidation(const String& varName,
                            std::function<bool(const PlcValue&)> validator);

    // ============================================================================
    // Command Definitions
    // ============================================================================

    /**
     * Register a PLC command
     * @param commandName Command identifier
     * @param mqttTopic MQTT topic to subscribe to
     * @param plcHandler PLC function name
     * @param parameters Expected parameter names
     */
    bool registerCommand(const String& commandName, const String& mqttTopic,
                        const String& plcHandler, const std::vector<String>& parameters = {});

    /**
     * Register command with custom handler
     */
    bool registerCommand(const String& commandName, const String& mqttTopic,
                        std::function<bool(const JsonObject&)> handler);

    /**
     * Unregister command
     */
    bool unregisterCommand(const String& commandName);

    /**
     * Set PLC command handler callback
     */
    void setPlcCommandHandler(PlcCommandHandler handler);

    // ============================================================================
    // Publishing
    // ============================================================================

    /**
     * Publish variable value to MQTT (manual)
     * @param varName Variable name
     * @param force Force publish even if not changed or throttled
     */
    bool publishVariable(const String& varName, bool force = false);

    /**
     * Publish all configured variables
     */
    void publishAllVariables(bool force = false);

    /**
     * Enable/disable auto-publish for a variable
     */
    bool setAutoPublish(const String& varName, bool enabled);

    // ============================================================================
    // MQTT Message Handling
    // ============================================================================

    /**
     * Handle incoming MQTT message
     * Called by MQTT callback
     */
    void handleMqttMessage(const String& topic, const String& payload);

    // ============================================================================
    // Statistics and Monitoring
    // ============================================================================

    struct ExportStats {
        int totalExports;
        int readOnlyVars;
        int readWriteVars;
        int commands;
        unsigned long totalPublishes;
        unsigned long totalWrites;
        unsigned long totalCommandExecutions;
        unsigned long lastActivity;
    };

    ExportStats getStatistics() const;

    /**
     * Get list of all exported variables
     */
    std::vector<String> getExportedVariables() const;

    /**
     * Get list of all registered commands
     */
    std::vector<String> getRegisteredCommands() const;

    // ============================================================================
    // Integration Hooks
    // ============================================================================

    void setMqttManager(MqttManager* manager);
    void setVariableRegistry(VariableRegistry* registry);
    void setPlcEngine(PlcEngine* engine);
    void setDeviceRegistry(DeviceRegistry* registry);

private:
    // Integration references
    MqttManager* mqttManager;
    VariableRegistry* variableRegistry;
    PlcEngine* plcEngine;
    DeviceRegistry* deviceRegistry;

    // Export rules and command definitions
    std::map<String, ExportRule> exportRules;        // varName -> rule
    std::map<String, CommandDefinition> commands;    // commandName -> definition
    std::map<String, String> topicToVariable;        // mqttTopic -> varName (for writes)
    std::map<String, String> topicToCommand;         // mqttTopic -> commandName

    // Callbacks
    PlcCommandHandler plcCommandHandler;

    // Statistics
    ExportStats stats;

    // Helper methods
    bool validateValue(const ExportRule& rule, const PlcValue& value);
    bool executeCommand(const CommandDefinition& cmd, const JsonObject& params);
    void subscribeToTopics();
    void handleVariableWrite(const String& varName, const String& payload);
    void handleCommandExecution(const String& commandName, const String& payload);
    String valueToMqttPayload(const PlcValue& value);
    bool mqttPayloadToValue(const String& payload, PlcValueType type, PlcValue& value);
    bool shouldPublish(const ExportRule& rule, const PlcValue& currentValue);
    bool isPlcControlledOutput(const String& varName); // Check if variable is a PLC output

    // Variable change callback
    static void onVariableChanged(const String& varName, const PlcValue& oldValue,
                                 const PlcValue& newValue, void* context);
};

#endif // MQTT_EXPORT_MANAGER_H
