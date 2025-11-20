# Phase 2: Error Handling & Testing Framework

**Дата:** 2025-11-20  
**Статус:** Implementation Guide  
**Локация:** `workDocs/improvements/`

---

## Overview

Фаза 2 добавя robust error handling система и unit testing infrastructure за подобряване на надеждността и поддръжката на кода.

---

## Part 1: Error Handling Framework

### File 1: ErrorCodes.h

**Локация:** `lib/Core/ErrorCodes.h` (NEW)

```cpp
#ifndef ERROR_CODES_H
#define ERROR_CODES_H

#include <Arduino.h>

/**
 * @brief Error code enumeration
 * 
 * Comprehensive error codes for all subsystems.
 * Organized by category for easy identification.
 */
enum class ErrorCode {
    SUCCESS = 0,
    
    // General errors (1-99)
    UNKNOWN_ERROR = 1,
    INVALID_PARAMETER = 2,
    OUT_OF_MEMORY = 3,
    TIMEOUT = 4,
    NOT_INITIALIZED = 5,
    ALREADY_EXISTS = 6,
    NOT_FOUND = 7,
    
    // JSON errors (100-199)
    JSON_PARSE_ERROR = 100,
    JSON_INVALID_SCHEMA = 101,
    JSON_MISSING_FIELD = 102,
    JSON_INVALID_TYPE = 103,
    JSON_BUFFER_TOO_SMALL = 104,
    
    // Network errors (200-299)
    NETWORK_NOT_CONNECTED = 200,
    MQTT_CONNECTION_FAILED = 201,
    MQTT_PUBLISH_FAILED = 202,
    MQTT_SUBSCRIBE_FAILED = 203,
    MESH_INIT_FAILED = 204,
    MESH_SEND_FAILED = 205,
    WIFI_CONNECTION_FAILED = 206,
    
    // PLC errors (300-399)
    PLC_PROGRAM_NOT_FOUND = 300,
    PLC_INVALID_CONFIG = 301,
    PLC_EXECUTION_ERROR = 302,
    PLC_MEMORY_ERROR = 303,
    PLC_BLOCK_NOT_FOUND = 304,
    PLC_VARIABLE_NOT_FOUND = 305,
    PLC_TYPE_MISMATCH = 306,
    PLC_OUTPUT_CONFLICT = 307,
    
    // Device errors (400-499)
    DEVICE_NOT_FOUND = 400,
    DEVICE_OFFLINE = 401,
    DEVICE_INVALID_STATE = 402,
    DEVICE_COMMUNICATION_ERROR = 403,
    DEVICE_TIMEOUT = 404,
    
    // Storage errors (500-599)
    STORAGE_READ_ERROR = 500,
    STORAGE_WRITE_ERROR = 501,
    STORAGE_NOT_INITIALIZED = 502,
    STORAGE_FULL = 503,
    STORAGE_CORRUPTED = 504,
    
    // Zone/Mesh errors (600-699)
    ZONE_NOT_FOUND = 600,
    ZONE_COORDINATOR_NOT_ELECTED = 601,
    ZONE_ROUTE_NOT_FOUND = 602,
    ZONE_SUBSCRIPTION_FAILED = 603,
    
    // Event system errors (700-799)
    EVENT_INVALID_TRIGGER = 700,
    EVENT_PROGRAM_NOT_FOUND = 701,
    EVENT_EXECUTION_FAILED = 702
};

/**
 * @brief Error severity levels
 */
enum class ErrorSeverity {
    INFO,       // Informational, no action needed
    WARNING,    // Warning, may need attention
    ERROR,      // Error, functionality affected
    CRITICAL    // Critical, system stability at risk
};

/**
 * @brief Error information structure
 * 
 * Contains all information about an error occurrence.
 */
class ErrorInfo {
public:
    ErrorCode code;
    ErrorSeverity severity;
    String message;
    String context;
    unsigned long timestamp;
    
    ErrorInfo(ErrorCode c, const String& msg, const String& ctx = "", 
              ErrorSeverity sev = ErrorSeverity::ERROR)
        : code(c), severity(sev), message(msg), context(ctx), timestamp(millis()) {}
    
    /**
     * @brief Check if error represents success
     */
    bool isSuccess() const { 
        return code == ErrorCode::SUCCESS; 
    }
    
    /**
     * @brief Get severity as string
     */
    const char* getSeverityString() const {
        switch (severity) {
            case ErrorSeverity::INFO: return "INFO";
            case ErrorSeverity::WARNING: return "WARNING";
            case ErrorSeverity::ERROR: return "ERROR";
            case ErrorSeverity::CRITICAL: return "CRITICAL";
            default: return "UNKNOWN";
        }
    }
    
    /**
     * @brief Convert to string representation
     */
    String toString() const {
        String result = "[";
        result += timestamp;
        result += "] ";
        result += getSeverityString();
        result += " (";
        result += static_cast<int>(code);
        result += "): ";
        result += message;
        
        if (context.length() > 0) {
            result += " | Context: ";
            result += context;
        }
        
        return result;
    }
    
    /**
     * @brief Convert to JSON
     */
    String toJson() const {
        String json = "{";
        json += "\"timestamp\":";
        json += timestamp;
        json += ",\"severity\":\"";
        json += getSeverityString();
        json += "\",\"code\":";
        json += static_cast<int>(code);
        json += ",\"message\":\"";
        json += message;
        json += "\"";
        
        if (context.length() > 0) {
            json += ",\"context\":\"";
            json += context;
            json += "\"";
        }
        
        json += "}";
        return json;
    }
};

#endif // ERROR_CODES_H
```

