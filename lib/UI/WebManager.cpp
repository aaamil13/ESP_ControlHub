#include "../UI/WebManager.h"
#include <LittleFS.h>
#include "../Core/StreamLogger.h"
#include "../Devices/DeviceRegistry.h"
#include <map>

// Define LITTLEFS as an alias for LittleFS if not already defined
#ifndef LITTLEFS
#define LITTLEFS LittleFS
#endif

extern StreamLogger* EspHubLog;

WebManager* WebManager::instance = nullptr;

WebManager::WebManager(PlcEngine* plcEngine, MeshDeviceManager* meshDeviceManager, ZigbeeManager* zigbeeManager)
    : server(80), ws("/ws"), _plcEngine(plcEngine), _meshDeviceManager(meshDeviceManager), _zigbeeManager(zigbeeManager), _moduleManager(nullptr) {
    instance = this;
}

void WebManager::begin() {
    if(!LITTLEFS.begin()){
        EspHubLog->println("An Error has occurred while mounting LittleFS");
        return;
    }

    ws.onEvent(onWsEvent);
    server.addHandler(&ws);

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LITTLEFS, "/index.html", "text/html");
    });

    server.on("/log", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LITTLEFS, "/log.html", "text/html");
    });

    // New route for PLC configuration upload
    server.on("/plc_config", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LITTLEFS, "/plc_config.html", "text/html");
    });

    // TODO: Implement file upload for PLC configuration
    // File upload functionality requires AsyncWebUpload support
    // server.on("/plc_config", HTTP_POST, ...);


    // New route for PLC monitoring
    server.on("/plc_monitor", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LITTLEFS, "/plc_monitor.html", "text/html");
    });

    server.on("/plc_command", HTTP_POST, [&](AsyncWebServerRequest *request){
        if (request->hasParam("command", false, true) && request->hasParam("program", false, true)) {
            String command = request->getParam("command", false, true)->value();
            String programName = request->getParam("program", false, true)->value();
            if (command == "run") {
                _plcEngine->runProgram(programName);
                request->send(200, "text/plain", "PLC Run command sent.");
            } else if (command == "stop") {
                _plcEngine->stopProgram(programName);
                request->send(200, "text/plain", "PLC Stop command sent.");
            } else if (command == "pause") {
                _plcEngine->pauseProgram(programName);
                request->send(200, "text/plain", "PLC Pause command sent.");
            } else {
                request->send(400, "text/plain", "Unknown PLC command.");
            }
        } else {
            request->send(400, "text/plain", "Missing command or program parameter.");
        }
    });

    // New route for mesh device registration
    server.on("/mesh_register", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LITTLEFS, "/mesh_register.html", "text/html");
    });

    server.on("/mesh_register", HTTP_POST, [&](AsyncWebServerRequest *request){
        if (_meshDeviceManager && request->hasParam("node_id") && request->hasParam("device_name")) {
            uint32_t nodeId = request->getParam("node_id")->value().toInt();
            String deviceName = request->getParam("device_name")->value();
            _meshDeviceManager->addDevice(nodeId, deviceName);
            request->send(200, "text/plain", "Device registered successfully.");
        } else {
            request->send(400, "text/plain", "Missing node_id or device_name.");
        }
    });

    // Zigbee device management page
    server.on("/zigbee", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LITTLEFS, "/zigbee.html", "text/html");
    });

    server.serveStatic("/", LITTLEFS, "/");

    // Setup Module Management API
    setupModuleAPI();

    // TODO: Add AsyncElegantOTA library for OTA updates
    // AsyncElegantOTA.begin(&server);    // Start ElegantOTA
    server.begin();
    EspHubLog->println("Web server started.");
}

void WebManager::log(const String& message) {
    ws.textAll(message);
}

