#include "ModuleManager.h"
#include <LittleFS.h>

ModuleManager::ModuleManager()
    : requireAuth(false) {
}

ModuleManager::~ModuleManager() {
    stopAll();
}

// ============================================================================
// Lifecycle
// ============================================================================

bool ModuleManager::initialize() {
    // Load configuration from file
    // loadConfiguration();

    return true;
}

bool ModuleManager::startAll() {
    // logger->println("Starting all enabled modules");

    int started = 0;
    int failed = 0;

    for (auto& pair : modules) {
        Module* module = pair.second;
        String moduleName = module->getName();

        // Check if module should be started
        if (!isModuleEnabled(moduleName)) {
            // logger->log(LogLevel::DEBUG, "ModuleManager",
                       // String("Skipping disabled module: ") + moduleName);
            continue;
        }

        if (!isModuleAutoStart(moduleName)) {
            // logger->log(LogLevel::DEBUG, "ModuleManager",
                       // String("Skipping non-autostart module: ") + moduleName);
            continue;
        }

        // Check hardware compatibility
        if (!module->isHardwareCompatible()) {
            // logger->log(LogLevel::WARNING, "ModuleManager",
                       // String("Module ") + moduleName + " not compatible with hardware");
            continue;
        }

        // Check dependencies
        if (!checkDependencies(moduleName)) {
            std::vector<String> missing = getMissingDependencies(moduleName);
            String missingStr = "";
            for (size_t i = 0; i < missing.size(); i++) {
                if (i > 0) missingStr += ", ";
                missingStr += missing[i];
            }
            // logger->log(LogLevel::ERROR, "ModuleManager",
                       // String("Module ") + moduleName + " missing dependencies: " + missingStr);
            failed++;
            continue;
        }

        // Initialize and start module
        if (initializeModule(module) && startModule(module)) {
            started++;
            logModuleEvent(moduleName, "started", "Auto-start on boot");
        } else {
            failed++;
            // logger->log(LogLevel::ERROR, "ModuleManager",
                       // String("Failed to start module: ") + moduleName);
        }
    }

    // logger->log(LogLevel::INFO, "ModuleManager",
               // String("Started ") + String(started) + " modules, " +
               // String(failed) + " failed");

    return failed == 0;
}

void ModuleManager::stopAll() {
    // logger->println("Stopping all modules");

    for (auto& pair : modules) {
        Module* module = pair.second;
        if (module->isRunning()) {
            stopModule(module);
            logModuleEvent(module->getName(), "stopped", "System shutdown");
        }
    }
}

void ModuleManager::loop() {
    // Call loop() on all running modules
    for (auto& pair : modules) {
        Module* module = pair.second;
        if (module->isRunning()) {
            module->loop();
        }
    }
}

// ============================================================================
// Module Registration
// ============================================================================

bool ModuleManager::registerModule(Module* module) {
    if (!module) {
        // logger->println("Cannot register null module");
        return false;
    }

    String moduleName = module->getName();

    if (modules.find(moduleName) != modules.end()) {
        // logger->log(LogLevel::WARNING, "ModuleManager",
                   // String("Module already registered: ") + moduleName);
        return false;
    }

    modules[moduleName] = module;

    // Initialize config if not exists
    if (moduleConfigs.find(moduleName) == moduleConfigs.end()) {
        ModuleCapabilities caps = module->getCapabilities();
        moduleConfigs[moduleName] = {
            .enabled = true,  // Default enabled
            .autoStart = true,  // Default auto-start
            .reason = ""
        };
    }

    // logger->log(LogLevel::INFO, "ModuleManager",
               // String("Registered module: ") + moduleName +
               // " (" + moduleTypeToString(module->getType()) + ")");

    return true;
}

bool ModuleManager::unregisterModule(const String& moduleName) {
    auto it = modules.find(moduleName);
    if (it == modules.end()) {
        return false;
    }

    // Stop module if running
    Module* module = it->second;
    if (module->isRunning()) {
        stopModule(module);
    }

    modules.erase(it);
    // logger->log(LogLevel::INFO, "ModuleManager",
               // String("Unregistered module: ") + moduleName);

    return true;
}

