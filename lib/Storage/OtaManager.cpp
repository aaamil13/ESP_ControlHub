#include "../Storage/OtaManager.h"
#include "../Core/StreamLogger.h"

extern StreamLogger* EspHubLog;

OtaManager::OtaManager() {
}

void OtaManager::begin() {
    // Note: Update event handlers (onStart, onEnd, onError, onProgress)
    // are not available in all ESP32 core versions
    // Logging is done inline in startOtaUpdate() instead
    EspHubLog->println("OtaManager initialized");
}

void OtaManager::startOtaUpdate(const String& firmwareUrl) {
    EspHubLog->printf("Starting OTA update from: %s\n", firmwareUrl.c_str());

    if (_otaClient.connect(firmwareUrl.c_str(), 80)) { // Assuming HTTP for now
        HTTPClient http;
        http.begin(_otaClient, firmwareUrl);

        int httpCode = http.GET();
        if (httpCode == HTTP_CODE_OK) {
            int contentLength = http.getSize();
            bool canBegin = Update.begin(contentLength);

            if (canBegin) {
                WiFiClient& client = http.getStream();
                size_t written = Update.writeStream(client);
                if (written == contentLength) {
                    EspHubLog->println("Written : " + String(written) + " successfully");
                } else {
                    EspHubLog->println("Written only : " + String(written) + "/" + String(contentLength) + ". Retry?");
                }
                if (Update.end()) {
                    EspHubLog->println("OTA update finished successfully. Restarting...");
                    ESP.restart();
                } else {
                    EspHubLog->printf("OTA update failed: %u\n", Update.getError());
                }
            } else {
                EspHubLog->println("Not enough space to begin OTA");
            }
        } else {
            EspHubLog->printf("HTTP GET failed: %s\n", http.errorToString(httpCode).c_str());
        }
        http.end();
    } else {
        EspHubLog->printf("Failed to connect to OTA server: %s\n", firmwareUrl.c_str());
    }
}