#ifndef MESH_EXPORT_MANAGER_H
#define MESH_EXPORT_MANAGER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <painlessMesh.h>
#include <map>
#include <vector>
#include <functional>
#include "../Export/VariableRegistry.h"
#include "../PlcEngine/Engine/PlcEngine.h"

/**
 * MeshExportManager - Mesh variable sharing system
 *
 * Enables variable sharing between ESP hubs in a mesh network.
 * Similar to MqttExportManager but for mesh communication.
 *
 * Features:
 * - Publish local variables to mesh network
 * - Subscribe to remote hub variables
 * - Auto-sync on value change
 * - Namespace support (hub_id.program.variable)
 * - Configurable via JSON
 *
 * Configuration example:
 * {
 *   "mesh_export": {
 *     "publish": {
 *       "main.temperature": {
 *         "sync_interval_ms": 10000,
 *         "sync_on_change": true,
 *         "min_change_threshold": 0.5
 *       }
 *     },
 *     "subscribe": {
 *       "remote_hub_id.main.pump_status": {
 *         "local_alias": "main.remote_pump",
 *         "timeout_ms": 30000
 *       }
 *     }
 *   }
 * }
 */

enum class MeshSyncMode {
    PUBLISH,        // Publish local variable to mesh
    SUBSCRIBE,      // Subscribe to remote variable
    BIDIRECTIONAL   // Both publish and subscribe
};

struct MeshPublishRule {
    String variableName;           // Local variable name
    unsigned long syncInterval;    // Sync interval in ms
    bool syncOnChange;             // Auto-sync on value change
    double minChangeThreshold;     // Minimum change to trigger sync (for numeric)
    unsigned long lastSync;        // Timestamp of last sync
    PlcValue lastValue;            // Last synced value

    MeshPublishRule()
        : syncInterval(10000),
          syncOnChange(true),
          minChangeThreshold(0.0),
          lastSync(0),
          lastValue(PlcValueType::BOOL) {}
};

struct MeshSubscribeRule {
    String remoteVariable;         // Full remote name (hub_id.program.variable)
    String remoteHubId;            // Remote hub ID
    String remoteVarName;          // Variable name on remote hub
    String localAlias;             // Local variable name (alias)
    unsigned long timeoutMs;       // Max time without update before marked stale
    unsigned long lastReceived;    // Timestamp of last received value
    bool isStale;                  // True if timeout exceeded

    MeshSubscribeRule()
        : timeoutMs(30000),
          lastReceived(0),
          isStale(true) {}
};

class MeshExportManager {
public:
    MeshExportManager();
    ~MeshExportManager();

    void begin();
    void loop();

    // ============================================================================
    // Configuration
    // ============================================================================

    /**
     * Load mesh export configuration from JSON
     */
    bool loadConfig(const JsonObject& config);

    /**
     * Load configuration from file
     */
    bool loadConfigFromFile(const String& filepath);

    /**
     * Save current configuration to file
     */
    bool saveConfig(const String& filepath);

    // ============================================================================
    // Publish Rules
    // ============================================================================

    /**
     * Add publish rule for a local variable
     * @param varName Local variable name
     * @param syncInterval Sync interval in milliseconds
     * @param syncOnChange Auto-sync on value change
     */
    bool addPublishRule(const String& varName, unsigned long syncInterval = 10000,
                       bool syncOnChange = true);

    /**
     * Remove publish rule
     */
    bool removePublishRule(const String& varName);

    /**
     * Publish variable immediately
     */
    bool publishVariable(const String& varName, bool force = false);

    /**
     * Publish all configured variables
     */
    void publishAllVariables(bool force = false);

    // ============================================================================
    // Subscribe Rules
    // ============================================================================

    /**
     * Subscribe to remote variable
     * @param remoteVariable Full remote name (hub_id.program.variable)
     * @param localAlias Local variable name (will be created if doesn't exist)
     * @param timeoutMs Timeout in milliseconds
     */
    bool addSubscribeRule(const String& remoteVariable, const String& localAlias,
                         unsigned long timeoutMs = 30000);

    /**
     * Remove subscribe rule
     */
    bool removeSubscribeRule(const String& remoteVariable);

    /**
     * Request variable value from remote hub
     */
    bool requestRemoteVariable(const String& remoteVariable);

    // ============================================================================
    // Mesh Message Handling
    // ============================================================================

    /**
     * Handle incoming mesh variable message
     * Called from mesh callback
     */
    void handleVariableSync(uint32_t fromNodeId, const JsonObject& payload);

    /**
     * Handle variable request from remote hub
     */
    void handleVariableRequest(uint32_t fromNodeId, const JsonObject& payload);

    // ============================================================================
    // Statistics and Monitoring
    // ============================================================================

    struct MeshExportStats {
        int totalPublishRules;
        int totalSubscribeRules;
        unsigned long totalPublishes;
        unsigned long totalReceived;
        unsigned long totalRequests;
        int staleSubscriptions;
        unsigned long lastActivity;
    };

    MeshExportStats getStatistics() const;

    /**
     * Get list of all published variables
     */
    std::vector<String> getPublishedVariables() const;

    /**
     * Get list of all subscribed variables
     */
    std::vector<String> getSubscribedVariables() const;

    /**
     * Check if subscribed variable is stale (timeout exceeded)
     */
    bool isSubscriptionStale(const String& remoteVariable) const;

    // ============================================================================
    // Integration Hooks
    // ============================================================================

    void setMesh(painlessMesh* mesh);
    void setVariableRegistry(VariableRegistry* registry);
    void setPlcEngine(PlcEngine* engine);
    void setLocalHubId(const String& hubId);

    String getLocalHubId() const { return localHubId; }

private:
    // Integration references
    painlessMesh* mesh;
    VariableRegistry* variableRegistry;
    PlcEngine* plcEngine;
    String localHubId;

    // Export rules
    std::map<String, MeshPublishRule> publishRules;     // varName -> rule
    std::map<String, MeshSubscribeRule> subscribeRules; // remoteVariable -> rule

    // Statistics
    MeshExportStats stats;

    // Helper methods
    bool shouldPublish(const MeshPublishRule& rule, const PlcValue& currentValue);
    void sendVariableToMesh(const String& varName, const PlcValue& value);
    void sendVariableRequest(uint32_t targetNodeId, const String& varName);
    void updateSubscribedVariable(const String& remoteVariable, const PlcValue& value);
    void checkStaleSubscriptions();

    String buildFullRemoteName(const String& hubId, const String& varName);
    bool parseRemoteName(const String& fullName, String& hubId, String& varName);
    String getHubIdFromNodeId(uint32_t nodeId);  // May need mesh device manager

    // Change callbacks
    static void onVariableChanged(const String& varName, const PlcValue& oldValue,
                                 const PlcValue& newValue, void* context);
};

#endif // MESH_EXPORT_MANAGER_H