Module* ModuleManager::getModule(const String& moduleName) {
    auto it = modules.find(moduleName);
    if (it != modules.end()) {
        return it->second;
    }
    return nullptr;
}

const Module* ModuleManager::getModule(const String& moduleName) const {
    auto it = modules.find(moduleName);
    if (it != modules.end()) {
        return it->second;
    }
    return nullptr;
}

std::vector<String> ModuleManager::getModuleList() const {
    std::vector<String> list;
    for (auto& pair : modules) {
        list.push_back(pair.first);
    }
    return list;
}

std::vector<String> ModuleManager::getEnabledModules() const {
    std::vector<String> list;
    for (auto& pair : modules) {
        if (pair.second->isEnabled()) {
            list.push_back(pair.first);
        }
    }
    return list;
}

std::vector<String> ModuleManager::getRunningModules() const {
    std::vector<String> list;
    for (auto& pair : modules) {
        if (pair.second->isRunning()) {
            list.push_back(pair.first);
        }
    }
    return list;
}

// ============================================================================
// Module Control
// ============================================================================

bool ModuleManager::enableModule(const String& moduleName, bool saveConfig) {
    Module* module = getModule(moduleName);
    if (!module) {
        // logger->log(LogLevel::ERROR, "ModuleManager",
                   // String("Module not found: ") + moduleName);
        return false;
    }

    // Check hardware compatibility
    if (!module->isHardwareCompatible()) {
        ModuleCapabilities caps = module->getCapabilities();
        // logger->log(LogLevel::ERROR, "ModuleManager",
                   // String("Module ") + moduleName +
                   // " requires hardware: " + caps.hardwareRequirement);
        return false;
    }

    // Check dependencies
    if (!checkDependencies(moduleName)) {
        std::vector<String> missing = getMissingDependencies(moduleName);
        String missingStr = "";
        for (size_t i = 0; i < missing.size(); i++) {
            if (i > 0) missingStr += ", ";
            missingStr += missing[i];
        }
        // logger->log(LogLevel::ERROR, "ModuleManager",
                   // String("Cannot enable ") + moduleName +
                   // ", missing dependencies: " + missingStr);
        return false;
    }

    // Initialize and start module
    bool success = initializeModule(module) && startModule(module);

    if (success) {
        setModuleEnabled(moduleName, true);
        logModuleEvent(moduleName, "enabled", "Manual enable");

        if (saveConfig) {
            saveConfiguration();
        }
    }

    return success;
}

bool ModuleManager::disableModule(const String& moduleName, bool saveConfig) {
    Module* module = getModule(moduleName);
    if (!module) {
        // logger->log(LogLevel::ERROR, "ModuleManager",
                   // String("Module not found: ") + moduleName);
        return false;
    }

    // Check if module can be disabled
    ModuleCapabilities caps = module->getCapabilities();
    if (!caps.canDisable) {
        // logger->log(LogLevel::ERROR, "ModuleManager",
                   // String("Module ") + moduleName + " cannot be disabled");
        return false;
    }

    // Check for dependent modules
    std::vector<String> dependents = getDependentModules(moduleName);
    if (!dependents.empty()) {
        String dependentStr = "";
        for (size_t i = 0; i < dependents.size(); i++) {
            if (i > 0) dependentStr += ", ";
            dependentStr += dependents[i];
        }
        // logger->log(LogLevel::ERROR, "ModuleManager",
                   // String("Cannot disable ") + moduleName +
                   // ", required by: " + dependentStr);
        return false;
    }

    // Stop module
    bool success = stopModule(module);

    if (success) {
        setModuleEnabled(moduleName, false);
        logModuleEvent(moduleName, "disabled", "Manual disable");

        if (saveConfig) {
            saveConfiguration();
        }
    }

    return success;
}

