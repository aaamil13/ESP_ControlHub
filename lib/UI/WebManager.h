#ifndef WEB_MANAGER_H
#define WEB_MANAGER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <functional>

#ifndef UNIT_TEST
#include <ESPAsyncWebServer.h>
#include <AsyncWebSocket.h>
#else
// Dummy classes for Unit Testing
#include <FS.h>
#include <LittleFS.h>

#define HTTP_GET 0
#define HTTP_POST 1
#define HTTP_ANY 2
#define WS_TEXT 1

struct AwsFrameInfo {
    bool final;
    uint8_t index;
    uint64_t len;
    uint8_t opcode;
};

class AsyncWebParameter {
public:
    String value() { return ""; }
};

class AsyncWebServerRequest {
public:
    void send(int code, const char* contentType, const String& content) {}
    template <typename T>
    void send(T& fs, const char* path, const char* contentType, bool download = false) {}
    String url() { return ""; }
    bool hasParam(const String& name, bool post = false, bool file = false) { return false; }
    AsyncWebParameter* getParam(const String& name, bool post = false, bool file = false) { return new AsyncWebParameter(); }
};

class AsyncWebSocketClient {
public:
    void text(const String& message) {}
    uint32_t id() { return 0; }
    IPAddress remoteIP() { return IPAddress(0,0,0,0); }
};

enum AwsEventType { WS_EVT_CONNECT, WS_EVT_DISCONNECT, WS_EVT_PONG, WS_EVT_ERROR, WS_EVT_DATA };

class AsyncWebSocket {
public:
    AsyncWebSocket(const char* url) {}
    template <typename T>
    void onEvent(T callback) {}
    void cleanupClients() {}
    void textAll(const char* message) {}
    void textAll(const String& message) {}
};

// Forward declarations
class PlcEngine;
class MeshDeviceManager;
class ZigbeeManager;
class ModuleManager;
class WebManager; // Forward declaration for static instance

class AsyncWebServer {
public:
    AsyncWebServer(int port) {}
    void on(const char* uri, int method, std::function<void(AsyncWebServerRequest *request)> callback) {}
};

class WebManager : public AsyncWebServer {
public:
    WebManager(PlcEngine* plcEngine, MeshDeviceManager* meshDeviceManager, ZigbeeManager* zigbeeManager);
    AsyncWebServer& getServer() { return *this; }
    void setZigbeeManager(ZigbeeManager* manager) { _zigbeeManager = manager; }
    void setModuleManager(ModuleManager* manager) { _moduleManager = manager; }
    void begin();
    void log(const String& message);

    static WebManager* instance;

private:
    AsyncWebSocket ws;
    PlcEngine* _plcEngine;
    MeshDeviceManager* _meshDeviceManager;
    ZigbeeManager* _zigbeeManager;
    ModuleManager* _moduleManager;

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

#endif // UNIT_TEST

#endif // WEB_MANAGER_H