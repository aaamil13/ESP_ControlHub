#ifndef MODULE_MANAGER_H
#define MODULE_MANAGER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>
#include <map>
#include "Module.h"
#include "StreamLogger.h"

/**
 * ModuleManager - Central manager for all system modules
 *
 * Responsibilities:
 * - Register and manage module lifecycle
 * - Enable/disable modules at runtime
 * - Handle module dependencies
 * - Persist module configuration
 * - Provide module status information
 */
class ModuleManager {
public:
    ModuleManager();
    ~ModuleManager();

    // ============================================================================
    // Lifecycle
    // ============================================================================

    /**
     * Initialize the module manager
     * Loads configuration from storage
     */
    bool initialize();

    /**
     * Start all enabled modules
     */
    bool startAll();

    /**
     * Stop all running modules
     */
    void stopAll();

    /**
     * Loop function - calls loop() on all running modules
     */
    void loop();

    // ============================================================================
    // Module Registration
    // ============================================================================

    /**
     * Register a module with the manager
     * Module must remain valid for the lifetime of ModuleManager
     */
    bool registerModule(Module* module);

    /**
     * Unregister a module
     */
    bool unregisterModule(const String& moduleName);

    /**
     * Get a registered module by name
     */
    Module* getModule(const String& moduleName);

    /**
     * Get a registered module by name (const version)
     */
    const Module* getModule(const String& moduleName) const;

    /**
     * Get list of all registered module names
     */
    std::vector<String> getModuleList() const;

    /**
     * Get list of enabled module names
     */
    std::vector<String> getEnabledModules() const;

    /**
     * Get list of running module names
     */
    std::vector<String> getRunningModules() const;

    // ============================================================================
    // Module Control
    // ============================================================================

    /**
     * Enable and start a module
     * @param moduleName Name of the module to enable
     * @param saveConfig If true, save the enabled state to configuration
     * @return true if successful
     */
    bool enableModule(const String& moduleName, bool saveConfig = true);

    /**
     * Disable and stop a module
     * @param moduleName Name of the module to disable
     * @param saveConfig If true, save the disabled state to configuration
     * @return true if successful
     */
    bool disableModule(const String& moduleName, bool saveConfig = true);

    /**
     * Restart a module (stop and start)
     */
    bool restartModule(const String& moduleName);

    /**
     * Get current state of a module
     */
    ModuleState getModuleState(const String& moduleName) const;

    // ============================================================================
    // Configuration
    // ============================================================================

    /**
     * Load module configuration from file
     * Default path: /config/modules.json
     */
    bool loadConfiguration(const String& configPath = "/config/modules.json");

    /**
     * Save current module configuration to file
     */
    bool saveConfiguration(const String& configPath = "/config/modules.json");

    /**
     * Set module enabled state (without starting/stopping)
     * Used during configuration loading
     */
    void setModuleEnabled(const String& moduleName, bool enabled);

    /**
     * Check if a module is configured to be enabled
     */
    bool isModuleEnabled(const String& moduleName) const;

    /**
     * Set auto-start flag for a module
     */
    void setModuleAutoStart(const String& moduleName, bool autoStart);

    /**
     * Check if module should auto-start on boot
     */
    bool isModuleAutoStart(const String& moduleName) const;

    // ============================================================================
    // Dependencies
    // ============================================================================

    /**
     * Check if all dependencies for a module are satisfied
     */
    bool checkDependencies(const String& moduleName) const;

    /**
     * Get list of modules that depend on the specified module
     */
    std::vector<String> getDependentModules(const String& moduleName) const;

    /**
     * Get list of missing dependencies for a module
     */
    std::vector<String> getMissingDependencies(const String& moduleName) const;

    // ============================================================================
    // Security
    // ============================================================================

    /**
     * Check if module operations require authentication
     */
    bool requiresAuthentication() const;

    /**
     * Set authentication requirement
     */
    void setRequireAuthentication(bool require);

    /**
     * Check if user can modify a module
     * @param moduleName Module to check
     * @param user Username to check (empty = no user)
     * @return true if allowed
     */
    bool canModifyModule(const String& moduleName, const String& user = "") const;

    /**
     * Log a module operation for audit trail
     */
    void logOperation(const String& moduleName, const String& operation,
                     const String& user = "", bool success = true);

    // ============================================================================
    // Statistics & Monitoring
    // ============================================================================

    /**
     * Get total memory usage of all modules
     */
    size_t getTotalMemoryUsage() const;

    /**
     * Get estimated memory savings from disabled modules
     */
    size_t getMemorySavings() const;

    /**
     * Get system-wide module status
     */
    JsonDocument getSystemStatus() const;

    /**
     * Get summary of all modules (for REST API)
     */
    JsonDocument getModuleSummary() const;

    /**
     * Get detailed information about a specific module
     */
    JsonDocument getModuleInfo(const String& moduleName) const;

    /**
     * Get statistics for a specific module
     */
    JsonDocument getModuleStatistics(const String& moduleName) const;

    /**
     * Count modules by type
     */
    int countModulesByType(ModuleType type) const;

    /**
     * Count modules by state
     */
    int countModulesByState(ModuleState state) const;

    // ============================================================================
    // Health Checks
    // ============================================================================

    /**
     * Perform health check on all modules
     * @return Number of unhealthy modules
     */
    int healthCheckAll();

    /**
     * Check if a specific module is healthy
     */
    bool isModuleHealthy(const String& moduleName) const;

    /**
     * Get list of unhealthy modules
     */
    std::vector<String> getUnhealthyModules() const;

private:
    // Module registry
    std::map<String, Module*> modules;

    // Module configuration
    struct ModuleConfig {
        bool enabled;
        bool autoStart;
        String reason;  // Reason for disabled state (for documentation)
    };
    std::map<String, ModuleConfig> moduleConfigs;

    // Security settings
    bool requireAuth;
    std::vector<String> allowedUsers;

    // Internal helper methods
    bool startModule(Module* module);
    bool stopModule(Module* module);
    bool initializeModule(Module* module);
    void logModuleEvent(const String& moduleName, const String& event, const String& details = "");
};

#endif // MODULE_MANAGER_H
