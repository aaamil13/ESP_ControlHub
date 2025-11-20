# Phase 1: Input Validation & Memory Monitoring

**Продължение на:** PHASE1_IMPLEMENTATION_GUIDE.md  
**Дата:** 2025-11-20

---

## Fix 4: Input Validation for PLC Configuration ✅

### Overview
Добавя валидация на JSON конфигурация преди парсиране, за да предотврати malformed data и crashes.

### New Files to Create

#### File 1: `lib/PlcEngine/Engine/PlcConfigValidator.h`

```cpp
#ifndef PLC_CONFIG_VALIDATOR_H
#define PLC_CONFIG_VALIDATOR_H

#include <Arduino.h>
#include <ArduinoJson.h>

/**
 * @brief Validator for PLC configuration JSON
 * 
 * Validates PLC configuration before loading to prevent
 * malformed data from causing crashes or undefined behavior.
 */
class PlcConfigValidator {
public:
    /**
     * @brief Validate PLC configuration JSON
     * 
     * @param jsonConfig JSON string to validate
     * @param errorMsg Output parameter for error message
     * @return true if valid, false otherwise
     */
    static bool validate(const char* jsonConfig, String& errorMsg) {
        // Parse JSON
        StaticJsonDocument<1024> doc;
        DeserializationError error = deserializeJson(doc, jsonConfig);
        
        if (error) {
            errorMsg = "JSON parse error: ";
            errorMsg += error.c_str();
            return false;
        }
        
        // Check required fields
        if (!doc.containsKey("program_name")) {
            errorMsg = "Missing required field: program_name";
            return false;
        }
        
        if (!doc.containsKey("logic")) {
            errorMsg = "Missing required field: logic";
            return false;
        }
        
        // Validate program_name
        String programName = doc["program_name"].as<String>();
        if (programName.length() == 0 || programName.length() > 32) {
            errorMsg = "Invalid program_name length (must be 1-32 chars)";
            return false;
        }
        
        // Check for invalid characters in program name
        for (size_t i = 0; i < programName.length(); i++) {
            char c = programName[i];
            if (!isalnum(c) && c != '_') {
                errorMsg = "Invalid character in program_name (only alphanumeric and underscore allowed)";
                return false;
            }
        }
        
        // Validate logic array
        if (!doc["logic"].is<JsonArray>()) {
            errorMsg = "Field 'logic' must be an array";
            return false;
        }
        
        JsonArray logic = doc["logic"].as<JsonArray>();
        if (logic.size() == 0) {
            errorMsg = "Logic array cannot be empty";
            return false;
        }
        
        if (logic.size() > 100) {
            errorMsg = "Logic array too large (max 100 blocks)";
            return false;
        }
        
        // Validate each logic block
        for (size_t i = 0; i < logic.size(); i++) {
            JsonObject block = logic[i];
            
            if (!block.containsKey("block_type")) {
                errorMsg = "Logic block ";
                errorMsg += i;
                errorMsg += " missing 'block_type'";
                return false;
            }
            
            String blockType = block["block_type"].as<String>();
            if (blockType.length() == 0) {
                errorMsg = "Empty block_type in block ";
                errorMsg += i;
                return false;
            }
            
            // Validate inputs and outputs exist
            if (!block.containsKey("inputs")) {
                errorMsg = "Logic block ";
                errorMsg += i;
                errorMsg += " missing 'inputs'";
                return false;
            }
            
            if (!block.containsKey("outputs")) {
                errorMsg = "Logic block ";
                errorMsg += i;
                errorMsg += " missing 'outputs'";
                return false;
            }
        }
        
        return true;
    }
    
    /**
     * @brief Validate endpoint string format
     * 
     * @param endpoint Endpoint string (e.g., "zone.device.variable.type")
     * @return true if valid format
     */
    static bool validateEndpoint(const String& endpoint) {
        if (endpoint.length() == 0 || endpoint.length() > 128) {
            return false;
        }
        
        // Count dots (should be exactly 3 for zone.device.variable.type)
        int dotCount = 0;
        for (size_t i = 0; i < endpoint.length(); i++) {
            if (endpoint[i] == '.') dotCount++;
        }
        
        return dotCount == 3;
    }
};

#endif // PLC_CONFIG_VALIDATOR_H
```

### Integration into PlcEngine

#### Modify: `lib/PlcEngine/Engine/PlcEngine.cpp`

**Add include at top:**
```cpp
#include "PlcConfigValidator.h"
```

**Modify `loadProgram` function (around line 34):**

Find this:
```cpp
bool PlcEngine::loadProgram(const String& programName, const char* jsonConfig) {
    // Current implementation...
```

