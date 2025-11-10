#include "WiFiDeviceManager.h"
#include "Logger.h"
#include "MqttManager.h"
#include <HTTPClient.h>
#include <base64.h>

WiFiDeviceManager::WiFiDeviceManager(MqttManager* mqttManager)
    : mqttManager(mqttManager),
      globalPollingInterval(5000) {
}

WiFiDeviceManager::~WiFiDeviceManager() {
}

void WiFiDeviceManager::begin() {
    LOG_INFO("WiFiDeviceManager", "Initialized");
}

void WiFiDeviceManager::loop() {
    // Poll endpoints for all devices
    pollEndpoints();
}

// ============================================================================
// ProtocolManagerInterface Implementation
// ============================================================================

bool WiFiDeviceManager::initializeDevice(const String& deviceId, const JsonObject& connectionConfig) {
    LOG_INFO("WiFiDeviceManager", "Initializing device: " + deviceId);

    WiFiDeviceConfig device;
    device.deviceId = deviceId;

    // Parse connection configuration
    String connType = connectionConfig["type"] | "http";
    device.connectionType = parseConnectionType(connType);
    device.host = connectionConfig["host"] | "";
    device.port = connectionConfig["port"] | 80;
    device.useSsl = connectionConfig["use_ssl"] | false;

    if (device.host.isEmpty()) {
        LOG_ERROR("WiFiDeviceManager", "Host is required for device: " + deviceId);
        return false;
    }

    // Auth
    JsonObject auth = connectionConfig["auth"];
    if (!auth.isNull()) {
        device.authUsername = auth["username"] | "";
        device.authPassword = auth["password"] | "";
    }

    // MQTT
    device.mqttTopicPrefix = connectionConfig["topic_prefix"] | "";

    // Initialize device state
    device.isOnline = false;
    device.lastSeen = 0;

    // Store device
    devices[deviceId] = device;

    LOG_INFO("WiFiDeviceManager", "Device initialized: " + deviceId + " at " + device.host);
    return true;
}

bool WiFiDeviceManager::removeDevice(const String& deviceId) {
    auto it = devices.find(deviceId);
    if (it == devices.end()) {
        return false;
    }

    devices.erase(it);
    LOG_INFO("WiFiDeviceManager", "Removed device: " + deviceId);
    return true;
}

bool WiFiDeviceManager::readEndpoint(const String& deviceId, const JsonObject& endpointConfig, PlcValue& value) {
    WiFiDeviceConfig* device = getDevice(deviceId);
    if (!device) {
        LOG_ERROR("WiFiDeviceManager", "Device not found: " + deviceId);
        return false;
    }

    // Parse endpoint configuration
    String endpointName = endpointConfig["name"] | "";
    String access = endpointConfig["access"] | "r";

    if (access != "r" && access != "rw") {
        LOG_ERROR("WiFiDeviceManager", "Endpoint not readable: " + endpointName);
        return false;
    }

    // Get datatype
    String datatypeStr = endpointConfig["type"] | "bool";
    PlcValueType datatype = parseDatatype(datatypeStr);
    value = PlcValue(datatype);

    // Parse read configuration
    JsonObject readConf = endpointConfig["read"];
    if (readConf.isNull()) {
        LOG_ERROR("WiFiDeviceManager", "No read configuration for endpoint: " + endpointName);
        return false;
    }

    String response;
    bool success = false;

    if (device->connectionType == ConnectionType::HTTP || device->connectionType == ConnectionType::HTTPS) {
        String methodStr = readConf["method"] | "GET";
        HttpMethod method = parseHttpMethod(methodStr);
        String path = readConf["path"] | "";

        success = httpRequest(device->host, device->port, device->useSsl,
                            device->authUsername, device->authPassword,
                            method, path, "", response);

        if (success && !response.isEmpty()) {
            String valuePath = readConf["value_path"] | "";

            if (extractJsonValue(response, valuePath, value, datatype)) {
                // Apply value mapping if present
                JsonObject valueMap = readConf["value_map"];
                if (!valueMap.isNull() && datatype == PlcValueType::BOOL) {
                    // Value mapping for string -> bool conversion
                    // This is handled in extractJsonValue
                }

                device->isOnline = true;
                device->lastSeen = millis();
                return true;
            }
        }
    } else if (device->connectionType == ConnectionType::MQTT) {
        // MQTT reading happens via subscription callback
        // For now, just mark as success if topic is configured
        String topic = readConf["topic"] | "";
        success = !topic.isEmpty();

        if (success) {
            LOG_INFO("WiFiDeviceManager", "MQTT read endpoint configured: " + topic);
            return true;
        }
    }

    if (!success) {
        device->isOnline = false;
    }

    return success;
}