bool ModuleManager::restartModule(const String& moduleName) {
    Module* module = getModule(moduleName);
    if (!module) {
        return false;
    }

    // logger->log(LogLevel::INFO, "ModuleManager",
               // String("Restarting module: ") + moduleName);

    bool wasRunning = module->isRunning();

    if (wasRunning) {
        if (!stopModule(module)) {
            return false;
        }
    }

    delay(100);  // Brief pause between stop and start

    if (wasRunning || isModuleEnabled(moduleName)) {
        if (!startModule(module)) {
            return false;
        }
    }

    logModuleEvent(moduleName, "restarted", "Manual restart");
    return true;
}

ModuleState ModuleManager::getModuleState(const String& moduleName) const {
    auto it = modules.find(moduleName);
    if (it != modules.end()) {
        return it->second->getState();
    }
    return ModuleState::MODULE_ERROR;
}

// ============================================================================
// Configuration
// ============================================================================

bool ModuleManager::loadConfiguration(const String& configPath) {
    if (!LittleFS.exists(configPath)) {
        // logger->log(LogLevel::WARNING, "ModuleManager",
                   // String("Config file not found: ") + configPath);
        return false;
    }

    File file = LittleFS.open(configPath, "r");
    if (!file) {
        // logger->log(LogLevel::ERROR, "ModuleManager",
                   // String("Failed to open config: ") + configPath);
        return false;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) {
        // logger->log(LogLevel::ERROR, "ModuleManager",
                   // String("Failed to parse config: ") + error.c_str());
        return false;
    }

    // Load module configurations
    JsonObject modulesObj = doc["modules"].as<JsonObject>();
    for (JsonPair kv : modulesObj) {
        String moduleName = kv.key().c_str();
        JsonObject moduleConfig = kv.value().as<JsonObject>();

        ModuleConfig config;
        config.enabled = moduleConfig["enabled"] | true;
        config.autoStart = moduleConfig["auto_start"] | true;
        config.reason = moduleConfig["reason"] | "";

        moduleConfigs[moduleName] = config;

        // Apply module-specific configuration if module is registered
        Module* module = getModule(moduleName);
        if (module && moduleConfig.containsKey("config")) {
            JsonObject modConfig = moduleConfig["config"].as<JsonObject>();
            module->configure(modConfig);
        }
    }

    // Load security settings
    if (doc.containsKey("security")) {
        JsonObject security = doc["security"].as<JsonObject>();
        requireAuth = security["require_auth_for_enable"] | false;

        if (security.containsKey("allowed_users")) {
            allowedUsers.clear();
            JsonArray users = security["allowed_users"].as<JsonArray>();
            for (JsonVariant user : users) {
                allowedUsers.push_back(user.as<String>());
            }
        }
    }

    // logger->println("Configuration loaded successfully");
    return true;
}

bool ModuleManager::saveConfiguration(const String& configPath) {
    JsonDocument doc;

    // Save module configurations
    JsonObject modulesObj = doc["modules"].to<JsonObject>();
    for (auto& pair : moduleConfigs) {
        JsonObject moduleConfig = modulesObj[pair.first].to<JsonObject>();
        moduleConfig["enabled"] = pair.second.enabled;
        moduleConfig["auto_start"] = pair.second.autoStart;
        if (!pair.second.reason.isEmpty()) {
            moduleConfig["reason"] = pair.second.reason;
        }

        // Save module-specific config if module exists
        Module* module = getModule(pair.first);
        if (module) {
            JsonDocument modConfigDoc = module->getConfig();
            if (!modConfigDoc.isNull()) {
                moduleConfig["config"] = modConfigDoc.as<JsonObject>();
            }
        }
    }

    // Save security settings
    JsonObject security = doc["security"].to<JsonObject>();
    security["require_auth_for_enable"] = requireAuth;
    security["require_auth_for_disable"] = requireAuth;

    if (!allowedUsers.empty()) {
        JsonArray users = security["allowed_users"].to<JsonArray>();
        for (String& user : allowedUsers) {
            users.add(user);
        }
    }

    // Write to file
    File file = LittleFS.open(configPath, "w");
    if (!file) {
        // logger->log(LogLevel::ERROR, "ModuleManager",
                   // String("Failed to create config file: ") + configPath);
        return false;
    }

    serializeJson(doc, file);
    file.close();

    // logger->println("Configuration saved successfully");
    return true;
}

