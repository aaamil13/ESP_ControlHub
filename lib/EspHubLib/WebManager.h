#ifndef WEB_MANAGER_H
#define WEB_MANAGER_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <AsyncWebSocket.h>
#include <AsyncElegantOTA.h>
#include "LITTLEFS.h"

class WebManager {
public:
    WebManager();
    void begin();
    void log(const String& message);

private:
    AsyncWebServer server;
    AsyncWebSocket ws;
    
    static void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
};

#endif // WEB_MANAGER_H