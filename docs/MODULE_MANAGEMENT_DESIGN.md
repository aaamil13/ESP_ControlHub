# Module Management System - Design Document

## Overview

Runtime module management system that allows dynamic enabling/disabling of protocol modules and other components for security, resource optimization, and flexibility.

## Goals

1. **Security** - Disable unused protocols to reduce attack surface
2. **Resource Optimization** - Free memory by disabling unused modules
3. **Flexibility** - Enable/disable modules without recompiling
4. **User Control** - Web UI for module management
5. **Persistence** - Remember module states across reboots

## Architecture

### Components

```
┌─────────────────────────────────────────────────────────────┐
│                        EspHub Core                          │
│                                                             │
│  ┌─────────────────────────────────────────────────────┐  │
│  │              ModuleManager                          │  │
│  │  - Register modules                                 │  │
│  │  - Enable/Disable modules                           │  │
│  │  - Query module status                              │  │
│  │  - Persist configuration                            │  │
│  └─────────────────────────────────────────────────────┘  │
│                          │                                  │
│         ┌────────────────┼────────────────┐               │
│         │                │                │               │
│    ┌────▼─────┐    ┌────▼─────┐    ┌────▼─────┐        │
│    │  Module  │    │  Module  │    │  Module  │        │
│    │ (MQTT)   │    │ (WiFi)   │    │ (RF433)  │        │
│    └──────────┘    └──────────┘    └──────────┘        │
└─────────────────────────────────────────────────────────────┘
```

## Module Interface

### Base Module Class

```cpp
enum class ModuleType {
    PROTOCOL,      // Communication protocols (MQTT, WiFi, etc.)
    STORAGE,       // Storage systems
    EXPORT,        // Export managers
    UI,            // User interface components
    APP,           // Applications
    CORE           // Core components (cannot be disabled)
};

enum class ModuleState {
    DISABLED,      // Module is disabled
    ENABLED,       // Module is enabled
    STARTING,      // Module is starting up
    RUNNING,       // Module is running
    STOPPING,      // Module is shutting down
    ERROR          // Module encountered an error
};

struct ModuleCapabilities {
    bool canDisable;           // Can this module be disabled?
    bool requiresReboot;       // Does enable/disable require reboot?
    bool hasWebUI;             // Does module have web UI?
    bool hasSecurity;          // Does module handle sensitive data?
    size_t memoryUsage;        // Estimated memory usage (bytes)
    std::vector<String> dependencies; // Module dependencies
};

class Module {
public:
    virtual ~Module() {}

    // Module lifecycle
    virtual bool initialize() = 0;
    virtual bool start() = 0;
    virtual bool stop() = 0;
    virtual void loop() = 0;

    // Module information
    virtual String getName() const = 0;
    virtual String getVersion() const = 0;
    virtual ModuleType getType() const = 0;
    virtual ModuleCapabilities getCapabilities() const = 0;

    // Module state
    virtual ModuleState getState() const = 0;
    virtual String getStatusMessage() const = 0;

    // Configuration
    virtual bool configure(const JsonObject& config) = 0;
    virtual JsonDocument getConfig() const = 0;

    // Statistics
    virtual JsonDocument getStatistics() const = 0;
};
```

## ModuleManager

### Responsibilities

1. **Registration** - Register available modules
2. **Lifecycle** - Manage module start/stop
3. **Configuration** - Load/save module states
4. **Dependencies** - Handle module dependencies
5. **Security** - Control access to module operations

### Key Methods

```cpp
class ModuleManager {
public:
    // Registration
    bool registerModule(Module* module);
    bool unregisterModule(const String& moduleName);

    // Module control
    bool enableModule(const String& moduleName);
    bool disableModule(const String& moduleName);
    bool restartModule(const String& moduleName);

    // Query
    Module* getModule(const String& moduleName);
    std::vector<String> getModuleList() const;
    std::vector<String> getEnabledModules() const;
    ModuleState getModuleState(const String& moduleName) const;

    // Configuration
    bool loadConfiguration();
    bool saveConfiguration();
    bool setModuleEnabled(const String& moduleName, bool enabled);

    // Dependencies
    bool checkDependencies(const String& moduleName) const;
    std::vector<String> getDependentModules(const String& moduleName) const;

    // Security
    bool requiresAuthentication(const String& moduleName) const;
    bool canModifyModule(const String& moduleName, const String& user) const;

    // Statistics
    size_t getTotalMemoryUsage() const;
    JsonDocument getSystemStatus() const;
};
```