Replace with:
```cpp
bool PlcEngine::loadProgram(const String& programName, const char* jsonConfig) {
    // ADDED: Validate configuration before parsing
    String errorMsg;
    if (!PlcConfigValidator::validate(jsonConfig, errorMsg)) {
        EspHubLog->printf("PLC config validation failed: %s\n", errorMsg.c_str());
        return false;
    }
    
    // Continue with existing implementation...
    StaticJsonDocument<2048> doc;
    DeserializationError error = deserializeJson(doc, jsonConfig);
    // ... rest unchanged
```

**Impact:** MEDIUM - Prevents crashes from malformed configuration

---

## Fix 5: Memory Monitoring System ✅

### Overview
Добавя система за мониторинг на паметта с периодично логване и предупреждения при ниска памет.

### New File to Create

#### File: `lib/Core/MemoryMonitor.h`

```cpp
#ifndef MEMORY_MONITOR_H
#define MEMORY_MONITOR_H

#include <Arduino.h>
#include "StreamLogger.h"

extern StreamLogger* EspHubLog;

/**
 * @brief Memory monitoring system
 * 
 * Monitors heap usage, detects low memory conditions,
 * and logs statistics periodically.
 */
class MemoryMonitor {
private:
    static const unsigned long CHECK_INTERVAL_MS = 60000; // 1 minute
    static const size_t LOW_MEMORY_THRESHOLD = 10000; // 10KB
    
    unsigned long lastCheckTime = 0;
    size_t minFreeHeap = 0;
    size_t maxUsedHeap = 0;
    bool lowMemoryWarningShown = false;
    
public:
    /**
     * @brief Initialize memory monitor
     */
    void begin() {
        minFreeHeap = ESP.getFreeHeap();
        maxUsedHeap = ESP.getHeapSize() - minFreeHeap;
        
        EspHubLog->println("=== Memory Monitor Initialized ===");
        logMemoryStats();
    }
    
    /**
     * @brief Check memory periodically (call from loop)
     */
    void loop() {
        unsigned long now = millis();
        if (now - lastCheckTime >= CHECK_INTERVAL_MS) {
            checkMemory();
            lastCheckTime = now;
        }
    }
    
    /**
     * @brief Force immediate memory check
     */
    void checkMemory() {
        size_t freeHeap = ESP.getFreeHeap();
        size_t usedHeap = ESP.getHeapSize() - freeHeap;
        
        // Update statistics
        if (freeHeap < minFreeHeap) {
            minFreeHeap = freeHeap;
        }
        if (usedHeap > maxUsedHeap) {
            maxUsedHeap = usedHeap;
        }
        
        // Log stats
        logMemoryStats();
        
        // Check for low memory
        if (freeHeap < LOW_MEMORY_THRESHOLD) {
            if (!lowMemoryWarningShown) {
                EspHubLog->println("⚠️  WARNING: Low memory detected!");
                EspHubLog->printf("Free heap: %d bytes (threshold: %d bytes)\n", 
                                  freeHeap, LOW_MEMORY_THRESHOLD);
                lowMemoryWarningShown = true;
            }
        } else {
            lowMemoryWarningShown = false; // Reset warning
        }
    }
    
    /**
     * @brief Log current memory statistics
     */
    void logMemoryStats() {
        EspHubLog->println("=== Memory Statistics ===");
        EspHubLog->printf("Free heap:        %6d bytes\n", ESP.getFreeHeap());
        EspHubLog->printf("Min free heap:    %6d bytes\n", ESP.getMinFreeHeap());
        EspHubLog->printf("Max alloc heap:   %6d bytes\n", ESP.getMaxAllocHeap());
        EspHubLog->printf("Heap size:        %6d bytes\n", ESP.getHeapSize());
        EspHubLog->printf("Fragmentation:    %6d%%\n", getHeapFragmentation());
        EspHubLog->printf("Largest block:    %6d bytes\n", ESP.getMaxAllocHeap());
        EspHubLog->println("========================");
    }
    
    /**
     * @brief Calculate heap fragmentation percentage
     * @return Fragmentation percentage (0-100)
     */
    uint8_t getHeapFragmentation() {
        size_t freeHeap = ESP.getFreeHeap();
        size_t maxAlloc = ESP.getMaxAllocHeap();
        
        if (freeHeap == 0) return 100;
        
        // Fragmentation = (free - largest_block) / free * 100
        return 100 - (maxAlloc * 100 / freeHeap);
    }
    
    /**
     * @brief Get minimum free heap recorded
     */
    size_t getMinFreeHeap() const { 
        return minFreeHeap; 
    }
    
    /**
     * @brief Get maximum used heap recorded
     */
    size_t getMaxUsedHeap() const { 
        return maxUsedHeap; 
    }
    
    /**
     * @brief Check if memory is low
     */
    bool isMemoryLow() const {
        return ESP.getFreeHeap() < LOW_MEMORY_THRESHOLD;
    }
    
    /**
     * @brief Get current free heap
     */
    size_t getFreeHeap() const {
        return ESP.getFreeHeap();
    }
};

#endif // MEMORY_MONITOR_H
```