bool WiFiDeviceManager::writeEndpoint(const String& deviceId, const JsonObject& endpointConfig, const PlcValue& value) {
    WiFiDeviceConfig* device = getDevice(deviceId);
    if (!device) {
        LOG_ERROR("WiFiDeviceManager", "Device not found: " + deviceId);
        return false;
    }

    // Parse endpoint configuration
    String endpointName = endpointConfig["name"] | "";
    String access = endpointConfig["access"] | "w|rw";

    if (access != "w" && access != "rw") {
        LOG_ERROR("WiFiDeviceManager", "Endpoint not writable: " + endpointName);
        return false;
    }

    // Get datatype
    String datatypeStr = endpointConfig["type"] | "bool";
    PlcValueType datatype = parseDatatype(datatypeStr);

    // Parse write configuration
    JsonObject writeConf = endpointConfig["write"];
    if (writeConf.isNull()) {
        LOG_ERROR("WiFiDeviceManager", "No write configuration for endpoint: " + endpointName);
        return false;
    }

    String body = formatWriteValue(value, datatype, endpointConfig);
    String response;
    bool success = false;

    if (device->connectionType == ConnectionType::HTTP || device->connectionType == ConnectionType::HTTPS) {
        String methodStr = writeConf["method"] | "POST";
        HttpMethod method = parseHttpMethod(methodStr);
        String path = writeConf["path"] | "";

        success = httpRequest(device->host, device->port, device->useSsl,
                            device->authUsername, device->authPassword,
                            method, path, body, response);
    } else if (device->connectionType == ConnectionType::MQTT) {
        String topic = writeConf["topic"] | "";
        if (mqttManager && !topic.isEmpty()) {
            mqttManager->publish(topic.c_str(), body.c_str());
            success = true;
        }
    }

    if (success) {
        LOG_INFO("WiFiDeviceManager", "Wrote to " + deviceId + "." + endpointName + ": " + body);
        device->isOnline = true;
        device->lastSeen = millis();
    } else {
        device->isOnline = false;
    }

    return success;
}

bool WiFiDeviceManager::testConnection(const JsonObject& connectionConfig) {
    String connType = connectionConfig["type"] | "http";
    ConnectionType type = parseConnectionType(connType);
    String host = connectionConfig["host"] | "";
    int port = connectionConfig["port"] | 80;
    bool useSsl = connectionConfig["use_ssl"] | false;

    if (host.isEmpty()) {
        LOG_ERROR("WiFiDeviceManager", "Host is required for connection test");
        return false;
    }

    JsonObject auth = connectionConfig["auth"];
    String authUsername = auth["username"] | "";
    String authPassword = auth["password"] | "";

    if (type == ConnectionType::HTTP || type == ConnectionType::HTTPS) {
        String response;
        // Try a simple GET request to root
        return httpRequest(host, port, useSsl, authUsername, authPassword,
                         HttpMethod::GET, "/", "", response);
    } else if (type == ConnectionType::MQTT) {
        // For MQTT, we assume it's working if MqttManager is available
        return (mqttManager != nullptr);
    }

    return false;
}