---

### File 2: ErrorHandler.h

**Локация:** `lib/Core/ErrorHandler.h` (NEW)

```cpp
#ifndef ERROR_HANDLER_H
#define ERROR_HANDLER_H

#include "ErrorCodes.h"
#include "StreamLogger.h"
#include <vector>

extern StreamLogger* EspHubLog;

/**
 * @brief Global error handler
 * 
 * Centralized error handling with:
 * - Error logging
 * - Error history
 * - Recovery strategies
 * - Callbacks for custom handling
 */
class ErrorHandler {
private:
    static const size_t MAX_ERROR_HISTORY = 50;
    std::vector<ErrorInfo> errorHistory;
    
    // Callback for custom error handling
    void (*errorCallback)(const ErrorInfo&) = nullptr;
    
    // Statistics
    uint32_t totalErrors = 0;
    uint32_t criticalErrors = 0;
    uint32_t warningCount = 0;
    
public:
    /**
     * @brief Set custom error callback
     * 
     * @param callback Function to call when error occurs
     */
    void setErrorCallback(void (*callback)(const ErrorInfo&)) {
        errorCallback = callback;
    }
    
    /**
     * @brief Handle an error
     * 
     * @param code Error code
     * @param message Error message
     * @param context Optional context information
     * @param severity Error severity (default: ERROR)
     */
    void handleError(ErrorCode code, const String& message, 
                     const String& context = "", 
                     ErrorSeverity severity = ErrorSeverity::ERROR) {
        ErrorInfo error(code, message, context, severity);
        
        // Update statistics
        totalErrors++;
        if (severity == ErrorSeverity::CRITICAL) {
            criticalErrors++;
        } else if (severity == ErrorSeverity::WARNING) {
            warningCount++;
        }
        
        // Log error
        if (EspHubLog) {
            EspHubLog->println(error.toString());
        }
        
        // Add to history
        errorHistory.push_back(error);
        if (errorHistory.size() > MAX_ERROR_HISTORY) {
            errorHistory.erase(errorHistory.begin());
        }
        
        // Call custom callback if set
        if (errorCallback) {
            errorCallback(error);
        }
        
        // Attempt automatic recovery
        attemptRecovery(code);
    }
    
    /**
     * @brief Attempt automatic error recovery
     * 
     * @param code Error code to recover from
     */
    void attemptRecovery(ErrorCode code) {
        switch (code) {
            case ErrorCode::OUT_OF_MEMORY:
                if (EspHubLog) {
                    EspHubLog->println("Recovery: Attempting memory cleanup...");
                }
                // Could trigger garbage collection, clear caches, etc.
                break;
                
            case ErrorCode::MQTT_CONNECTION_FAILED:
                if (EspHubLog) {
                    EspHubLog->println("Recovery: MQTT reconnection will be attempted");
                }
                // MQTT manager should handle reconnection automatically
                break;
                
            case ErrorCode::NETWORK_NOT_CONNECTED:
                if (EspHubLog) {
                    EspHubLog->println("Recovery: WiFi reconnection will be attempted");
                }
                // WiFi reconnection logic
                break;
                
            case ErrorCode::DEVICE_OFFLINE:
                if (EspHubLog) {
                    EspHubLog->println("Recovery: Device will be marked as offline");
                }
                // Device manager should handle this
                break;
                
            default:
                // No automatic recovery available
                break;
        }
    }
    
    /**
     * @brief Get error history
     * 
     * @return Vector of error info objects
     */
    const std::vector<ErrorInfo>& getErrorHistory() const {
        return errorHistory;
    }
    
    /**
     * @brief Get error history as JSON array
     * 
     * @param maxCount Maximum number of errors to include (0 = all)
     * @return JSON string
     */
    String getErrorHistoryJson(size_t maxCount = 0) const {
        String json = "[";
        
        size_t count = maxCount > 0 ? min(maxCount, errorHistory.size()) : errorHistory.size();
        size_t startIdx = errorHistory.size() - count;
        
        for (size_t i = startIdx; i < errorHistory.size(); i++) {
            if (i > startIdx) json += ",";
            json += errorHistory[i].toJson();
        }
        
        json += "]";
        return json;
    }
    
    /**
     * @brief Clear error history
     */
    void clearErrorHistory() {
        errorHistory.clear();
    }
    
    /**
     * @brief Get total error count
     */
    uint32_t getTotalErrors() const {
        return totalErrors;
    }
    
    /**
     * @brief Get critical error count
     */
    uint32_t getCriticalErrors() const {
        return criticalErrors;
    }
    
    /**
     * @brief Get warning count
     */
    uint32_t getWarningCount() const {
        return warningCount;
    }
    
    /**
     * @brief Get statistics as string
     */
    String getStatistics() const {
        String stats = "Error Statistics:\n";
        stats += "  Total errors: ";
        stats += totalErrors;
        stats += "\n  Critical: ";
        stats += criticalErrors;
        stats += "\n  Warnings: ";
        stats += warningCount;
        stats += "\n  History size: ";
        stats += errorHistory.size();
        return stats;
    }
    
    /**
     * @brief Reset all statistics
     */
    void resetStatistics() {
        totalErrors = 0;
        criticalErrors = 0;
        warningCount = 0;
    }
};

// Global error handler instance
extern ErrorHandler globalErrorHandler;

#endif // ERROR_HANDLER_H
```

