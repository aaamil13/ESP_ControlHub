#ifndef ESP_HUB_LIB_H
#define ESP_HUB_LIB_H

#include <Arduino.h>
#include <painlessMesh.h>
#include "MqttManager.h"
#include "../PlcCore/PlcEngine.h"
#include "WebManager.h"
#include "StreamLogger.h"
#include "TimeManager.h"
#include "AppManager.h"
#include "MeshDeviceManager.h" // New MeshDeviceManager
#include "UserManager.h" // New UserManager

extern StreamLogger* Log;

class EspHub {
public:
    EspHub();
    void begin();
    void loop();
    void setupMesh(const char* password);
    void setupMqtt(const char* server, int port, MQTT_CALLBACK_SIGNATURE, bool use_tls = false);
    void setupTime(const char* tz_info);
    void loadPlcConfiguration(const char* jsonConfig);
    void runPlc(const String& programName);
    void pausePlc(const String& programName);
    void stopPlc(const String& programName);
    void deletePlc(const String& programName);
    void factoryReset();
    void restartEsp(); // New method for software restart

private:
    painlessMesh mesh;
    MqttManager mqttManager;
    PlcEngine plcEngine;
    WebManager webManager;
    TimeManager timeManager;
    AppManager appManager;
    MeshDeviceManager meshDeviceManager; // New MeshDeviceManager instance
    UserManager userManager; // New UserManager instance
    StreamLogger logger;
    static EspHub* instance;

    // painlessMesh callbacks
    static void receivedCallback(uint32_t from, String &msg);
    static void newConnectionCallback(uint32_t nodeId);
    static void changedConnectionCallback();
    static void nodeTimeAdjustedCallback(int32_t offset);
};

#endif // ESP_HUB_LIB_H