void WebManager::onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    if(type == WS_EVT_CONNECT){
        EspHubLog->printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
        client->text("Welcome!");
    } else if(type == WS_EVT_DISCONNECT){
        EspHubLog->printf("WebSocket client #%u disconnected\n", client->id());
    } else if (type == WS_EVT_DATA) {
        AwsFrameInfo *info = (AwsFrameInfo*)arg;
        if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
            String message = (char*)data;
            StaticJsonDocument<200> doc;
            DeserializationError error = deserializeJson(doc, message);
            if (error) {
                EspHubLog->printf("deserializeJson() failed for WebSocket message: %s\n", error.c_str());
                return;
            }

            const char* request_type = doc["request"];
            if (strcmp(request_type, "plc_status") == 0) {
                StaticJsonDocument<100> response;
                response["type"] = "plc_status";
                // Get main program status
                PlcProgram* mainProgram = instance->_plcEngine->getProgram("main_program");
                if (mainProgram) {
                    response["state"] = (mainProgram->getState() == PlcProgramState::RUNNING) ? "RUNNING" : "STOPPED";
                } else {
                    response["state"] = "NO_PROGRAM";
                }
                String response_str;
                serializeJson(response, response_str);
                client->text(response_str);
            } else if (strcmp(request_type, "plc_variables") == 0) {
                StaticJsonDocument<1024> response; // Adjust size as needed
                response["type"] = "plc_variables";
                // Populate with PLC variables
                // For now, just send a placeholder
                response["variables"]["temp"] = 25.5;
                response["variables"]["light"] = true;
                String response_str;
                serializeJson(response, response_str);
                client->text(response_str);
            } else if (strcmp(request_type, "mesh_devices") == 0) {
                StaticJsonDocument<1024> response; // Adjust size as needed
                response["type"] = "mesh_devices";
                JsonArray devices_array = response.createNestedArray("devices");
                for (const auto& device : instance->_meshDeviceManager->getAllDevices()) {
                    JsonObject dev_obj = devices_array.createNestedObject();
                    dev_obj["nodeId"] = device.nodeId;
                    dev_obj["name"] = device.name;
                    dev_obj["lastSeen"] = device.lastSeen;
                    dev_obj["isOnline"] = device.isOnline;
                }
                String response_str;
                serializeJson(response, response_str);
                client->text(response_str);
            } else if (strcmp(request_type, "get_zigbee_devices") == 0) {
                if (instance->_zigbeeManager) {
                    instance->handleZigbeeRequest("get_zigbee_devices", doc.as<JsonObject>(), client);
                }
            } else if (strcmp(request_type, "refresh_zigbee_devices") == 0) {
                if (instance->_zigbeeManager) {
                    instance->handleZigbeeRequest("refresh_zigbee_devices", doc.as<JsonObject>(), client);
                }
            } else if (strcmp(request_type, "zigbee_start_pairing") == 0) {
                if (instance->_zigbeeManager) {
                    instance->handleZigbeeRequest("zigbee_start_pairing", doc.as<JsonObject>(), client);
                }
            } else if (strcmp(request_type, "zigbee_stop_pairing") == 0) {
                if (instance->_zigbeeManager) {
                    instance->handleZigbeeRequest("zigbee_stop_pairing", doc.as<JsonObject>(), client);
                }
            } else if (strcmp(request_type, "zigbee_control") == 0) {
                if (instance->_zigbeeManager) {
                    instance->handleZigbeeRequest("zigbee_control", doc.as<JsonObject>(), client);
                }
            }
        }
    }
}

