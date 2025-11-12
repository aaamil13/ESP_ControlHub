#ifndef MODULE_H
#define MODULE_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>

/**
 * Module Types
 */
enum class ModuleType {
    PROTOCOL,      // Communication protocols (MQTT, WiFi, RF433, Zigbee, etc.)
    STORAGE,       // Storage systems
    EXPORT,        // Export managers (MQTT/Mesh export)
    UI,            // User interface components
    APP,           // Applications
    CORE           // Core components (cannot be disabled)
};

/**
 * Module States
 */
enum class ModuleState {
    MODULE_DISABLED,      // Module is disabled and not running
    MODULE_ENABLED,       // Module is enabled (config loaded)
    MODULE_STARTING,      // Module is starting up
    MODULE_RUNNING,       // Module is fully operational
    MODULE_STOPPING,      // Module is shutting down
    MODULE_ERROR          // Module encountered an error
};

/**
 * Module Capabilities - describes what a module can/cannot do
 */
struct ModuleCapabilities {
    bool canDisable;                // Can this module be disabled at runtime?
    bool requiresReboot;            // Does enable/disable require system reboot?
    bool hasWebUI;                  // Does module provide web UI endpoints?
    bool hasSecurity;               // Does module handle sensitive data?
    size_t estimatedMemory;         // Estimated memory usage in bytes
    std::vector<String> dependencies; // List of module names this depends on
    String hardwareRequirement;     // Hardware requirement (e.g., "ESP32-C6", "ESP32-H2")

    ModuleCapabilities()
        : canDisable(true),
          requiresReboot(false),
          hasWebUI(false),
          hasSecurity(false),
          estimatedMemory(0),
          hardwareRequirement("") {}
};

/**
 * Base Module Interface
 *
 * All modules (protocols, exports, apps, etc.) should implement this interface
 * to be managed by the ModuleManager.
 */
class Module {
public:
    virtual ~Module() {}

    // ============================================================================
    // Module Lifecycle
    // ============================================================================

    /**
     * Initialize module - called once at system startup
     * Module should prepare resources but not start operations yet
     */
    virtual bool initialize() = 0;

    /**
     * Start module operations
     * Module begins its main functionality
     */
    virtual bool start() = 0;

    /**
     * Stop module operations
     * Module should gracefully shut down and free resources
     */
    virtual bool stop() = 0;

    /**
     * Module loop - called repeatedly when module is RUNNING
     */
    virtual void loop() = 0;

    // ============================================================================
    // Module Information
    // ============================================================================

    /**
     * Get unique module name (e.g., "mqtt", "wifi", "rf433")
     */
    virtual String getName() const = 0;

    /**
     * Get human-readable display name (e.g., "MQTT Protocol", "WiFi Devices")
     */
    virtual String getDisplayName() const = 0;

    /**
     * Get module version string
     */
    virtual String getVersion() const = 0;

    /**
     * Get module type
     */
    virtual ModuleType getType() const = 0;

    /**
     * Get module capabilities
     */
    virtual ModuleCapabilities getCapabilities() const = 0;

    /**
     * Get module description
     */
    virtual String getDescription() const = 0;

    // ============================================================================
    // Module State
    // ============================================================================

    /**
     * Get current module state
     */
    virtual ModuleState getState() const = 0;

    /**
     * Get human-readable status message
     */
    virtual String getStatusMessage() const = 0;

    /**
     * Check if module is currently enabled
     */
    virtual bool isEnabled() const {
        ModuleState state = getState();
        return state == ModuleState::MODULE_ENABLED ||
               state == ModuleState::MODULE_STARTING ||
               state == ModuleState::MODULE_RUNNING;
    }

    /**
     * Check if module is running
     */
    virtual bool isRunning() const {
        return getState() == ModuleState::MODULE_RUNNING;
    }

    /**
     * Check if hardware requirements are met
     */
    virtual bool isHardwareCompatible() const {
        ModuleCapabilities caps = getCapabilities();
        if (caps.hardwareRequirement.isEmpty()) {
            return true; // No specific requirement
        }

        // Check hardware compatibility
        #ifdef ESP32_C6
        return caps.hardwareRequirement.indexOf("ESP32-C6") >= 0;
        #elif defined(ESP32_H2)
        return caps.hardwareRequirement.indexOf("ESP32-H2") >= 0;
        #elif defined(ESP32_S3)
        return caps.hardwareRequirement.indexOf("ESP32-S3") >= 0;
        #elif defined(ESP32_S2)
        return caps.hardwareRequirement.indexOf("ESP32-S2") >= 0;
        #else
        return caps.hardwareRequirement.indexOf("ESP32") >= 0;
        #endif
    }

    // ============================================================================
    // Configuration
    // ============================================================================

    /**
     * Configure module from JSON object
     */
    virtual bool configure(const JsonObject& config) = 0;

    /**
     * Get current module configuration as JSON
     */
    virtual JsonDocument getConfig() const = 0;

    /**
     * Validate configuration without applying it
     */
    virtual bool validateConfig(const JsonObject& config) const {
        // Default implementation - assume valid
        return true;
    }

    // ============================================================================
    // Statistics & Monitoring
    // ============================================================================

    /**
     * Get module statistics (connections, messages, errors, etc.)
     */
    virtual JsonDocument getStatistics() const = 0;

    /**
     * Get current memory usage in bytes
     */
    virtual size_t getMemoryUsage() const {
        // Default implementation - return estimate
        return getCapabilities().estimatedMemory;
    }

    /**
     * Get uptime in milliseconds (time since module started)
     */
    virtual unsigned long getUptime() const {
        return 0; // Override in derived classes
    }

    // ============================================================================
    // Health Checks
    // ============================================================================

    /**
     * Perform health check
     * @return true if module is healthy, false if there are issues
     */
    virtual bool healthCheck() const {
        // Default implementation
        ModuleState state = getState();
        return state == ModuleState::MODULE_RUNNING || state == ModuleState::MODULE_ENABLED;
    }

    /**
     * Get last error message (if any)
     */
    virtual String getLastError() const {
        return "";
    }
};

/**
 * Convenience function to convert ModuleType to string
 */
inline String moduleTypeToString(ModuleType type) {
    switch (type) {
        case ModuleType::PROTOCOL: return "Protocol";
        case ModuleType::STORAGE:  return "Storage";
        case ModuleType::EXPORT:   return "Export";
        case ModuleType::UI:       return "UI";
        case ModuleType::APP:      return "Application";
        case ModuleType::CORE:     return "Core";
        default:                   return "Unknown";
    }
}

/**
 * Convenience function to convert ModuleState to string
 */
inline String moduleStateToString(ModuleState state) {
    switch (state) {
        case ModuleState::MODULE_DISABLED:  return "Disabled";
        case ModuleState::MODULE_ENABLED:   return "Enabled";
        case ModuleState::MODULE_STARTING:  return "Starting";
        case ModuleState::MODULE_RUNNING:   return "Running";
        case ModuleState::MODULE_STOPPING:  return "Stopping";
        case ModuleState::MODULE_ERROR:     return "Error";
        default:                            return "Unknown";
    }
}

#endif // MODULE_H
