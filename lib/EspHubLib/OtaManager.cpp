#include "OtaManager.h"

extern StreamLogger* Log;

OtaManager::OtaManager() {
}

void OtaManager::begin() {
    // Optional: Set up OTA event handlers for more detailed logging
    Update.onStart([](){
        Log->println("OTA update started!");
    });
    Update.onEnd([](){
        Log->println("OTA update finished!");
    });
    Update.onError([](int error){
        Log->printf("OTA update error: %d\n", error);
    });
    Update.onProgress([](size_t current, size_t total){
        Log->printf("OTA progress: %u%%\n", (current * 100) / total);
    });
}

void OtaManager::startOtaUpdate(const String& firmwareUrl) {
    Log->printf("Starting OTA update from: %s\n", firmwareUrl.c_str());

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
                    Log->println("Written : " + String(written) + " successfully");
                } else {
                    Log->println("Written only : " + String(written) + "/" + String(contentLength) + ". Retry?");
                }
                if (Update.end()) {
                    Log->println("OTA update finished successfully. Restarting...");
                    ESP.restart();
                } else {
                    Log->printf("OTA update failed: %u\n", Update.getError());
                }
            } else {
                Log->println("Not enough space to begin OTA");
            }
        } else {
            Log->printf("HTTP GET failed: %s\n", http.errorToString(httpCode).c_str());
        }
        http.end();
    } else {
        Log->printf("Failed to connect to OTA server: %s\n", firmwareUrl.c_str());
    }
}