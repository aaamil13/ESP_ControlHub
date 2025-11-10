#include "WebManager.h"
#include <LittleFS.h>
#include "StreamLogger.h"

// Define LITTLEFS as an alias for LittleFS if not already defined
#ifndef LITTLEFS
#define LITTLEFS LittleFS
#endif

extern StreamLogger* EspHubLog;

WebManager* WebManager::instance = nullptr;

WebManager::WebManager(PlcEngine* plcEngine, MeshDeviceManager* meshDeviceManager) : server(80), ws("/ws"), _plcEngine(plcEngine), _meshDeviceManager(meshDeviceManager) {
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

    server.serveStatic("/", LITTLEFS, "/");

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
            }
        }
    }
}