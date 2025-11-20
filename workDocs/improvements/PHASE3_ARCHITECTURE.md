# Phase 3: Architecture Improvements

**Дата:** 2025-11-20  
**Статус:** Implementation Guide  
**Локация:** `workDocs/improvements/`

---

## Overview

Фаза 3 се фокусира върху архитектурни подобрения за намаляване на свързаността (coupling), стандартизиране на протоколите и оптимизация на паметта.

---

## Part 1: Protocol Interface Standardization

### File 1: ProtocolManagerInterface.h

**Локация:** `lib/Protocols/ProtocolManagerInterface.h` (NEW)

```cpp
#ifndef PROTOCOL_MANAGER_INTERFACE_H
#define PROTOCOL_MANAGER_INTERFACE_H

#include <Arduino.h>

/**
 * @brief Interface for all protocol managers
 * 
 * Standardizes how different protocols (MQTT, Mesh, Zigbee, RF433)
 * interact with the main system.
 */
class IProtocolManager {
public:
    virtual ~IProtocolManager() {}

    /**
     * @brief Initialize the protocol manager
     * @return true if successful
     */
    virtual bool begin() = 0;

    /**
     * @brief Main loop processing
     * Should be called frequently
     */
    virtual void loop() = 0;

    /**
     * @brief Send a message via this protocol
     * @param target Target device or topic
     * @param message Message content
     * @return true if sent successfully
     */
    virtual bool send(const String& target, const String& message) = 0;

    /**
     * @brief Check if protocol is connected/active
     */
    virtual bool isConnected() = 0;

    /**
     * @brief Get protocol name
     */
    virtual const char* getName() const = 0;
};

#endif // PROTOCOL_MANAGER_INTERFACE_H
```

### Refactoring MqttManager

**Локация:** `lib/Protocols/Mqtt/MqttManager.h`

```cpp
#include "../../Protocols/ProtocolManagerInterface.h"

// Inherit from interface
class MqttManager : public IProtocolManager {
public:
    // ... existing methods ...

    // Implement interface methods
    bool begin() override {
        // Call existing begin logic or wrap it
        // Note: Existing begin() might have different signature, 
        // so we might need an adapter or update the signature
        return true; 
    }

    void loop() override {
        this->loop(); // Call existing loop
    }

    bool send(const String& target, const String& message) override {
        return this->publish(target.c_str(), message.c_str());
    }

    bool isConnected() override {
        return this->connected();
    }

    const char* getName() const override {
        return "MQTT";
    }
};
```

---

## Part 2: Dependency Injection (DI)

### Overview

Преминаване от директно създаване на обекти в `EspHub` към подаване на зависимостите през конструктора. Това улеснява тестването и гъвкавостта.

### Refactoring EspHub

**Стъпка 1: Промяна на конструктора в `lib/Core/EspHub.h`**

```cpp
class EspHub {
public:
    // Old Constructor
    // EspHub();

    // New Constructor with Dependency Injection
    EspHub(
        MqttManager* mqtt, 
        PlcEngine* plc, 
        WebManager* web,
        TimeManager* time
    );
    
    // ...
};
```

**Стъпка 2: Промяна на имплементацията в `lib/Core/EspHub.cpp`**

```cpp
EspHub::EspHub(MqttManager* mqtt, PlcEngine* plc, WebManager* web, TimeManager* time) 
    : mqttManager(mqtt), plcEngine(plc), webManager(web), timeManager(time) 
{
    instance = this;
    EspHubLog = &logger;
    // ...
}
```

**Стъпка 3: Промяна на `src/main.cpp` (Composition Root)**

```cpp
// Create instances globally or in setup
MqttManager mqttManager;
PlcEngine plcEngine;
WebManager webManager;
TimeManager timeManager;

// Inject dependencies
EspHub hub(&mqttManager, &plcEngine, &webManager, &timeManager);

void setup() {
    // ...
    hub.begin();
}
```

---

## Part 3: Memory Optimization (StringPool)

### File: StringPool.h

**Локация:** `lib/Core/StringPool.h` (NEW)

```cpp
#ifndef STRING_POOL_H
#define STRING_POOL_H

#include <Arduino.h>
#include <vector>
#include <map>

/**
 * @brief String Deduplication Pool
 * 
 * Reduces memory usage by storing unique copies of repeated strings
 * (like JSON keys, configuration parameters, etc.)
 */
class StringPool {
private:
    std::vector<String> pool;
    std::map<String, uint16_t> indexMap;

public:
    /**
     * @brief Get a shared instance of a string
     */
    const String& get(const String& str) {
        // If string exists, return reference to stored copy
        auto it = indexMap.find(str);
        if (it != indexMap.end()) {
            return pool[it->second];
        }

        // Add new string
        uint16_t index = pool.size();
        pool.push_back(str);
        indexMap[str] = index;
        
        return pool[index];
    }

    /**
     * @brief Clear the pool to free memory
     */
    void clear() {
        pool.clear();
        indexMap.clear();
    }

    size_t size() const {
        return pool.size();
    }
};

#endif // STRING_POOL_H
```

### Usage Example

```cpp
// In PlcEngine or ConfigLoader
StringPool stringPool;

void loadConfig(JsonObject doc) {
    // Instead of storing many copies of "type", "value", etc.
    String typeKey = stringPool.get("type");
    String valueKey = stringPool.get("value");
    
    // Use shared strings...
}
```

---

## Part 4: InputValidator Class

### File: InputValidator.h

**Локация:** `lib/Core/InputValidator.h` (NEW)

```cpp
#ifndef INPUT_VALIDATOR_H
#define INPUT_VALIDATOR_H

#include <Arduino.h>

class InputValidator {
public:
    static bool isValidTopic(const String& topic) {
        if (topic.length() == 0 || topic.length() > 64) return false;
        // Check for illegal MQTT characters
        if (topic.indexOf('#') >= 0 || topic.indexOf('+') >= 0) return false;
        return true;
    }

    static bool isValidHostname(const String& hostname) {
        if (hostname.length() == 0 || hostname.length() > 255) return false;
        // Basic regex-like check could go here
        return true;
    }

    static String sanitizeString(String input) {
        input.trim();
        // Remove potentially dangerous characters if needed
        // input.replace("'", "");
        // input.replace(";", "");
        return input;
    }
};

#endif // INPUT_VALIDATOR_H
```

---

## Git Commit Strategy

```powershell
# Step 1: Protocol Interface
git add lib/Protocols/ProtocolManagerInterface.h
git commit -m "feat: add ProtocolManagerInterface for standardization"

# Step 2: String Pool
git add lib/Core/StringPool.h
git commit -m "feat: add StringPool for memory optimization"

# Step 3: Input Validator
git add lib/Core/InputValidator.h
git commit -m "feat: add InputValidator class"
```

---

**Status:** Ready for Implementation  
**Estimated Time:** 1-2 hours (High Complexity for DI refactoring)  
**Risk Level:** High (DI changes affect main initialization)

**Created by:** AI Code Reviewer  
**Date:** 2025-11-20