## Configuration Storage

### File: /config/modules.json

```json
{
  "modules": {
    "mqtt": {
      "enabled": true,
      "auto_start": true,
      "config": {
        "server": "mqtt.example.com",
        "port": 1883
      }
    },
    "wifi_devices": {
      "enabled": true,
      "auto_start": true
    },
    "rf433": {
      "enabled": false,
      "auto_start": false,
      "reason": "Not used in current deployment"
    },
    "zigbee": {
      "enabled": false,
      "auto_start": false,
      "reason": "Security - disabled unused protocol"
    }
  },
  "security": {
    "require_auth_for_enable": true,
    "require_auth_for_disable": true,
    "allowed_users": ["admin"]
  }
}
```

## Module States & Transitions

```
    ┌──────────┐
    │ DISABLED │
    └────┬─────┘
         │ enable()
         ▼
    ┌──────────┐
    │ STARTING │◄─────┐
    └────┬─────┘      │
         │            │ restart()
         │ start OK   │
         ▼            │
    ┌──────────┐      │
    │ RUNNING  │──────┘
    └────┬─────┘
         │ disable()
         ▼
    ┌──────────┐
    │ STOPPING │
    └────┬─────┘
         │
         ▼
    ┌──────────┐
    │ DISABLED │
    └──────────┘

    (ERROR can occur from any state)
```

## Security Features

### 1. Authentication Required
- Enabling/disabling modules requires user authentication
- Configurable per module
- Role-based access control (admin only by default)

### 2. Dangerous Operations Warning
- Disabling critical modules shows warning
- Requires confirmation
- Logs all module state changes

### 3. Audit Log
```json
{
  "timestamp": "2025-01-11T16:45:00Z",
  "user": "admin",
  "action": "disable",
  "module": "zigbee",
  "reason": "Security - unused protocol",
  "success": true
}
```

## Web UI Design

### Module Management Page

```
╔═══════════════════════════════════════════════════════════╗
║                    Module Management                      ║
╠═══════════════════════════════════════════════════════════╣
║                                                           ║
║  Protocol Modules                              [Filter]  ║
║  ┌─────────────────────────────────────────────────────┐ ║
║  │ ✓ MQTT Protocol                    [■] Running      │ ║
║  │   Server: mqtt.example.com:1883                     │ ║
║  │   Memory: 12.5 KB | Status: Connected               │ ║
║  │   [Stop] [Restart] [Configure]                      │ ║
║  ├─────────────────────────────────────────────────────┤ ║
║  │ ✓ WiFi Devices                     [■] Running      │ ║
║  │   Devices: 3 connected                              │ ║
║  │   Memory: 8.2 KB | Status: OK                       │ ║
║  │   [Stop] [Restart] [Configure]                      │ ║
║  ├─────────────────────────────────────────────────────┤ ║
║  │ ✗ RF433 Protocol                   [□] Disabled     │ ║
║  │   Reason: Not used                                  │ ║
║  │   Memory: 0 KB (6.8 KB when enabled)                │ ║
║  │   [Enable] [Configure]                              │ ║
║  ├─────────────────────────────────────────────────────┤ ║
║  │ ✗ Zigbee Protocol                  [□] Disabled     │ ║
║  │   Reason: Security - disabled unused protocol       │ ║
║  │   Memory: 0 KB (15.3 KB when enabled)              │ ║
║  │   [Enable] [Configure]              🔒 Requires    │ ║
║  │                                        ESP32-C6     │ ║
║  └─────────────────────────────────────────────────────┘ ║
║                                                           ║
║  System Summary                                           ║
║  ┌─────────────────────────────────────────────────────┐ ║
║  │ Total Modules: 12                                   │ ║
║  │ Enabled: 8 | Disabled: 4                           │ ║
║  │ Memory Saved: 28.1 KB                              │ ║
║  │ Security Score: ████████░░ 8/10                    │ ║
║  └─────────────────────────────────────────────────────┘ ║
║                                                           ║
║  [Save Configuration] [Reset to Defaults]                ║
╚═══════════════════════════════════════════════════════════╝
```

