#ifndef WEB_MANAGER_H
#define WEB_MANAGER_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <AsyncWebSocket.h>
#include "../PlcEngine/Engine/PlcEngine.h" // For PLC config upload
#include "../Protocols/Mesh/MeshDeviceManager.h" // For mesh device monitoring
#include "../Protocols/Zigbee/ZigbeeManager.h" // For Zigbee device monitoring
#include "../Core/ModuleManager.h" // For module management

class WebManager {
public:
    WebManager(PlcEngine* plcEngine, MeshDeviceManager* meshDeviceManager, ZigbeeManager* zigbeeManager = nullptr);
    void begin();
    void log(const String& message);
    AsyncWebServer& getServer() { return server; } // Expose server instance
    void setZigbeeManager(ZigbeeManager* manager) { _zigbeeManager = manager; }
    void setModuleManager(ModuleManager* manager) { _moduleManager = manager; }

private:
    AsyncWebServer server;
    AsyncWebSocket ws;
    PlcEngine* _plcEngine; // Pointer to PlcEngine for config upload
    MeshDeviceManager* _meshDeviceManager; // Pointer to MeshDeviceManager for monitoring
    ZigbeeManager* _zigbeeManager; // Pointer to ZigbeeManager for Zigbee device monitoring
    ModuleManager* _moduleManager; // Pointer to ModuleManager for module control
    static WebManager* instance; // Static instance for callbacks

    static void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
    void handleZigbeeRequest(const String& requestType, const JsonObject& data, AsyncWebSocketClient* client);

    // Module management API handlers
    void setupModuleAPI();
    void handleGetModules(AsyncWebServerRequest *request);
    void handleGetModule(AsyncWebServerRequest *request);
    void handleEnableModule(AsyncWebServerRequest *request);
    void handleDisableModule(AsyncWebServerRequest *request);
    void handleGetModuleStats(AsyncWebServerRequest *request);
};

#endif // WEB_MANAGER_H