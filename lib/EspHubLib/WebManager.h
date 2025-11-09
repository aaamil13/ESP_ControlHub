#ifndef WEB_MANAGER_H
#define WEB_MANAGER_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <AsyncWebSocket.h>
#include <AsyncElegantOTA.h>
#include "LITTLEFS.h"
#include "../PlcCore/PlcEngine.h" // For PLC config upload
#include "MeshDeviceManager.h" // For mesh device monitoring

class WebManager {
public:
    WebManager(PlcEngine* plcEngine, MeshDeviceManager* meshDeviceManager);
    void begin();
    void log(const String& message);
    AsyncWebServer& getServer() { return server; } // Expose server instance

private:
    AsyncWebServer server;
    AsyncWebSocket ws;
    PlcEngine* _plcEngine; // Pointer to PlcEngine for config upload
    MeshDeviceManager* _meshDeviceManager; // Pointer to MeshDeviceManager for monitoring
    
    static void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
};

#endif // WEB_MANAGER_H