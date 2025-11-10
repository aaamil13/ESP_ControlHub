#include "WiFiDeviceManager.h"
#include "StreamLogger.h"
#include "DeviceRegistry.h"
#include "MqttManager.h"
#include <LittleFS.h>
#include <HTTPClient.h>
#include <base64.h>

extern StreamLogger* EspHubLog;

WiFiDeviceManager::WiFiDeviceManager(MqttManager* mqttManager)
    : DeviceManager("wifi", ProtocolType::WIFI),
      mqttManager(mqttManager),
      globalPollingInterval(5000) {
}

WiFiDeviceManager::~WiFiDeviceManager() {
}

void WiFiDeviceManager::begin() {
    EspHubLog->println("WiFiDeviceManager: Initializing...");

    // Load device configurations from LittleFS
    File devicesFile = LittleFS.open("/config/devices.json", "r");
    if (devicesFile) {
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, devicesFile);
        devicesFile.close();

        if (!error) {
            JsonArray deviceList = doc["devices"].as<JsonArray>();
            for (JsonObject deviceConfig : deviceList) {
                String protocol = deviceConfig["protocol"] | "";
                if (protocol == "wifi") {
                    loadDeviceFromJson(deviceConfig);
                }
            }
            EspHubLog->printf("Loaded %d WiFi devices from config\n", devices.size());
        } else {
            EspHubLog->printf("ERROR: Failed to parse devices.json: %s\n", error.c_str());
        }
    } else {
        EspHubLog->println("No devices.json found, starting with empty config");
    }

    EspHubLog->println("WiFiDeviceManager initialized");
}

void WiFiDeviceManager::loop() {
    // Poll endpoints
    pollEndpoints();

    // Check offline devices
    checkOfflineDevices(60000); // 60 second timeout
}