void ModuleManager::setModuleEnabled(const String& moduleName, bool enabled) {
    moduleConfigs[moduleName].enabled = enabled;
}

bool ModuleManager::isModuleEnabled(const String& moduleName) const {
    auto it = moduleConfigs.find(moduleName);
    if (it != moduleConfigs.end()) {
        return it->second.enabled;
    }
    return true;  // Default enabled if not configured
}

void ModuleManager::setModuleAutoStart(const String& moduleName, bool autoStart) {
    moduleConfigs[moduleName].autoStart = autoStart;
}

bool ModuleManager::isModuleAutoStart(const String& moduleName) const {
    auto it = moduleConfigs.find(moduleName);
    if (it != moduleConfigs.end()) {
        return it->second.autoStart;
    }
    return true;  // Default auto-start if not configured
}

// ============================================================================
// Dependencies
// ============================================================================

bool ModuleManager::checkDependencies(const String& moduleName) const {
    const Module* module = getModule(moduleName);
    if (!module) {
        return false;
    }

    ModuleCapabilities caps = module->getCapabilities();
    for (String& dependency : caps.dependencies) {
        // Check if dependency is registered
        const Module* depModule = getModule(dependency);
        if (!depModule) {
            return false;  // Dependency not registered
        }

        // Check if dependency is enabled
        if (!isModuleEnabled(dependency)) {
            return false;  // Dependency not enabled
        }
    }

    return true;
}

std::vector<String> ModuleManager::getDependentModules(const String& moduleName) const {
    std::vector<String> dependents;

    // Find all modules that depend on this module
    for (auto& pair : modules) {
        const Module* module = pair.second;
        ModuleCapabilities caps = module->getCapabilities();

        for (String& dependency : caps.dependencies) {
            if (dependency == moduleName) {
                // Check if this dependent module is enabled
                if (isModuleEnabled(pair.first)) {
                    dependents.push_back(pair.first);
                }
                break;
            }
        }
    }

    return dependents;
}

std::vector<String> ModuleManager::getMissingDependencies(const String& moduleName) const {
    std::vector<String> missing;

    const Module* module = getModule(moduleName);
    if (!module) {
        return missing;
    }

    ModuleCapabilities caps = module->getCapabilities();
    for (String& dependency : caps.dependencies) {
        const Module* depModule = getModule(dependency);
        if (!depModule || !isModuleEnabled(dependency)) {
            missing.push_back(dependency);
        }
    }

    return missing;
}

// ============================================================================
// Security
// ============================================================================

bool ModuleManager::requiresAuthentication() const {
    return requireAuth;
}

void ModuleManager::setRequireAuthentication(bool require) {
    requireAuth = require;
}

bool ModuleManager::canModifyModule(const String& moduleName, const String& user) const {
    if (!requireAuth) {
        return true;  // No authentication required
    }

    if (user.isEmpty()) {
        return false;  // Authentication required but no user provided
    }

    // Check if user is in allowed list
    for (const String& allowedUser : allowedUsers) {
        if (allowedUser == user) {
            return true;
        }
    }

    return false;
}

void ModuleManager::logOperation(const String& moduleName, const String& operation,
                                 const String& user, bool success) {
    // TODO: Implement audit logging to file
    // For now, just log to console
    String logMsg = "Module: " + moduleName +
                   ", Operation: " + operation +
                   ", User: " + (user.isEmpty() ? "system" : user) +
                   ", Success: " + (success ? "true" : "false");

    // logger->println(logMsg);
}

