#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <Update.h>
#include <WiFiClient.h>
#include "StreamLogger.h" // For Log

class OtaManager {
public:
    OtaManager();
    void begin();
    void startOtaUpdate(const String& firmwareUrl);

private:
    WiFiClient _otaClient;
};

#endif // OTA_MANAGER_H