---

### File 3: ErrorHandler.cpp

**Локация:** `lib/Core/ErrorHandler.cpp` (NEW)

```cpp
#include "ErrorHandler.h"

// Global error handler instance
ErrorHandler globalErrorHandler;
```

---

## Integration into EspHub

### Step 1: Add to EspHub.h

**Add includes:**
```cpp
#include "ErrorCodes.h"
#include "ErrorHandler.h"
```

**Add member (optional, if you want instance-level handler):**
```cpp
private:
    // ... existing members ...
    // Note: We're using global handler, so no member needed
```

### Step 2: Usage Examples

#### Example 1: In PlcEngine.cpp

```cpp
#include "../Core/ErrorHandler.h"

bool PlcEngine::loadProgram(const String& programName, const char* jsonConfig) {
    String errorMsg;
    if (!PlcConfigValidator::validate(jsonConfig, errorMsg)) {
        // Use error handler instead of just logging
        globalErrorHandler.handleError(
            ErrorCode::PLC_INVALID_CONFIG,
            errorMsg,
            "PlcEngine::loadProgram",
            ErrorSeverity::ERROR
        );
        return false;
    }
    
    // ... rest of implementation
}
```

#### Example 2: In MqttManager.cpp

```cpp
#include "../Core/ErrorHandler.h"

bool MqttManager::connect() {
    if (!mqttClient.connect(clientId)) {
        globalErrorHandler.handleError(
            ErrorCode::MQTT_CONNECTION_FAILED,
            "Failed to connect to MQTT broker",
            String("Broker: ") + mqttServer,
            ErrorSeverity::ERROR
        );
        return false;
    }
    return true;
}
```