void WebManager::handleZigbeeRequest(const String& requestType, const JsonObject& data, AsyncWebSocketClient* client) {
    if (requestType == "get_zigbee_devices") {
        // Get all Zigbee devices from DeviceRegistry
        DeviceRegistry& registry = DeviceRegistry::getInstance();
        auto zigbeeEndpoints = registry.getEndpointsByProtocol(ProtocolType::ZIGBEE);

        JsonDocument response;
        response["type"] = "zigbee_devices";
        response["bridge_online"] = _zigbeeManager->isBridgeOnline();
        response["pairing_enabled"] = _zigbeeManager->isPairingEnabled();

        JsonArray devicesArray = response.createNestedArray("devices");

        // Group endpoints by device
        std::map<String, JsonObject> deviceMap;

        for (auto* endpoint : zigbeeEndpoints) {
            String deviceId = endpoint->deviceId;

            // Create device object if not exists
            if (deviceMap.find(deviceId) == deviceMap.end()) {
                JsonObject deviceObj = devicesArray.createNestedObject();
                deviceObj["id"] = deviceId;
                deviceObj["name"] = deviceId.substring(deviceId.lastIndexOf('.') + 1);
                deviceObj["online"] = endpoint->isOnline;
                deviceObj["location"] = endpoint->location;
                JsonArray endpointsArray = deviceObj.createNestedArray("endpoints");
                deviceMap[deviceId] = deviceObj;
            }

            // Add endpoint to device
            JsonObject deviceObj = deviceMap[deviceId];
            JsonArray endpointsArray = deviceObj["endpoints"];
            JsonObject endpointObj = endpointsArray.createNestedObject();
            endpointObj["name"] = endpoint->endpoint;
            endpointObj["datatype"] = (endpoint->datatype == PlcValueType::BOOL ? "bool" :
                                       endpoint->datatype == PlcValueType::INT ? "int" :
                                       endpoint->datatype == PlcValueType::REAL ? "real" : "string");
            endpointObj["writable"] = endpoint->isWritable;

            // Add current value
            switch (endpoint->datatype) {
                case PlcValueType::BOOL:
                    endpointObj["value"] = endpoint->currentValue.value.bVal;
                    break;
                case PlcValueType::INT:
                    endpointObj["value"] = endpoint->currentValue.value.i16Val;
                    break;
                case PlcValueType::REAL:
                    endpointObj["value"] = endpoint->currentValue.value.fVal;
                    break;
                case PlcValueType::STRING_TYPE:
                    endpointObj["value"] = String(endpoint->currentValue.value.sVal);
                    break;
                default:
                    endpointObj["value"] = nullptr;
            }
        }

        String responseStr;
        serializeJson(response, responseStr);
        client->text(responseStr);

    } else if (requestType == "refresh_zigbee_devices") {
        _zigbeeManager->refreshDeviceList();

        JsonDocument response;
        response["type"] = "zigbee_refresh";
        response["status"] = "requested";

        String responseStr;
        serializeJson(response, responseStr);
        client->text(responseStr);

    } else if (requestType == "zigbee_start_pairing") {
        uint32_t duration = data["duration"] | 60;
        _zigbeeManager->enablePairing(duration);

        JsonDocument response;
        response["type"] = "zigbee_pairing";
        response["enabled"] = true;
        response["duration"] = duration;

        String responseStr;
        serializeJson(response, responseStr);
        client->text(responseStr);

    } else if (requestType == "zigbee_stop_pairing") {
        _zigbeeManager->disablePairing();

        JsonDocument response;
        response["type"] = "zigbee_pairing";
        response["enabled"] = false;

        String responseStr;
        serializeJson(response, responseStr);
        client->text(responseStr);

    } else if (requestType == "zigbee_control") {
        const char* deviceId = data["device"];
        const char* endpoint = data["endpoint"];
        JsonVariant value = data["value"];

        if (deviceId && endpoint) {
            // Build full endpoint name
            DeviceRegistry& registry = DeviceRegistry::getInstance();
            auto zigbeeEndpoints = registry.getEndpointsByDevice(deviceId);

            for (auto* ep : zigbeeEndpoints) {
                if (ep->endpoint == String(endpoint) && ep->isWritable) {
                    // Update value in registry
                    PlcValue newValue(ep->datatype);

                    switch (ep->datatype) {
                        case PlcValueType::BOOL:
                            if (value.is<bool>()) {
                                newValue.value.bVal = value.as<bool>();
                            } else if (value.is<const char*>()) {
                                const char* val = value.as<const char*>();
                                newValue.value.bVal = (strcmp(val, "ON") == 0 || strcmp(val, "true") == 0);
                            }
                            break;
                        case PlcValueType::INT:
                            newValue.value.i16Val = value.as<int16_t>();
                            break;
                        case PlcValueType::REAL:
                            newValue.value.fVal = value.as<float>();
                            break;
                        case PlcValueType::STRING_TYPE:
                            strncpy(newValue.value.sVal, value.as<const char*>(), 63);
                            newValue.value.sVal[63] = '\0';
                            break;
                        default:
                            break;
                    }

                    registry.updateEndpointValue(ep->fullName, newValue);

                    // Publish to MQTT (will be handled by MQTT manager)
                    // The ZigbeeManager should have a method to publish control commands
                    // For now, just update the registry

                    JsonDocument response;
                    response["type"] = "zigbee_control_result";
                    response["success"] = true;
                    response["device"] = deviceId;
                    response["endpoint"] = endpoint;

                    String responseStr;
                    serializeJson(response, responseStr);
                    client->text(responseStr);
                    return;
                }
            }
        }

        // Send error response
        JsonDocument response;
        response["type"] = "zigbee_control_result";
        response["success"] = false;
        response["error"] = "Device or endpoint not found or not writable";

        String responseStr;
        serializeJson(response, responseStr);
        client->text(responseStr);
    }
}

// ============================================================================
// Module Management API
// ============================================================================