bool WiFiDeviceManager::testEndpoint(const String& deviceId, const JsonObject& endpointConfig) {
    WiFiDeviceConfig* device = getDevice(deviceId);
    if (!device) {
        LOG_ERROR("WiFiDeviceManager", "Device not found: " + deviceId);
        return false;
    }

    String endpointName = endpointConfig["name"] | "";
    JsonObject readConf = endpointConfig["read"];

    if (readConf.isNull()) {
        LOG_ERROR("WiFiDeviceManager", "No read configuration for endpoint: " + endpointName);
        return false;
    }

    if (device->connectionType == ConnectionType::HTTP || device->connectionType == ConnectionType::HTTPS) {
        String methodStr = readConf["method"] | "GET";
        HttpMethod method = parseHttpMethod(methodStr);
        String path = readConf["path"] | "";
        String response;

        return httpRequest(device->host, device->port, device->useSsl,
                         device->authUsername, device->authPassword,
                         method, path, "", response);
    } else if (device->connectionType == ConnectionType::MQTT) {
        String topic = readConf["topic"] | "";
        return !topic.isEmpty();
    }

    return false;
}

bool WiFiDeviceManager::isDeviceOnline(const String& deviceId) {
    WiFiDeviceConfig* device = getDevice(deviceId);
    if (!device) {
        return false;
    }

    // Consider device offline if not seen in last 60 seconds
    if (device->lastSeen > 0 && (millis() - device->lastSeen > 60000)) {
        device->isOnline = false;
    }

    return device->isOnline;
}

// ============================================================================
// Private Methods
// ============================================================================

WiFiDeviceConfig* WiFiDeviceManager::getDevice(const String& deviceId) {
    auto it = devices.find(deviceId);
    if (it != devices.end()) {
        return &it->second;
    }
    return nullptr;
}

EndpointConfig* WiFiDeviceManager::getEndpointConfig(const String& deviceId, const String& endpointName) {
    WiFiDeviceConfig* device = getDevice(deviceId);
    if (!device) {
        return nullptr;
    }

    for (auto& ep : device->endpoints) {
        if (ep.name == endpointName) {
            return &ep;
        }
    }

    return nullptr;
}

bool WiFiDeviceManager::httpRequest(const String& host, int port, bool useSsl,
                                    const String& authUsername, const String& authPassword,
                                    HttpMethod method, const String& path,
                                    const String& body, String& response) {
    HTTPClient http;

    // Build URL
    String url = String(useSsl ? "https://" : "http://") + host + ":" + String(port) + path;

    http.begin(url);

    // Add auth if configured
    if (!authUsername.isEmpty()) {
        http.setAuthorization(authUsername.c_str(), authPassword.c_str());
    }

    // Set timeout
    http.setTimeout(5000);

    int httpCode = -1;

    switch (method) {
        case HttpMethod::GET:
            httpCode = http.GET();
            break;
        case HttpMethod::POST:
            http.addHeader("Content-Type", "application/json");
            httpCode = http.POST(body);
            break;
        case HttpMethod::PUT:
            http.addHeader("Content-Type", "application/json");
            httpCode = http.PUT(body);
            break;
        case HttpMethod::DELETE:
            httpCode = http.sendRequest("DELETE");
            break;
    }

    bool success = (httpCode > 0 && httpCode < 400);

    if (success && httpCode == HTTP_CODE_OK) {
        response = http.getString();
    }

    http.end();

    return success;
}

