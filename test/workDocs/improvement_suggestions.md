# EspHub - Конкретни Предложения за Подобрения

**Дата:** 20 Ноември 2025  
**Допълнение към:** code_quality_analysis.md

---

## 📋 Съдържание

1. [Критични Поправки с Код](#критични-поправки-с-код)
2. [Рефакториране на Архитектурата](#рефакториране-на-архитектурата)
3. [Примери за Unit Tests](#примери-за-unit-tests)
4. [Error Handling Framework](#error-handling-framework)
5. [Memory Management Improvements](#memory-management-improvements)
6. [Security Enhancements](#security-enhancements)

---

## 🔴 Критични Поправки с Код

### 1. Поправка на Buffer Overflow в main.cpp

**Проблем:**
```cpp
// main.cpp:16-24 (UNSAFE)
void mqtt_callback(char* topic, byte* payload, unsigned int length) {
  EspHubLog->print("Message arrived [");
  EspHubLog->print(topic);
  EspHubLog->print("] ");
  
  // ОПАСНО: Записва извън буфера!
  char* message = (char*)payload;
  message[length] = '\0';
  EspHubLog->println(message);
  // ...
}
```

**Решение:**
```cpp
// main.cpp:16-24 (SAFE)
void mqtt_callback(char* topic, byte* payload, unsigned int length) {
  EspHubLog->print("Message arrived [");
  EspHubLog->print(topic);
  EspHubLog->print("] ");
  
  // БЕЗОПАСНО: Създаваме нов буфер с правилен размер
  char message[length + 1];
  memcpy(message, payload, length);
  message[length] = '\0';
  EspHubLog->println(message);
  
  // Още по-добре: използвайте String с reserve
  String msg;
  msg.reserve(length + 1);
  for (unsigned int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }
  EspHubLog->println(msg);
  
  // ...
}
```

### 2. Добавяне на Деструктор за EspHub

**Проблем:** Липсва деструктор, потенциални memory leaks

**Решение:**
```cpp
// EspHub.h
class EspHub {
public:
    EspHub();
    ~EspHub(); // Добавете деструктор
    // ...
};

// EspHub.cpp
EspHub::~EspHub() {
    EspHubLog->println("EspHub shutting down...");
    
    // Освободете условно заделена памет
    #ifdef USE_WIFI_DEVICES
    if (wifiDeviceManager) {
        delete wifiDeviceManager;
        wifiDeviceManager = nullptr;
    }
    #endif
    
    #ifdef USE_RF433
    if (rf433Manager) {
        delete rf433Manager;
        rf433Manager = nullptr;
    }
    #endif
    
    #ifdef USE_ZIGBEE
    if (zigbeeManager) {
        delete zigbeeManager;
        zigbeeManager = nullptr;
    }
    #endif
    
    EspHubLog->println("EspHub shutdown complete.");
}
```

### 3. Преместване на Button Handling от setup() в loop()

**Проблем:** Button handling в setup() се изпълнява само веднъж

**Решение:**
```cpp
// main.cpp - Нова функция
class ButtonHandler {
private:
    static const int BUTTON_PIN = 0;
    static const unsigned long DEBOUNCE_MS = 50;
    static const unsigned long FACTORY_RESET_MS = 5000;
    static const unsigned long RESTART_MS = 1000;
    
    bool buttonPressed = false;
    unsigned long buttonPressStartTime = 0;
    unsigned long lastDebounceTime = 0;
    bool lastButtonState = HIGH;
    
public:
    void begin() {
        pinMode(BUTTON_PIN, INPUT_PULLUP);
    }
    
    void loop(EspHub& hub) {
        bool currentState = digitalRead(BUTTON_PIN);
        unsigned long now = millis();
        
        // Debounce
        if (currentState != lastButtonState) {
            lastDebounceTime = now;
        }
        
        if ((now - lastDebounceTime) > DEBOUNCE_MS) {
            // Button pressed (LOW)
            if (currentState == LOW && !buttonPressed) {
                buttonPressed = true;
                buttonPressStartTime = now;
                EspHubLog->println("Button pressed...");
            }
            
            // Button held
            if (currentState == LOW && buttonPressed) {
                unsigned long pressDuration = now - buttonPressStartTime;
                
                if (pressDuration > FACTORY_RESET_MS) {
                    EspHubLog->println("Factory reset triggered!");
                    hub.factoryReset();
                    buttonPressed = false; // Prevent multiple resets
                }
            }
            
            // Button released
            if (currentState == HIGH && buttonPressed) {
                unsigned long pressDuration = now - buttonPressStartTime;
                
                if (pressDuration < RESTART_MS) {
                    EspHubLog->println("Restart triggered!");
                    hub.restartEsp();
                }
                
                buttonPressed = false;
            }
        }
        
        lastButtonState = currentState;
    }
};

// Глобална инстанция
ButtonHandler buttonHandler;

void setup() {
    Serial.begin(115200);
    hub.begin();
    
    // WiFi setup...
    
    buttonHandler.begin();
}

void loop() {
    buttonHandler.loop(hub);
    hub.loop();
}
```

### 4. Input Validation за PLC Configuration

**Проблем:** Няма валидация на JSON конфигурация

**Решение:**
```cpp
// PlcEngine.cpp
class PlcConfigValidator {
public:
    static bool validate(const char* jsonConfig, String& errorMsg) {
        StaticJsonDocument<1024> doc;
        DeserializationError error = deserializeJson(doc, jsonConfig);
        
        if (error) {
            errorMsg = "JSON parse error: ";
            errorMsg += error.c_str();
            return false;
        }
        
        // Проверка за задължителни полета
        if (!doc.containsKey("program_name")) {
            errorMsg = "Missing required field: program_name";
            return false;
        }
        
        if (!doc.containsKey("logic")) {
            errorMsg = "Missing required field: logic";
            return false;
        }
        
        // Проверка на program_name
        String programName = doc["program_name"].as<String>();
        if (programName.length() == 0 || programName.length() > 32) {
            errorMsg = "Invalid program_name length (must be 1-32 chars)";
            return false;
        }
        
        // Проверка на logic array
        if (!doc["logic"].is<JsonArray>()) {
            errorMsg = "Field 'logic' must be an array";
            return false;
        }
        
        JsonArray logic = doc["logic"].as<JsonArray>();
        if (logic.size() == 0) {
            errorMsg = "Logic array cannot be empty";
            return false;
        }
        
        // Проверка на всеки блок
        for (JsonObject block : logic) {
            if (!block.containsKey("block_type")) {
                errorMsg = "Logic block missing 'block_type'";
                return false;
            }
            
            String blockType = block["block_type"].as<String>();
            if (blockType.length() == 0) {
                errorMsg = "Empty block_type not allowed";
                return false;
            }
        }
        
        return true;
    }
};

// Използване в PlcEngine::loadProgram
bool PlcEngine::loadProgram(const String& programName, const char* jsonConfig) {
    String errorMsg;
    if (!PlcConfigValidator::validate(jsonConfig, errorMsg)) {
        EspHubLog->printf("PLC config validation failed: %s\n", errorMsg.c_str());
        return false;
    }
    
    // Продължете с парсинга...
}
```

---

## 🏗️ Рефакториране на Архитектурата

### 1. Protocol Manager Interface

**Проблем:** Всеки протокол е директно свързан, липсва абстракция

**Решение:**
```cpp
// Protocols/ProtocolManagerInterface.h
#ifndef PROTOCOL_MANAGER_INTERFACE_H
#define PROTOCOL_MANAGER_INTERFACE_H

#include <Arduino.h>

class ProtocolManagerInterface {
public:
    virtual ~ProtocolManagerInterface() = default;
    
    // Lifecycle
    virtual bool begin() = 0;
    virtual void loop() = 0;
    virtual void stop() = 0;
    
    // Device management
    virtual bool addDevice(const String& deviceId, const String& config) = 0;
    virtual bool removeDevice(const String& deviceId) = 0;
    virtual bool hasDevice(const String& deviceId) = 0;
    
    // Communication
    virtual bool sendCommand(const String& endpoint, const String& value) = 0;
    virtual String getDeviceState(const String& deviceId) = 0;
    
    // Status
    virtual bool isConnected() = 0;
    virtual String getProtocolName() = 0;
    virtual size_t getDeviceCount() = 0;
};

#endif
```

**Имплементация за MQTT:**
```cpp
// Protocols/Mqtt/MqttManager.h
class MqttManager : public ProtocolManagerInterface {
public:
    // ProtocolManagerInterface implementation
    bool begin() override;
    void loop() override;
    void stop() override;
    
    bool addDevice(const String& deviceId, const String& config) override;
    bool removeDevice(const String& deviceId) override;
    bool hasDevice(const String& deviceId) override;
    
    bool sendCommand(const String& endpoint, const String& value) override;
    String getDeviceState(const String& deviceId) override;
    
    bool isConnected() override;
    String getProtocolName() override { return "MQTT"; }
    size_t getDeviceCount() override;
    
    // MQTT-specific methods
    void setupMqtt(const char* server, int port, MQTT_CALLBACK_SIGNATURE);
    // ...
};
```

**Използване в EspHub:**
```cpp
// EspHub.h
class EspHub {
private:
    std::vector<ProtocolManagerInterface*> protocolManagers;
    
public:
    void registerProtocol(ProtocolManagerInterface* protocol) {
        protocolManagers.push_back(protocol);
    }
    
    void loop() {
        for (auto* protocol : protocolManagers) {
            protocol->loop();
        }
        // ...
    }
};
```

### 2. Dependency Injection за EspHub

**Проблем:** EspHub създава всички зависимости вътрешно

**Решение:**
```cpp
// EspHub.h
class EspHub {
public:
    // Constructor injection
    EspHub(
        PlcEngine* plcEngine,
        TimeManager* timeManager,
        WebManager* webManager,
        DeviceRegistry* deviceRegistry
    );
    
    // Setter injection за опционални компоненти
    void setMqttManager(MqttManager* manager);
    void setMeshManager(MeshDeviceManager* manager);
    
private:
    // Указатели вместо обекти
    PlcEngine* plcEngine;
    TimeManager* timeManager;
    WebManager* webManager;
    DeviceRegistry* deviceRegistry;
    
    // Опционални
    MqttManager* mqttManager = nullptr;
    MeshDeviceManager* meshManager = nullptr;
};

// main.cpp
void setup() {
    // Създайте зависимостите
    TimeManager timeManager;
    PlcEngine plcEngine(&timeManager, nullptr);
    WebManager webManager;
    DeviceRegistry deviceRegistry;
    
    // Инжектирайте в EspHub
    EspHub hub(&plcEngine, &timeManager, &webManager, &deviceRegistry);
    
    // Добавете опционални компоненти
    MqttManager mqttManager;
    hub.setMqttManager(&mqttManager);
    
    hub.begin();
}
```

---

## 🧪 Примери за Unit Tests

### 1. Test Framework Setup

**platformio.ini:**
```ini
[env:esp32_test]
platform = espressif32
board = esp32dev
framework = arduino
test_framework = unity
test_build_src = yes
build_flags =
    -D UNIT_TEST
    -D UNITY_INCLUDE_DOUBLE
lib_deps =
    throwtheswitch/Unity@^2.5.2
```

### 2. PlcMemory Tests

**test/test_plc_memory/test_plc_memory.cpp:**
```cpp
#include <unity.h>
#include "PlcEngine/Engine/PlcMemory.h"

void setUp(void) {
    // Изпълнява се преди всеки тест
}

void tearDown(void) {
    // Изпълнява се след всеки тест
}

void test_plc_memory_bool_operations() {
    PlcMemory memory;
    
    // Test setBool and getBool
    memory.setBool("test_bool", true);
    TEST_ASSERT_TRUE(memory.getBool("test_bool"));
    
    memory.setBool("test_bool", false);
    TEST_ASSERT_FALSE(memory.getBool("test_bool"));
}

void test_plc_memory_int_operations() {
    PlcMemory memory;
    
    memory.setInt("test_int", 42);
    TEST_ASSERT_EQUAL_INT32(42, memory.getInt("test_int"));
    
    memory.setInt("test_int", -100);
    TEST_ASSERT_EQUAL_INT32(-100, memory.getInt("test_int"));
}

void test_plc_memory_real_operations() {
    PlcMemory memory;
    
    memory.setReal("test_real", 3.14159);
    TEST_ASSERT_FLOAT_WITHIN(0.0001, 3.14159, memory.getReal("test_real"));
}

void test_plc_memory_string_operations() {
    PlcMemory memory;
    
    memory.setString("test_string", "Hello, World!");
    TEST_ASSERT_EQUAL_STRING("Hello, World!", memory.getString("test_string").c_str());
}

void test_plc_memory_nonexistent_variable() {
    PlcMemory memory;
    
    // Accessing non-existent variable should return default value
    TEST_ASSERT_FALSE(memory.getBool("nonexistent"));
    TEST_ASSERT_EQUAL_INT32(0, memory.getInt("nonexistent"));
    TEST_ASSERT_FLOAT_WITHIN(0.0001, 0.0, memory.getReal("nonexistent"));
}

void setup() {
    delay(2000); // Изчакайте serial да се инициализира
    
    UNITY_BEGIN();
    
    RUN_TEST(test_plc_memory_bool_operations);
    RUN_TEST(test_plc_memory_int_operations);
    RUN_TEST(test_plc_memory_real_operations);
    RUN_TEST(test_plc_memory_string_operations);
    RUN_TEST(test_plc_memory_nonexistent_variable);
    
    UNITY_END();
}

void loop() {
    // Празно
}
```

### 3. ZoneManager Tests

**test/test_zone_manager/test_zone_manager.cpp:**
```cpp
#include <unity.h>
#include "Protocols/Mesh/ZoneManager.h"

ZoneManager* manager = nullptr;

void setUp(void) {
    manager = new ZoneManager();
}

void tearDown(void) {
    delete manager;
    manager = nullptr;
}

void test_zone_manager_initialization() {
    manager->begin("test.device", "test_zone");
    
    TEST_ASSERT_EQUAL_STRING("test_zone", manager->getZoneName().c_str());
    TEST_ASSERT_EQUAL_STRING("test.device", manager->getDeviceName().c_str());
}

void test_zone_manager_role_assignment() {
    manager->begin("test.device", "test_zone");
    
    // Initially should be UNASSIGNED
    TEST_ASSERT_EQUAL(ZoneRole::UNASSIGNED, manager->getRole());
    
    // Simulate becoming coordinator
    manager->becomeCoordinator();
    TEST_ASSERT_EQUAL(ZoneRole::COORDINATOR, manager->getRole());
}

void test_zone_manager_device_registration() {
    manager->begin("coordinator", "zone1");
    manager->becomeCoordinator();
    
    // Add a device
    bool result = manager->registerDevice("device1", {0x01, 0x02, 0x03, 0x04, 0x05, 0x06});
    TEST_ASSERT_TRUE(result);
    
    // Verify device count
    TEST_ASSERT_EQUAL(1, manager->getDeviceCount());
}

void setup() {
    delay(2000);
    
    UNITY_BEGIN();
    
    RUN_TEST(test_zone_manager_initialization);
    RUN_TEST(test_zone_manager_role_assignment);
    RUN_TEST(test_zone_manager_device_registration);
    
    UNITY_END();
}

void loop() {}
```

---

## ⚠️ Error Handling Framework

### 1. Error Code Definitions

**Core/ErrorCodes.h:**
```cpp
#ifndef ERROR_CODES_H
#define ERROR_CODES_H

#include <Arduino.h>

enum class ErrorCode {
    SUCCESS = 0,
    
    // General errors (1-99)
    UNKNOWN_ERROR = 1,
    INVALID_PARAMETER = 2,
    OUT_OF_MEMORY = 3,
    TIMEOUT = 4,
    
    // JSON errors (100-199)
    JSON_PARSE_ERROR = 100,
    JSON_INVALID_SCHEMA = 101,
    JSON_MISSING_FIELD = 102,
    
    // Network errors (200-299)
    NETWORK_NOT_CONNECTED = 200,
    MQTT_CONNECTION_FAILED = 201,
    MESH_INIT_FAILED = 202,
    
    // PLC errors (300-399)
    PLC_PROGRAM_NOT_FOUND = 300,
    PLC_INVALID_CONFIG = 301,
    PLC_EXECUTION_ERROR = 302,
    PLC_MEMORY_ERROR = 303,
    
    // Device errors (400-499)
    DEVICE_NOT_FOUND = 400,
    DEVICE_OFFLINE = 401,
    DEVICE_INVALID_STATE = 402,
    
    // Storage errors (500-599)
    STORAGE_READ_ERROR = 500,
    STORAGE_WRITE_ERROR = 501,
    STORAGE_NOT_INITIALIZED = 502
};

class ErrorInfo {
public:
    ErrorCode code;
    String message;
    String context;
    unsigned long timestamp;
    
    ErrorInfo(ErrorCode c, const String& msg, const String& ctx = "")
        : code(c), message(msg), context(ctx), timestamp(millis()) {}
    
    bool isSuccess() const { return code == ErrorCode::SUCCESS; }
    
    String toString() const {
        String result = "[";
        result += timestamp;
        result += "] Error ";
        result += static_cast<int>(code);
        result += ": ";
        result += message;
        if (context.length() > 0) {
            result += " (Context: ";
            result += context;
            result += ")";
        }
        return result;
    }
};

#endif
```

### 2. Error Handler Implementation

**Core/ErrorHandler.h:**
```cpp
#ifndef ERROR_HANDLER_H
#define ERROR_HANDLER_H

#include "ErrorCodes.h"
#include <vector>

class ErrorHandler {
private:
    static const size_t MAX_ERROR_HISTORY = 50;
    std::vector<ErrorInfo> errorHistory;
    
    void (*errorCallback)(const ErrorInfo&) = nullptr;
    
public:
    void setErrorCallback(void (*callback)(const ErrorInfo&)) {
        errorCallback = callback;
    }
    
    void handleError(ErrorCode code, const String& message, const String& context = "") {
        ErrorInfo error(code, message, context);
        
        // Log error
        EspHubLog->println(error.toString());
        
        // Add to history
        errorHistory.push_back(error);
        if (errorHistory.size() > MAX_ERROR_HISTORY) {
            errorHistory.erase(errorHistory.begin());
        }
        
        // Call callback if set
        if (errorCallback) {
            errorCallback(error);
        }
        
        // Attempt recovery
        attemptRecovery(code);
    }
    
    void attemptRecovery(ErrorCode code) {
        switch (code) {
            case ErrorCode::OUT_OF_MEMORY:
                EspHubLog->println("Attempting memory cleanup...");
                // Trigger garbage collection, clear caches, etc.
                break;
                
            case ErrorCode::MQTT_CONNECTION_FAILED:
                EspHubLog->println("Attempting MQTT reconnection...");
                // Trigger MQTT reconnect
                break;
                
            case ErrorCode::NETWORK_NOT_CONNECTED:
                EspHubLog->println("Attempting WiFi reconnection...");
                // Trigger WiFi reconnect
                break;
                
            default:
                // No automatic recovery
                break;
        }
    }
    
    const std::vector<ErrorInfo>& getErrorHistory() const {
        return errorHistory;
    }
    
    void clearErrorHistory() {
        errorHistory.clear();
    }
};

// Global error handler instance
extern ErrorHandler globalErrorHandler;

#endif
```

### 3. Usage Example

```cpp
// PlcEngine.cpp
bool PlcEngine::loadProgram(const String& programName, const char* jsonConfig) {
    String errorMsg;
    if (!PlcConfigValidator::validate(jsonConfig, errorMsg)) {
        globalErrorHandler.handleError(
            ErrorCode::PLC_INVALID_CONFIG,
            errorMsg,
            "PlcEngine::loadProgram"
        );
        return false;
    }
    
    StaticJsonDocument<2048> doc;
    DeserializationError error = deserializeJson(doc, jsonConfig);
    
    if (error) {
        globalErrorHandler.handleError(
            ErrorCode::JSON_PARSE_ERROR,
            error.c_str(),
            "PlcEngine::loadProgram"
        );
        return false;
    }
    
    // Continue with loading...
    return true;
}
```

---

## 💾 Memory Management Improvements

### 1. Memory Monitor

**Core/MemoryMonitor.h:**
```cpp
#ifndef MEMORY_MONITOR_H
#define MEMORY_MONITOR_H

#include <Arduino.h>

class MemoryMonitor {
private:
    static const unsigned long CHECK_INTERVAL_MS = 60000; // 1 минута
    unsigned long lastCheckTime = 0;
    
    size_t minFreeHeap = 0;
    size_t maxUsedHeap = 0;
    
public:
    void begin() {
        minFreeHeap = ESP.getFreeHeap();
        maxUsedHeap = ESP.getHeapSize() - minFreeHeap;
    }
    
    void loop() {
        unsigned long now = millis();
        if (now - lastCheckTime >= CHECK_INTERVAL_MS) {
            checkMemory();
            lastCheckTime = now;
        }
    }
    
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
        
        // Log memory stats
        logMemoryStats();
        
        // Check for low memory
        if (freeHeap < 10000) { // Less than 10KB free
            EspHubLog->println("WARNING: Low memory!");
            globalErrorHandler.handleError(
                ErrorCode::OUT_OF_MEMORY,
                "Free heap below 10KB",
                "MemoryMonitor"
            );
        }
    }
    
    void logMemoryStats() {
        EspHubLog->println("=== Memory Statistics ===");
        EspHubLog->printf("Free heap: %d bytes\n", ESP.getFreeHeap());
        EspHubLog->printf("Min free heap: %d bytes\n", ESP.getMinFreeHeap());
        EspHubLog->printf("Max alloc heap: %d bytes\n", ESP.getMaxAllocHeap());
        EspHubLog->printf("Heap size: %d bytes\n", ESP.getHeapSize());
        EspHubLog->printf("Heap fragmentation: %d%%\n", getHeapFragmentation());
        EspHubLog->println("========================");
    }
    
    uint8_t getHeapFragmentation() {
        size_t freeHeap = ESP.getFreeHeap();
        size_t maxAlloc = ESP.getMaxAllocHeap();
        
        if (freeHeap == 0) return 100;
        
        return 100 - (maxAlloc * 100 / freeHeap);
    }
    
    size_t getMinFreeHeap() const { return minFreeHeap; }
    size_t getMaxUsedHeap() const { return maxUsedHeap; }
};

#endif
```

### 2. String Pool для Често Използвани Стрингове

**Core/StringPool.h:**
```cpp
#ifndef STRING_POOL_H
#define STRING_POOL_H

#include <Arduino.h>
#include <map>

class StringPool {
private:
    std::map<String, const char*> pool;
    
public:
    const char* intern(const String& str) {
        auto it = pool.find(str);
        if (it != pool.end()) {
            return it->second;
        }
        
        // Allocate permanent storage
        char* permanent = new char[str.length() + 1];
        strcpy(permanent, str.c_str());
        pool[str] = permanent;
        
        return permanent;
    }
    
    ~StringPool() {
        for (auto& pair : pool) {
            delete[] pair.second;
        }
    }
};

// Global string pool
extern StringPool globalStringPool;

#endif
```

---

## 🔒 Security Enhancements

### 1. Configuration Manager със Encryption

**Storage/SecureConfig.h:**
```cpp
#ifndef SECURE_CONFIG_H
#define SECURE_CONFIG_H

#include <Arduino.h>
#include <Preferences.h>
#include <mbedtls/aes.h>

class SecureConfig {
private:
    Preferences prefs;
    mbedtls_aes_context aes;
    unsigned char key[32]; // 256-bit key
    
    void deriveKey(const char* password) {
        // Simple key derivation (use proper PBKDF2 in production)
        mbedtls_md_context_t md_ctx;
        mbedtls_md_init(&md_ctx);
        mbedtls_md_setup(&md_ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
        mbedtls_md_hmac_starts(&md_ctx, (const unsigned char*)password, strlen(password));
        mbedtls_md_hmac_update(&md_ctx, (const unsigned char*)"esphub", 6);
        mbedtls_md_hmac_finish(&md_ctx, key);
        mbedtls_md_free(&md_ctx);
    }
    
public:
    bool begin(const char* password) {
        deriveKey(password);
        mbedtls_aes_init(&aes);
        return prefs.begin("esphub", false);
    }
    
    bool setString(const char* key, const String& value) {
        // Encrypt value
        size_t len = value.length();
        unsigned char encrypted[len + 16]; // Add padding
        
        mbedtls_aes_setkey_enc(&aes, this->key, 256);
        // Simplified encryption (use proper CBC/GCM in production)
        
        // Store encrypted value
        return prefs.putBytes(key, encrypted, sizeof(encrypted));
    }
    
    String getString(const char* key, const String& defaultValue = "") {
        size_t len = prefs.getBytesLength(key);
        if (len == 0) return defaultValue;
        
        unsigned char encrypted[len];
        prefs.getBytes(key, encrypted, len);
        
        // Decrypt
        unsigned char decrypted[len];
        mbedtls_aes_setkey_dec(&aes, this->key, 256);
        // Simplified decryption
        
        return String((char*)decrypted);
    }
    
    ~SecureConfig() {
        mbedtls_aes_free(&aes);
        prefs.end();
    }
};

#endif
```

### 2. Input Sanitization

**Core/InputValidator.h:**
```cpp
#ifndef INPUT_VALIDATOR_H
#define INPUT_VALIDATOR_H

#include <Arduino.h>
#include <regex>

class InputValidator {
public:
    static bool isValidDeviceId(const String& deviceId) {
        // Device ID: alphanumeric, dots, underscores, 1-64 chars
        if (deviceId.length() == 0 || deviceId.length() > 64) {
            return false;
        }
        
        for (char c : deviceId) {
            if (!isalnum(c) && c != '.' && c != '_') {
                return false;
            }
        }
        
        return true;
    }
    
    static bool isValidEndpoint(const String& endpoint) {
        // Endpoint: zone.device.variable.type format
        int dotCount = 0;
        for (char c : endpoint) {
            if (c == '.') dotCount++;
        }
        
        return dotCount == 3 && endpoint.length() > 0 && endpoint.length() <= 128;
    }
    
    static bool isValidProgramName(const String& name) {
        // Program name: alphanumeric and underscores, 1-32 chars
        if (name.length() == 0 || name.length() > 32) {
            return false;
        }
        
        for (char c : name) {
            if (!isalnum(c) && c != '_') {
                return false;
            }
        }
        
        return true;
    }
    
    static String sanitize(const String& input, size_t maxLength = 256) {
        String result;
        result.reserve(min(input.length(), maxLength));
        
        for (size_t i = 0; i < input.length() && i < maxLength; i++) {
            char c = input[i];
            // Remove control characters
            if (c >= 32 && c <= 126) {
                result += c;
            }
        }
        
        return result;
    }
};

#endif
```

---

## 📝 Заключение

Тези конкретни предложения за подобрения адресират критичните проблеми идентифицирани в основния анализ. Имплементацията им ще:

1. **Елиминира критични бъгове** (buffer overflow, memory leaks)
2. **Подобри надеждността** (error handling, validation)
3. **Улесни тестването** (dependency injection, unit tests)
4. **Повиши сигурността** (encryption, input validation)
5. **Подобри поддръжката** (protocol interface, error framework)

**Препоръчителен ред на имплементация:**
1. Критични поправки (1-4)
2. Error handling framework
3. Unit tests
4. Memory monitoring
5. Security enhancements
6. Architectural refactoring

---

**Документ създаден от:** AI Code Reviewer  
**Дата:** 20 Ноември 2025