void WebManager::setupModuleAPI() {
    if (!_moduleManager) {
        return; // ModuleManager not set
    }

    // GET /api/modules - List all modules
    server.on("/api/modules", HTTP_GET, [this](AsyncWebServerRequest *request){
        this->handleGetModules(request);
    });

    // GET /api/modules/:name - Get module info
    server.on("^\\/api\\/modules\\/([a-zA-Z0-9_]+)$", HTTP_GET, [this](AsyncWebServerRequest *request){
        this->handleGetModule(request);
    });

    // POST /api/modules/:name/enable - Enable module
    server.on("^\\/api\\/modules\\/([a-zA-Z0-9_]+)\\/enable$", HTTP_POST, [this](AsyncWebServerRequest *request){
        this->handleEnableModule(request);
    });

    // POST /api/modules/:name/disable - Disable module
    server.on("^\\/api\\/modules\\/([a-zA-Z0-9_]+)\\/disable$", HTTP_POST, [this](AsyncWebServerRequest *request){
        this->handleDisableModule(request);
    });

    // GET /api/modules/:name/stats - Get module statistics
    server.on("^\\/api\\/modules\\/([a-zA-Z0-9_]+)\\/stats$", HTTP_GET, [this](AsyncWebServerRequest *request){
        this->handleGetModuleStats(request);
    });

    // Serve modules management page
    server.on("/modules", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LITTLEFS, "/modules.html", "text/html");
    });
}

void WebManager::handleGetModules(AsyncWebServerRequest *request) {
    if (!_moduleManager) {
        request->send(503, "application/json", "{\"error\":\"ModuleManager not available\"}");
        return;
    }

    JsonDocument doc = _moduleManager->getModuleSummary();
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void WebManager::handleGetModule(AsyncWebServerRequest *request) {
    if (!_moduleManager) {
        request->send(503, "application/json", "{\"error\":\"ModuleManager not available\"}");
        return;
    }

    // Extract module name from URL
    String path = request->url();
    int lastSlash = path.lastIndexOf('/');
    int secondLastSlash = path.lastIndexOf('/', lastSlash - 1);
    String moduleName = path.substring(secondLastSlash + 1, lastSlash);

    JsonDocument doc = _moduleManager->getModuleInfo(moduleName);
    if (doc.isNull()) {
        request->send(404, "application/json", "{\"error\":\"Module not found\"}");
        return;
    }

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void WebManager::handleEnableModule(AsyncWebServerRequest *request) {
    if (!_moduleManager) {
        request->send(503, "application/json", "{\"error\":\"ModuleManager not available\"}");
        return;
    }

    // Extract module name from URL
    String path = request->url();
    int lastSlash = path.lastIndexOf('/');
    int secondLastSlash = path.lastIndexOf('/', lastSlash - 1);
    String moduleName = path.substring(secondLastSlash + 1, lastSlash);

    bool success = _moduleManager->enableModule(moduleName);

    JsonDocument response;
    response["success"] = success;
    response["module"] = moduleName;
    response["state"] = success ? "enabled" : "error";
    if (!success) {
        response["error"] = _moduleManager->getModule(moduleName)->getLastError();
    }

    String responseStr;
    serializeJson(response, responseStr);
    request->send(success ? 200 : 400, "application/json", responseStr);
}

void WebManager::handleDisableModule(AsyncWebServerRequest *request) {
    if (!_moduleManager) {
        request->send(503, "application/json", "{\"error\":\"ModuleManager not available\"}");
        return;
    }

    // Extract module name from URL
    String path = request->url();
    int lastSlash = path.lastIndexOf('/');
    int secondLastSlash = path.lastIndexOf('/', lastSlash - 1);
    String moduleName = path.substring(secondLastSlash + 1, lastSlash);

    bool success = _moduleManager->disableModule(moduleName);

    JsonDocument response;
    response["success"] = success;
    response["module"] = moduleName;
    response["state"] = success ? "disabled" : "error";
    if (!success) {
        const Module* module = _moduleManager->getModule(moduleName);
        if (module) {
            response["error"] = module->getLastError();
        }
    }

    String responseStr;
    serializeJson(response, responseStr);
    request->send(success ? 200 : 400, "application/json", responseStr);
}

void WebManager::handleGetModuleStats(AsyncWebServerRequest *request) {
    if (!_moduleManager) {
        request->send(503, "application/json", "{\"error\":\"ModuleManager not available\"}");
        return;
    }

    // Extract module name from URL
    String path = request->url();
    int lastSlash = path.lastIndexOf('/');
    int secondLastSlash = path.lastIndexOf('/', lastSlash - 1);
    String moduleName = path.substring(secondLastSlash + 1, lastSlash);

    JsonDocument doc = _moduleManager->getModuleStatistics(moduleName);
    if (doc.isNull()) {
        request->send(404, "application/json", "{\"error\":\"Module not found\"}");
        return;
    }

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}