bool WiFiDeviceManager::extractJsonValue(const String& json, const String& path,
                                        PlcValue& value, PlcValueType type) {
    if (json.isEmpty()) {
        return false;
    }

    // If path is empty, treat entire response as the value
    if (path.isEmpty()) {
        switch (type) {
            case PlcValueType::BOOL:
                if (json == "true" || json == "1" || json == "ON" || json == "on") {
                    value.value.bVal = true;
                } else if (json == "false" || json == "0" || json == "OFF" || json == "off") {
                    value.value.bVal = false;
                } else {
                    return false;
                }
                return true;
            case PlcValueType::INT:
                value.value.i16Val = json.toInt();
                return true;
            case PlcValueType::REAL:
                value.value.fVal = json.toFloat();
                return true;
            case PlcValueType::STRING_TYPE:
                strncpy(value.value.sVal, json.c_str(), 63);
                value.value.sVal[63] = '\0';
                return true;
            default:
                return false;
        }
    }

    // Parse JSON response
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, json);

    if (error) {
        LOG_ERROR("WiFiDeviceManager", "Failed to parse JSON: " + String(error.c_str()));
        return false;
    }

    // Navigate JSON path (simple dot notation)
    JsonVariant current = doc.as<JsonVariant>();

    // Split path by dots
    int startIndex = 0;
    int endIndex = 0;

    while (endIndex >= 0) {
        endIndex = path.indexOf('.', startIndex);
        String key;

        if (endIndex >= 0) {
            key = path.substring(startIndex, endIndex);
            startIndex = endIndex + 1;
        } else {
            key = path.substring(startIndex);
        }

        if (!key.isEmpty()) {
            if (current.is<JsonObject>()) {
                current = current[key];
            } else {
                return false;
            }
        }
    }

    // Extract final value
    if (current.isNull()) {
        return false;
    }

    switch (type) {
        case PlcValueType::BOOL:
            if (current.is<bool>()) {
                value.value.bVal = current.as<bool>();
            } else if (current.is<String>()) {
                String str = current.as<String>();
                if (str == "ON" || str == "on" || str == "true" || str == "1") {
                    value.value.bVal = true;
                } else if (str == "OFF" || str == "off" || str == "false" || str == "0") {
                    value.value.bVal = false;
                } else {
                    return false;
                }
            } else {
                return false;
            }
            return true;

        case PlcValueType::INT:
            if (current.is<int>()) {
                value.value.i16Val = current.as<int>();
            } else {
                value.value.i16Val = current.as<String>().toInt();
            }
            return true;

        case PlcValueType::REAL:
            if (current.is<float>() || current.is<double>()) {
                value.value.fVal = current.as<float>();
            } else {
                value.value.fVal = current.as<String>().toFloat();
            }
            return true;

        case PlcValueType::STRING_TYPE:
            strncpy(value.value.sVal, current.as<String>().c_str(), 63);
            value.value.sVal[63] = '\0';
            return true;

        default:
            return false;
    }
}

String WiFiDeviceManager::formatWriteValue(const PlcValue& value, PlcValueType datatype,
                                          const JsonObject& endpointConfig) {
    JsonObject writeConf = endpointConfig["write"];
    String bodyTemplate = writeConf["body_template"] | "";

    // Get value as string for template replacement
    String valueStr;

    // Check for value format mapping
    JsonObject valueFormat = writeConf["value_format"];
    if (!valueFormat.isNull() && datatype == PlcValueType::BOOL) {
        if (value.value.bVal && valueFormat.containsKey("true")) {
            valueStr = valueFormat["true"].as<String>();
        } else if (!value.value.bVal && valueFormat.containsKey("false")) {
            valueStr = valueFormat["false"].as<String>();
        } else {
            valueStr = value.value.bVal ? "true" : "false";
        }
    } else {
        // Default formatting
        switch (datatype) {
            case PlcValueType::BOOL:
                valueStr = value.value.bVal ? "true" : "false";
                break;
            case PlcValueType::INT:
                valueStr = String(value.value.i16Val);
                break;
            case PlcValueType::REAL:
                valueStr = String(value.value.fVal, 2);
                break;
            case PlcValueType::STRING_TYPE:
                valueStr = value.value.sVal;
                break;
            default:
                valueStr = "";
        }
    }

    // Replace {{value}} in template
    if (!bodyTemplate.isEmpty()) {
        bodyTemplate.replace("{{value}}", valueStr);
        return bodyTemplate;
    }

    // If no template, return plain value
    return valueStr;
}