bool WiFiDeviceManager::loadDeviceFromJson(const JsonObject& config) {
    WiFiDeviceConfig device;

    // Basic info
    device.deviceId = config["device_id"] | "";
    device.friendlyName = config["friendly_name"] | device.deviceId;
    device.location = config["location"] | "";

    if (device.deviceId.isEmpty() || device.location.isEmpty()) {
        EspHubLog->println("ERROR: device_id and location are required");
        return false;
    }

    // Connection
    JsonObject conn = config["connection"];
    String connType = conn["type"] | "http";
    device.connectionType = parseConnectionType(connType);
    device.host = conn["host"] | "";
    device.port = conn["port"] | 80;
    device.useSsl = conn["use_ssl"] | false;

    // Auth
    JsonObject auth = conn["auth"];
    device.authUsername = auth["username"] | "";
    device.authPassword = auth["password"] | "";

    // MQTT
    device.mqttTopicPrefix = conn["topic_prefix"] | "";

    // Endpoints
    JsonArray endpoints = config["endpoints"];
    for (JsonObject ep : endpoints) {
        EndpointConfig epConfig;
        epConfig.name = ep["name"] | "";
        String datatypeStr = ep["type"] | "bool";
        epConfig.datatype = parseDatatype(datatypeStr);
        epConfig.access = ep["access"] | "r";
        epConfig.pollingInterval = ep["polling_interval"] | globalPollingInterval;

        // Read config
        JsonObject readConf = ep["read"];
        if (!readConf.isNull()) {
            epConfig.hasRead = true;
            String methodStr = readConf["method"] | "GET";
            epConfig.readMethod = parseHttpMethod(methodStr);
            epConfig.readPath = readConf["path"] | "";
            epConfig.readValuePath = readConf["value_path"] | "";

            // Value mapping
            JsonObject valueMap = readConf["value_map"];
            if (!valueMap.isNull()) {
                for (JsonPair kv : valueMap) {
                    epConfig.readValueMap[String(kv.key().c_str())] = kv.value().as<bool>();
                }
            }

            // MQTT topic
            epConfig.mqttReadTopic = readConf["topic"] | "";
        } else {
            epConfig.hasRead = false;
        }

        // Write config
        JsonObject writeConf = ep["write"];
        if (!writeConf.isNull()) {
            epConfig.hasWrite = true;
            String methodStr = writeConf["method"] | "POST";
            epConfig.writeMethod = parseHttpMethod(methodStr);
            epConfig.writePath = writeConf["path"] | "";
            epConfig.writeBodyTemplate = writeConf["body_template"] | "";

            // Value format
            JsonObject valueFormat = writeConf["value_format"];
            if (!valueFormat.isNull()) {
                if (valueFormat.containsKey("true")) {
                    epConfig.writeValueFormat[true] = valueFormat["true"] | "true";
                }
                if (valueFormat.containsKey("false")) {
                    epConfig.writeValueFormat[false] = valueFormat["false"] | "false";
                }
            }

            // MQTT topic
            epConfig.mqttWriteTopic = writeConf["topic"] | "";
        } else {
            epConfig.hasWrite = false;
        }

        epConfig.currentValue = PlcValue(epConfig.datatype);
        epConfig.lastRead = 0;

        device.endpoints.push_back(epConfig);
    }

    // Metadata
    JsonObject meta = config["metadata"];
    device.manufacturer = meta["manufacturer"] | "";
    device.model = meta["model"] | "";
    device.firmware = meta["firmware"] | "";

    device.isOnline = false;
    device.lastSeen = 0;

    // Build full device ID
    String fullDeviceId = buildDeviceId(device.location, device.deviceId);

    // Register device in registry
    JsonDocument deviceDoc;
    deviceDoc["friendly_name"] = device.friendlyName;
    deviceDoc["manufacturer"] = device.manufacturer;
    deviceDoc["model"] = device.model;

    bool registered = registerDevice(fullDeviceId, deviceDoc.as<JsonObject>());
    if (!registered) {
        EspHubLog->printf("ERROR: Failed to register WiFi device %s\n", fullDeviceId.c_str());
        return false;
    }

    // Register endpoints
    for (const auto& ep : device.endpoints) {
        registerEndpointInRegistry(fullDeviceId, ep);
    }

    // Store device
    devices[fullDeviceId] = device;

    EspHubLog->printf("Loaded WiFi device: %s (%s) with %d endpoints\n",
                     fullDeviceId.c_str(), device.host.c_str(), device.endpoints.size());

    return true;
}

bool WiFiDeviceManager::loadDeviceFromFile(const String& filepath) {
    File file = LittleFS.open(filepath, "r");
    if (!file) {
        EspHubLog->printf("ERROR: Failed to open %s\n", filepath.c_str());
        return false;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) {
        EspHubLog->printf("ERROR: Failed to parse %s: %s\n", filepath.c_str(), error.c_str());
        return false;
    }

    return loadDeviceFromJson(doc.as<JsonObject>());
}

bool WiFiDeviceManager::saveDeviceToFile(const String& deviceId, const String& filepath) {
    auto it = devices.find(deviceId);
    if (it == devices.end()) {
        EspHubLog->printf("ERROR: Device %s not found\n", deviceId.c_str());
        return false;
    }

    // TODO: Serialize device config back to JSON and save
    EspHubLog->println("WARNING: saveDeviceToFile not yet implemented");
    return false;
}

bool WiFiDeviceManager::removeDevice(const String& deviceId) {
    auto it = devices.find(deviceId);
    if (it == devices.end()) {
        return false;
    }

    // Remove from registry
    unregisterDevice(deviceId);

    // Remove from map
    devices.erase(it);

    EspHubLog->printf("Removed WiFi device: %s\n", deviceId.c_str());
    return true;
}

WiFiDeviceConfig* WiFiDeviceManager::getDevice(const String& deviceId) {
    auto it = devices.find(deviceId);
    if (it != devices.end()) {
        return &it->second;
    }
    return nullptr;
}