### Integration into EspHub

#### Step 1: Modify `lib/Core/EspHub.h`

**Add include:**
```cpp
#include "MemoryMonitor.h"
```

**Add member variable (in private section, around line 90):**
```cpp
private:
    // ... existing members ...
    MemoryMonitor memoryMonitor; // ADD THIS LINE
```

#### Step 2: Modify `lib/Core/EspHub.cpp`

**In `begin()` function, add near the end (before the final log line):**
```cpp
void EspHub::begin() {
    // ... existing code ...
    
    // Initialize Memory Monitor
    memoryMonitor.begin();
    EspHubLog->println("Memory Monitor initialized");
    
    EspHubLog->println("EspHub Library Initialized with painlessMesh");
}
```

**In `loop()` function, add at the beginning:**
```cpp
void EspHub::loop() {
    memoryMonitor.loop(); // ADD THIS LINE
    
    mesh.update();
    appManager.updateAll();
    // ... rest unchanged
```

**Impact:** MEDIUM - Provides visibility into memory usage and early warning of leaks

---

## Complete Verification Checklist

After applying ALL Phase 1 fixes:

### 1. Code Compilation
```powershell
cd d:\Dev\ESP\EspHub
platformio run -e esp32_full
```

Expected: ✅ No errors, no warnings

### 2. Memory Usage Check
```powershell
platformio run -e esp32_full --target size
```

Expected output should show:
```
RAM:   [==        ]  ~20% (used/total)
Flash: [====      ]  ~45% (used/total)
```

### 3. Upload and Test
```powershell
platformio run -e esp32_full --target upload
platformio device monitor -b 115200
```

Expected console output:
```
Memory Monitor initialized
=== Memory Statistics ===
Free heap:        XXXXX bytes
...
Button handler initialized
EspHub Library Initialized...
```

### 4. Runtime Tests

#### Test 4.1: Memory Monitoring
- Wait 1 minute
- Should see memory statistics logged automatically
- Check for fragmentation warnings

#### Test 4.2: Input Validation
Send invalid PLC config via MQTT:
```json
{
  "invalid": "config"
}
```

Expected: Error message in console, no crash

#### Test 4.3: Button Handler
- Short press: ESP restarts
- Long press (>5s): Factory reset

#### Test 4.4: MQTT Buffer Safety
Send long MQTT message (>256 chars)
Expected: No crash, message processed correctly

---

## Git Commit Strategy

```powershell
# Fix 4: Input Validation
git add lib/PlcEngine/Engine/PlcConfigValidator.h
git add lib/PlcEngine/Engine/PlcEngine.cpp
git commit -m "feat: add PLC configuration validation

- Create PlcConfigValidator class
- Validate JSON structure and required fields
- Check program name format and length
- Validate logic blocks structure
- Prevents crashes from malformed configuration"

# Fix 5: Memory Monitoring
git add lib/Core/MemoryMonitor.h
git add lib/Core/EspHub.h
git add lib/Core/EspHub.cpp
git commit -m "feat: add memory monitoring system

- Create MemoryMonitor class
- Log memory statistics every minute
- Detect and warn on low memory conditions
- Track min/max heap usage
- Calculate heap fragmentation
- Provides early warning of memory leaks"
```

---

## Files Summary

### Modified Files
1. ✅ `src/main.cpp` - Buffer overflow + Button handler
2. ✅ `lib/Core/EspHub.h` - Destructor + MemoryMonitor member
3. ✅ `lib/Core/EspHub.cpp` - Destructor impl + MemoryMonitor integration
4. ✅ `lib/PlcEngine/Engine/PlcEngine.cpp` - Validation integration

### New Files Created
1. ✅ `lib/PlcEngine/Engine/PlcConfigValidator.h`
2. ✅ `lib/Core/MemoryMonitor.h`

---

## Phase 1 Complete! 🎉

**All 5 critical fixes documented:**
- ✅ Buffer overflow fix
- ✅ EspHub destructor
- ✅ Button handler
- ✅ Input validation
- ✅ Memory monitoring

**Estimated implementation time:** 30-40 minutes  
**Risk level:** Low  
**Testing required:** Medium

**Next:** Phase 2 - Error Handling & Testing

---

**Created by:** AI Code Reviewer  
**Date:** 2025-11-20  
**Status:** Complete and Ready for Implementation
