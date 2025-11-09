#include "WebManager.h"

WebManager::WebManager() : server(80), ws("/ws") {
}

void WebManager::begin() {
    if(!LITTLEFS.begin()){
        Log->println("An Error has occurred while mounting LITTLEFS");
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

    server.serveStatic("/", LITTLEFS, "/");

    AsyncElegantOTA.begin(&server);    // Start ElegantOTA
    server.begin();
    Log->println("Web server started. OTA available at /update");
}

void WebManager::log(const String& message) {
    ws.textAll(message);
}

void WebManager::onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    if(type == WS_EVT_CONNECT){
        Log->printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
        client->text("Welcome!");
    } else if(type == WS_EVT_DISCONNECT){
        Log->printf("WebSocket client #%u disconnected\n", client->id());
    }
}