// ============================================================================
// Statistics & Monitoring
// ============================================================================

size_t ModuleManager::getTotalMemoryUsage() const {
    size_t total = 0;
    for (auto& pair : modules) {
        if (pair.second->isRunning()) {
            total += pair.second->getMemoryUsage();
        }
    }
    return total;
}

size_t ModuleManager::getMemorySavings() const {
    size_t savings = 0;
    for (auto& pair : modules) {
        if (!pair.second->isEnabled()) {
            savings += pair.second->getMemoryUsage();
        }
    }
    return savings;
}

JsonDocument ModuleManager::getSystemStatus() const {
    JsonDocument doc;

    doc["total_modules"] = modules.size();
    doc["enabled_modules"] = getEnabledModules().size();
    doc["running_modules"] = getRunningModules().size();
    doc["disabled_modules"] = modules.size() - getEnabledModules().size();

    doc["memory_used"] = getTotalMemoryUsage();
    doc["memory_saved"] = getMemorySavings();

    // Count by type
    JsonObject byType = doc["by_type"].to<JsonObject>();
    byType["protocol"] = countModulesByType(ModuleType::PROTOCOL);
    byType["storage"] = countModulesByType(ModuleType::STORAGE);
    byType["export"] = countModulesByType(ModuleType::EXPORT);
    byType["ui"] = countModulesByType(ModuleType::UI);
    byType["app"] = countModulesByType(ModuleType::APP);
    byType["core"] = countModulesByType(ModuleType::CORE);

    // Count by state
    JsonObject byState = doc["by_state"].to<JsonObject>();
    byState["disabled"] = countModulesByState(ModuleState::MODULE_DISABLED);
    byState["enabled"] = countModulesByState(ModuleState::MODULE_ENABLED);
    byState["starting"] = countModulesByState(ModuleState::MODULE_STARTING);
    byState["running"] = countModulesByState(ModuleState::MODULE_RUNNING);
    byState["stopping"] = countModulesByState(ModuleState::MODULE_STOPPING);
    byState["error"] = countModulesByState(ModuleState::MODULE_ERROR);

    return doc;
}

JsonDocument ModuleManager::getModuleInfo(const String& moduleName) const {
    JsonDocument doc;

    const Module* module = getModule(moduleName);
    if (!module) {
        doc["error"] = "Module not found";
        return doc;
    }

    doc["name"] = module->getName();
    doc["display_name"] = module->getDisplayName();
    doc["version"] = module->getVersion();
    doc["type"] = moduleTypeToString(module->getType());
    doc["state"] = moduleStateToString(module->getState());
    doc["status"] = module->getStatusMessage();
    doc["description"] = module->getDescription();

    ModuleCapabilities caps = module->getCapabilities();
    JsonObject capsObj = doc["capabilities"].to<JsonObject>();
    capsObj["can_disable"] = caps.canDisable;
    capsObj["requires_reboot"] = caps.requiresReboot;
    capsObj["has_web_ui"] = caps.hasWebUI;
    capsObj["has_security"] = caps.hasSecurity;
    capsObj["memory_usage"] = caps.estimatedMemory;

    if (!caps.hardwareRequirement.isEmpty()) {
        capsObj["hardware_requirement"] = caps.hardwareRequirement;
    }

    if (!caps.dependencies.empty()) {
        JsonArray deps = capsObj["dependencies"].to<JsonArray>();
        for (const String& dep : caps.dependencies) {
            deps.add(dep);
        }
    }

    doc["enabled"] = isModuleEnabled(moduleName);
    doc["auto_start"] = isModuleAutoStart(moduleName);
    doc["hardware_compatible"] = module->isHardwareCompatible();
    doc["dependencies_met"] = checkDependencies(moduleName);

    auto it = moduleConfigs.find(moduleName);
    if (it != moduleConfigs.end() && !it->second.reason.isEmpty()) {
        doc["reason"] = it->second.reason;
    }

    return doc;
}