std::vector<String> WiFiDeviceManager::getAllDeviceIds() const {
    std::vector<String> ids;
    for (const auto& pair : devices) {
        ids.push_back(pair.first);
    }
    return ids;
}

bool WiFiDeviceManager::readEndpoint(const String& deviceId, const String& endpointName) {
    WiFiDeviceConfig* device = getDevice(deviceId);
    if (!device) {
        return false;
    }

    // Find endpoint
    EndpointConfig* endpoint = nullptr;
    for (auto& ep : device->endpoints) {
        if (ep.name == endpointName) {
            endpoint = &ep;
            break;
        }
    }

    if (!endpoint || !endpoint->hasRead) {
        return false;
    }

    String response;
    bool success = false;

    if (device->connectionType == ConnectionType::HTTP || device->connectionType == ConnectionType::HTTPS) {
        success = httpRequest(*device, endpoint->readMethod, endpoint->readPath, "", response);
    } else if (device->connectionType == ConnectionType::MQTT) {
        // MQTT reading happens via subscription callback
        // Just mark as success if topic is configured
        success = !endpoint->mqttReadTopic.isEmpty();
    }

    if (success && !response.isEmpty()) {
        PlcValue value(endpoint->datatype);
        if (extractJsonValue(response, endpoint->readValuePath, value, endpoint->datatype)) {
            updateEndpointValue(deviceId, endpointName, value);
            endpoint->currentValue = value;
            endpoint->lastRead = millis();
            device->isOnline = true;
            device->lastSeen = millis();
            updateDeviceStatus(deviceId, true);
            return true;
        }
    }

    return false;
}

bool WiFiDeviceManager::writeEndpoint(const String& deviceId, const String& endpointName, const PlcValue& value) {
    WiFiDeviceConfig* device = getDevice(deviceId);
    if (!device) {
        return false;
    }

    // Find endpoint
    EndpointConfig* endpoint = nullptr;
    for (auto& ep : device->endpoints) {
        if (ep.name == endpointName) {
            endpoint = &ep;
            break;
        }
    }

    if (!endpoint || !endpoint->hasWrite) {
        return false;
    }

    String body = formatWriteValue(value, *endpoint);
    String response;
    bool success = false;

    if (device->connectionType == ConnectionType::HTTP || device->connectionType == ConnectionType::HTTPS) {
        success = httpRequest(*device, endpoint->writeMethod, endpoint->writePath, body, response);
    } else if (device->connectionType == ConnectionType::MQTT) {
        if (mqttManager && !endpoint->mqttWriteTopic.isEmpty()) {
            mqttManager->publish(endpoint->mqttWriteTopic.c_str(), body.c_str());
            success = true;
        }
    }

    if (success) {
        endpoint->currentValue = value;
        updateEndpointValue(deviceId, endpointName, value);
        EspHubLog->printf("WiFi device %s endpoint %s written: %s\n",
                         deviceId.c_str(), endpointName.c_str(), body.c_str());
    }

    return success;
}

bool WiFiDeviceManager::testDeviceConnection(const WiFiDeviceConfig& config) {
    // Try to connect to device
    String response;
    bool success = false;

    if (config.connectionType == ConnectionType::HTTP || config.connectionType == ConnectionType::HTTPS) {
        // Try a simple GET request
        HTTPClient http;
        String url = String(config.useSsl ? "https://" : "http://") + config.host + ":" + String(config.port) + "/";

        http.begin(url);
        int httpCode = http.GET();
        success = (httpCode > 0);
        http.end();
    }

    return success;
}

bool WiFiDeviceManager::testEndpoint(const WiFiDeviceConfig& config, const String& endpointName) {
    // Find endpoint
    for (const auto& ep : config.endpoints) {
        if (ep.name == endpointName && ep.hasRead) {
            String response;
            return httpRequest(config, ep.readMethod, ep.readPath, "", response);
        }
    }
    return false;
}