## Implementation Plan

### Phase 1: Core Infrastructure
1. Create Module base class
2. Create ModuleManager class
3. Add configuration storage
4. Implement enable/disable logic

### Phase 2: Protocol Integration
1. Adapt existing protocol managers to Module interface
2. Register protocols with ModuleManager
3. Test enable/disable functionality
4. Add dependency management

### Phase 3: Web UI
1. Create module management HTML page
2. Add REST API endpoints
3. Implement WebSocket for real-time status
4. Add authentication checks

### Phase 4: Security & Polish
1. Add audit logging
2. Implement role-based access
3. Add confirmation dialogs for critical operations
4. Performance testing

## API Endpoints

### REST API

```
GET    /api/modules                    - List all modules
GET    /api/modules/:name              - Get module details
POST   /api/modules/:name/enable       - Enable module
POST   /api/modules/:name/disable      - Disable module
POST   /api/modules/:name/restart      - Restart module
GET    /api/modules/:name/config       - Get module config
POST   /api/modules/:name/config       - Update module config
GET    /api/modules/:name/stats        - Get module statistics
GET    /api/system/modules/status      - System-wide status
```

### WebSocket Events

```javascript
// Client -> Server
{
  "action": "enable_module",
  "module": "zigbee",
  "auth_token": "..."
}

// Server -> Client
{
  "event": "module_state_changed",
  "module": "zigbee",
  "state": "starting",
  "timestamp": 1705000000
}
```

## Example Usage

### 1. Disabling Unused Protocol for Security

```cpp
// In setup()
moduleManager.disableModule("zigbee");
moduleManager.disableModule("rf433");
moduleManager.saveConfiguration();
```

### 2. Runtime Enable via Web UI

```javascript
// User clicks "Enable" on Zigbee module
fetch('/api/modules/zigbee/enable', {
    method: 'POST',
    headers: {
        'Authorization': 'Bearer ' + authToken
    }
})
.then(response => {
    if (response.ok) {
        showNotification('Zigbee module enabled successfully');
    }
});
```

### 3. Checking Dependencies

```cpp
// Before disabling MQTT
if (moduleManager.getDependentModules("mqtt").size() > 0) {
    // Warning: Other modules depend on MQTT
    // Show list of dependent modules
}
```

## Benefits

### Security
- **Reduced Attack Surface** - Disable unused protocols
- **Audit Trail** - Log all module operations
- **Access Control** - Require authentication for changes

### Performance
- **Memory Savings** - Free RAM by disabling modules
- **CPU Savings** - Skip loop() calls for disabled modules
- **Faster Boot** - Don't initialize disabled modules

### Flexibility
- **Runtime Changes** - No recompilation needed
- **Easy Testing** - Enable/disable for debugging
- **Per-Deployment** - Different configs for different use cases

## ESP32-C6 Considerations

### Zigbee Support
- Zigbee module requires ESP32-C6 or ESP32-H2
- Automatically disabled on incompatible hardware
- Web UI shows hardware requirements

### Flash Size
- ESP32-C6 typically has 4MB+ flash
- Plenty of space for all modules
- OTA updates with 2MB partition scheme

### Build Configuration

```ini
[env:esp32c6]
platform = espressif32
board = esp32-c6-devkitc-1
framework = arduino
build_flags =
    -DUSE_ZIGBEE
    -DESP32_C6
board_build.partitions = default_8MB.csv
```

## Future Enhancements

1. **Plugin System** - Load modules from external files
2. **Module Marketplace** - Download new modules
3. **Auto-Discovery** - Detect and suggest needed modules
4. **Resource Monitoring** - Real-time memory/CPU usage
5. **Health Checks** - Automatic module restart on failure
6. **A/B Testing** - Test module configurations safely

---

**Document Version:** 1.0
**Date:** 2025-01-11
**Status:** Design Phase