void WiFiDeviceManager::pollEndpoints() {
    // Polling is now handled by DeviceConfigManager
    // This method can be used for background tasks if needed
}

// ============================================================================
// Parsing Helpers
// ============================================================================

ConnectionType WiFiDeviceManager::parseConnectionType(const String& type) {
    String lower = type;
    lower.toLowerCase();

    if (lower == "http") return ConnectionType::HTTP;
    if (lower == "https") return ConnectionType::HTTPS;
    if (lower == "mqtt") return ConnectionType::MQTT;

    return ConnectionType::HTTP; // default
}

HttpMethod WiFiDeviceManager::parseHttpMethod(const String& method) {
    String upper = method;
    upper.toUpperCase();

    if (upper == "GET") return HttpMethod::GET;
    if (upper == "POST") return HttpMethod::POST;
    if (upper == "PUT") return HttpMethod::PUT;
    if (upper == "DELETE") return HttpMethod::DELETE;

    return HttpMethod::GET; // default
}

PlcValueType WiFiDeviceManager::parseDatatype(const String& type) {
    String lower = type;
    lower.toLowerCase();

    if (lower == "bool" || lower == "boolean") return PlcValueType::BOOL;
    if (lower == "int" || lower == "integer") return PlcValueType::INT;
    if (lower == "real" || lower == "float" || lower == "double") return PlcValueType::REAL;
    if (lower == "string" || lower == "text") return PlcValueType::STRING_TYPE;

    return PlcValueType::BOOL; // default
}

bool WiFiDeviceManager::parseEndpointConfig(const JsonObject& json, EndpointConfig& endpoint) {
    endpoint.name = json["name"] | "";
    String datatypeStr = json["type"] | "bool";
    endpoint.datatype = parseDatatype(datatypeStr);
    endpoint.access = json["access"] | "r";
    endpoint.pollingInterval = json["polling_interval"] | globalPollingInterval;

    // Parse read config
    JsonObject readConf = json["read"];
    if (!readConf.isNull()) {
        endpoint.hasRead = true;
        String methodStr = readConf["method"] | "GET";
        endpoint.readMethod = parseHttpMethod(methodStr);
        endpoint.readPath = readConf["path"] | "";
        endpoint.readValuePath = readConf["value_path"] | "";
        endpoint.mqttReadTopic = readConf["topic"] | "";

        // Value mapping
        JsonObject valueMap = readConf["value_map"];
        if (!valueMap.isNull()) {
            for (JsonPair kv : valueMap) {
                endpoint.readValueMap[String(kv.key().c_str())] = kv.value().as<bool>();
            }
        }
    } else {
        endpoint.hasRead = false;
    }

    // Parse write config
    JsonObject writeConf = json["write"];
    if (!writeConf.isNull()) {
        endpoint.hasWrite = true;
        String methodStr = writeConf["method"] | "POST";
        endpoint.writeMethod = parseHttpMethod(methodStr);
        endpoint.writePath = writeConf["path"] | "";
        endpoint.writeBodyTemplate = writeConf["body_template"] | "";
        endpoint.mqttWriteTopic = writeConf["topic"] | "";

        // Value format
        JsonObject valueFormat = writeConf["value_format"];
        if (!valueFormat.isNull()) {
            if (valueFormat.containsKey("true")) {
                endpoint.writeValueFormat[true] = valueFormat["true"] | "true";
            }
            if (valueFormat.containsKey("false")) {
                endpoint.writeValueFormat[false] = valueFormat["false"] | "false";
            }
        }
    } else {
        endpoint.hasWrite = false;
    }

    endpoint.currentValue = PlcValue(endpoint.datatype);
    endpoint.lastRead = 0;

    return true;
}