JsonDocument ModuleManager::getModuleStatistics(const String& moduleName) const {
    const Module* module = getModule(moduleName);
    if (!module) {
        JsonDocument doc;
        doc["error"] = "Module not found";
        return doc;
    }

    return module->getStatistics();
}

int ModuleManager::countModulesByType(ModuleType type) const {
    int count = 0;
    for (auto& pair : modules) {
        if (pair.second->getType() == type) {
            count++;
        }
    }
    return count;
}

int ModuleManager::countModulesByState(ModuleState state) const {
    int count = 0;
    for (auto& pair : modules) {
        if (pair.second->getState() == state) {
            count++;
        }
    }
    return count;
}

// ============================================================================
// Health Checks
// ============================================================================

int ModuleManager::healthCheckAll() {
    int unhealthy = 0;

    for (auto& pair : modules) {
        Module* module = pair.second;
        if (module->isEnabled() && !module->healthCheck()) {
            unhealthy++;
            // logger->log(LogLevel::WARNING, "ModuleManager",
                       // String("Module unhealthy: ") + module->getName() +
                       // " - " + module->getLastError());
        }
    }

    return unhealthy;
}

bool ModuleManager::isModuleHealthy(const String& moduleName) const {
    const Module* module = getModule(moduleName);
    if (!module) {
        return false;
    }
    return module->healthCheck();
}

std::vector<String> ModuleManager::getUnhealthyModules() const {
    std::vector<String> unhealthy;

    for (auto& pair : modules) {
        Module* module = pair.second;
        if (module->isEnabled() && !module->healthCheck()) {
            unhealthy.push_back(module->getName());
        }
    }

    return unhealthy;
}

// ============================================================================
// Internal Helper Methods
// ============================================================================

bool ModuleManager::initializeModule(Module* module) {
    if (!module) {
        return false;
    }

    String moduleName = module->getName();

    // Skip if already initialized
    ModuleState state = module->getState();
    if (state != ModuleState::MODULE_DISABLED) {
        return true;
    }

    // logger->log(LogLevel::INFO, "ModuleManager",
               // String("Initializing module: ") + moduleName);

    if (!module->initialize()) {
        // logger->log(LogLevel::ERROR, "ModuleManager",
                   // String("Failed to initialize module: ") + moduleName +
                   // " - " + module->getLastError());
        return false;
    }

    return true;
}

bool ModuleManager::startModule(Module* module) {
    if (!module) {
        return false;
    }

    String moduleName = module->getName();

    // Skip if already running
    if (module->isRunning()) {
        return true;
    }

    // logger->log(LogLevel::INFO, "ModuleManager",
               // String("Starting module: ") + moduleName);

    if (!module->start()) {
        // logger->log(LogLevel::ERROR, "ModuleManager",
                   // String("Failed to start module: ") + moduleName +
                   // " - " + module->getLastError());
        return false;
    }

    // logger->log(LogLevel::INFO, "ModuleManager",
               // String("Module started: ") + moduleName);

    return true;
}

bool ModuleManager::stopModule(Module* module) {
    if (!module) {
        return false;
    }

    String moduleName = module->getName();

    // Skip if not running
    if (!module->isRunning()) {
        return true;
    }

    // logger->log(LogLevel::INFO, "ModuleManager",
               // String("Stopping module: ") + moduleName);

    if (!module->stop()) {
        // logger->log(LogLevel::ERROR, "ModuleManager",
                   // String("Failed to stop module: ") + moduleName +
                   // " - " + module->getLastError());
        return false;
    }

    // logger->log(LogLevel::INFO, "ModuleManager",
               // String("Module stopped: ") + moduleName);

    return true;
}

void ModuleManager::logModuleEvent(const String& moduleName, const String& event,
                                   const String& details) {
    String logMsg = "Module: " + moduleName + ", Event: " + event;
    if (!details.isEmpty()) {
        logMsg += ", Details: " + details;
    }

    // logger->println(logMsg);
    logOperation(moduleName, event, "", true);
}