#### Example 3: In DeviceRegistry.cpp

```cpp
#include "../Core/ErrorHandler.h"

bool DeviceRegistry::getDeviceState(const String& deviceId, String& state) {
    if (!hasDevice(deviceId)) {
        globalErrorHandler.handleError(
            ErrorCode::DEVICE_NOT_FOUND,
            "Device not found in registry",
            String("Device ID: ") + deviceId,
            ErrorSeverity::WARNING
        );
        return false;
    }
    // ... get state
}
```

---

## Web API for Error History

### Add to WebManager

**In WebManager.cpp, add endpoint:**

```cpp
void WebManager::setupRoutes() {
    // ... existing routes ...
    
    // Error history endpoint
    server->on("/api/errors", HTTP_GET, [](AsyncWebServerRequest *request) {
        String json = globalErrorHandler.getErrorHistoryJson(50); // Last 50 errors
        request->send(200, "application/json", json);
    });
    
    // Error statistics endpoint
    server->on("/api/errors/stats", HTTP_GET, [](AsyncWebServerRequest *request) {
        String stats = "{";
        stats += "\"total\":";
        stats += globalErrorHandler.getTotalErrors();
        stats += ",\"critical\":";
        stats += globalErrorHandler.getCriticalErrors();
        stats += ",\"warnings\":";
        stats += globalErrorHandler.getWarningCount();
        stats += "}";
        request->send(200, "application/json", stats);
    });
    
    // Clear error history
    server->on("/api/errors/clear", HTTP_POST, [](AsyncWebServerRequest *request) {
        globalErrorHandler.clearErrorHistory();
        request->send(200, "text/plain", "Error history cleared");
    });
}
```

---

## MQTT Integration

### Publish Errors to MQTT

**Add to EspHub.cpp or create ErrorMqttPublisher:**

```cpp
// In EspHub::begin(), set error callback
void EspHub::begin() {
    // ... existing code ...
    
    // Set error callback to publish critical errors to MQTT
    globalErrorHandler.setErrorCallback([](const ErrorInfo& error) {
        if (error.severity == ErrorSeverity::CRITICAL) {
            // Publish to MQTT if connected
            if (instance && instance->mqttManager.isConnected()) {
                String topic = "esphub/errors/critical";
                instance->mqttManager.publish(topic.c_str(), error.toJson().c_str());
            }
        }
    });
}
```

---

## Verification Steps

### 1. Compile Test
```powershell
platformio run -e esp32_full
```

### 2. Test Error Handling

**Trigger test errors:**
```cpp
// In setup() or via MQTT command
globalErrorHandler.handleError(
    ErrorCode::PLC_INVALID_CONFIG,
    "Test error message",
    "Test context",
    ErrorSeverity::WARNING
);
```

### 3. Check Error History

**Via Web API:**
```
GET http://esphub.local/api/errors
GET http://esphub.local/api/errors/stats
```

### 4. Monitor Serial Output

Expected output:
```
[12345] ERROR (301): Test error message | Context: Test context
Error Statistics:
  Total errors: 1
  Critical: 0
  Warnings: 1
  History size: 1
```

---

## Git Commit

```powershell
git add lib/Core/ErrorCodes.h
git add lib/Core/ErrorHandler.h
git add lib/Core/ErrorHandler.cpp
git commit -m "feat: add comprehensive error handling framework

- Create ErrorCodes enum with categorized error codes
- Implement ErrorHandler with error history and recovery
- Add error severity levels (INFO, WARNING, ERROR, CRITICAL)
- Support custom error callbacks
- Add error statistics tracking
- Integrate with Web API and MQTT
- Provides foundation for robust error management"
```

---

## Next: Unit Testing (Phase 2.2)

След като error handling е имплементиран, следващата стъпка е unit testing infrastructure.

---

**Status:** Ready for Implementation  
**Estimated Time:** 20-30 minutes  
**Risk Level:** Low-Medium  
**Dependencies:** None

**Created by:** AI Code Reviewer  
**Date:** 2025-11-20
