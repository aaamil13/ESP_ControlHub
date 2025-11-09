#include "WebManager.h"

WebManager::WebManager() : server(80), ws("/ws") {
}

void WebManager::begin() {
    ws.onEvent(onWsEvent);
    server.addHandler(&ws);

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", "Hello, world. Go to /log for live logs.");
    });

    // Simple HTML page for the logger
    server.on("/log", HTTP_GET, [](AsyncWebServerRequest *request){
        const char* html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>EspHub Live Log</title>
    <meta charset="UTF-8">
</head>
<body>
    <h1>EspHub Live Log</h1>
    <pre id="log"></pre>
    <script>
        var ws = new WebSocket("ws://" + window.location.hostname + "/ws");
        ws.onmessage = function(evt) {
            var log = document.getElementById('log');
            log.innerHTML += evt.data + '\n';
        };
    </script>
</body>
</html>
)rawliteral";
        request->send(200, "text/html", html);
    });

    server.begin();
    Log->println("Web server started.");
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