// Private methods

bool WiFiDeviceManager::httpRequest(const WiFiDeviceConfig& device, HttpMethod method,
                                    const String& path, const String& body, String& response) {
    HTTPClient http;

    // Build URL
    String url = String(device.useSsl ? "https://" : "http://") +
                 device.host + ":" + String(device.port) + path;

    http.begin(url);

    // Add auth if configured
    if (!device.authUsername.isEmpty()) {
        http.setAuthorization(device.authUsername.c_str(), device.authPassword.c_str());
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

    bool success = false;
    if (httpCode > 0) {
        if (httpCode == 200 || httpCode == 201 || httpCode == 204) {
            response = http.getString();
            success = true;
        } else {
            EspHubLog->printf("HTTP request failed with code %d\n", httpCode);
        }
    } else {
        EspHubLog->printf("HTTP request failed: %s\n", http.errorToString(httpCode).c_str());
    }

    http.end();
    return success;
}

bool WiFiDeviceManager::extractJsonValue(const String& json, const String& path, PlcValue& value, PlcValueType type) {
    if (json.isEmpty()) {
        return false;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, json);
    if (error) {
        EspHubLog->printf("JSON parse error: %s\n", error.c_str());
        return false;
    }

    // Navigate JSON path (e.g., "data.temperature")
    JsonVariant current = doc.as<JsonVariant>();

    if (!path.isEmpty()) {
        int start = 0;
        int end = path.indexOf('.');

        while (current && start < path.length()) {
            String key = (end > 0) ? path.substring(start, end) : path.substring(start);

            if (current.is<JsonObject>()) {
                current = current[key];
            } else if (current.is<JsonArray>() && key.toInt() >= 0) {
                current = current[key.toInt()];
            } else {
                return false;
            }

            start = end + 1;
            end = path.indexOf('.', start);
        }
    }

    // Extract value based on type
    if (current.isNull()) {
        return false;
    }

    switch (type) {
        case PlcValueType::BOOL:
            if (current.is<bool>()) {
                value.value.bVal = current.as<bool>();
                return true;
            } else if (current.is<const char*>()) {
                String strVal = current.as<String>();
                strVal.toLowerCase();
                value.value.bVal = (strVal == "on" || strVal == "true" || strVal == "1");
                return true;
            }
            break;

        case PlcValueType::INT:
            if (current.is<int>()) {
                value.value.i16Val = current.as<int16_t>();
                return true;
            }
            break;

        case PlcValueType::REAL:
            if (current.is<float>() || current.is<double>()) {
                value.value.fVal = current.as<float>();
                return true;
            }
            break;

        case PlcValueType::STRING_TYPE:
            if (current.is<const char*>()) {
                strncpy(value.value.sVal, current.as<const char*>(), 63);
                value.value.sVal[63] = '\0';
                return true;
            }
            break;

        default:
            break;
    }

    return false;
}

String WiFiDeviceManager::formatWriteValue(const PlcValue& value, const EndpointConfig& endpoint) {
    String formatted;

    if (!endpoint.writeBodyTemplate.isEmpty()) {
        // Use template
        formatted = endpoint.writeBodyTemplate;

        // Replace {{value}} placeholder
        String valueStr;
        switch (endpoint.datatype) {
            case PlcValueType::BOOL:
                if (endpoint.writeValueFormat.find(value.value.bVal) != endpoint.writeValueFormat.end()) {
                    valueStr = endpoint.writeValueFormat.at(value.value.bVal);
                } else {
                    valueStr = value.value.bVal ? "true" : "false";
                }
                break;
            case PlcValueType::INT:
                valueStr = String(value.value.i16Val);
                break;
            case PlcValueType::REAL:
                valueStr = String(value.value.fVal, 2);
                break;
            case PlcValueType::STRING_TYPE:
                valueStr = String(value.value.sVal);
                break;
            default:
                valueStr = "null";
        }

        formatted.replace("{{value}}", valueStr);
    } else {
        // Simple value
        switch (endpoint.datatype) {
            case PlcValueType::BOOL:
                formatted = value.value.bVal ? "true" : "false";
                break;
            case PlcValueType::INT:
                formatted = String(value.value.i16Val);
                break;
            case PlcValueType::REAL:
                formatted = String(value.value.fVal, 2);
                break;
            case PlcValueType::STRING_TYPE:
                formatted = String(value.value.sVal);
                break;
            default:
                formatted = "null";
        }
    }

    return formatted;
}

void WiFiDeviceManager::pollEndpoints() {
    unsigned long now = millis();

    for (auto& devicePair : devices) {
        WiFiDeviceConfig& device = devicePair.second;

        for (auto& endpoint : device.endpoints) {
            if (endpoint.hasRead && endpoint.access.indexOf('r') >= 0) {
                // Check if it's time to poll
                if (now - endpoint.lastRead >= endpoint.pollingInterval) {
                    readEndpoint(devicePair.first, endpoint.name);
                }
            }
        }
    }
}

void WiFiDeviceManager::registerEndpointInRegistry(const String& deviceId, const EndpointConfig& endpointConfig) {
    Endpoint endpoint;
    endpoint.fullName = deviceId + "." + endpointConfig.name + "." +
                       String(endpointConfig.datatype == PlcValueType::BOOL ? "bool" :
                              endpointConfig.datatype == PlcValueType::INT ? "int" :
                              endpointConfig.datatype == PlcValueType::REAL ? "real" : "string");

    // Extract location from deviceId (format: location.wifi.device_id)
    int firstDot = deviceId.indexOf('.');
    endpoint.location = deviceId.substring(0, firstDot);

    endpoint.protocol = ProtocolType::WIFI;
    endpoint.deviceId = deviceId;
    endpoint.endpoint = endpointConfig.name;
    endpoint.datatype = endpointConfig.datatype;
    endpoint.isOnline = true;
    endpoint.lastSeen = millis();
    endpoint.isWritable = endpointConfig.hasWrite && (endpointConfig.access.indexOf('w') >= 0);
    endpoint.currentValue = endpointConfig.currentValue;

    bool registered = registerEndpointHelper(endpoint);
    if (registered) {
        EspHubLog->printf("  Registered endpoint: %s (%s%s)\n",
                         endpoint.fullName.c_str(),
                         endpoint.isWritable ? "RW" : "RO",
                         endpoint.isOnline ? " ONLINE" : " OFFLINE");
    }
}

void WiFiDeviceManager::updateEndpointValue(const String& deviceId, const String& endpointName, const PlcValue& value) {
    String endpointFullName = deviceId + "." + endpointName + "." +
                             String(value.type == PlcValueType::BOOL ? "bool" :
                                    value.type == PlcValueType::INT ? "int" :
                                    value.type == PlcValueType::REAL ? "real" : "string");

    registry->updateEndpointValue(endpointFullName, value);
}

ConnectionType WiFiDeviceManager::parseConnectionType(const String& type) {
    if (type == "https") return ConnectionType::HTTPS;
    if (type == "mqtt") return ConnectionType::MQTT;
    return ConnectionType::HTTP;
}

HttpMethod WiFiDeviceManager::parseHttpMethod(const String& method) {
    String m = method;
    m.toUpperCase();
    if (m == "POST") return HttpMethod::POST;
    if (m == "PUT") return HttpMethod::PUT;
    if (m == "DELETE") return HttpMethod::DELETE;
    return HttpMethod::GET;
}

PlcValueType WiFiDeviceManager::parseDatatype(const String& type) {
    if (type == "bool") return PlcValueType::BOOL;
    if (type == "int") return PlcValueType::INT;
    if (type == "real") return PlcValueType::REAL;
    if (type == "string") return PlcValueType::STRING_TYPE;
    return PlcValueType::BOOL;
}
