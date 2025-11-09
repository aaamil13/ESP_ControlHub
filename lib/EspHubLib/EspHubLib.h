#ifndef ESP_HUB_LIB_H
#define ESP_HUB_LIB_H

#include <Arduino.h>
#include <painlessMesh.h>
#include "DeviceManager.h"
#include "MqttManager.h"
#include "PlcEngine.h"
#include "WebManager.h"
#include "StreamLogger.h"

extern StreamLogger* Log;

class EspHub {
public:
    EspHub();
    void begin();
    void loop();
    void setupMqtt(const char* server, int port, MQTT_CALLBACK_SIGNATURE);
    void loadPlcConfiguration(const char* jsonConfig);
    void runPlc();
    void stopPlc();

private:
    painlessMesh mesh;
    DeviceManager deviceManager;
    MqttManager mqttManager;
    PlcEngine plcEngine;
    WebManager webManager;
    StreamLogger logger;
    static EspHub* instance;

    // painlessMesh callbacks
    static void receivedCallback(uint32_t from, String &msg);
    static void newConnectionCallback(uint32_t nodeId);
    static void changedConnectionCallback();
    static void nodeTimeAdjustedCallback(int32_t offset);
};

#endif // ESP_HUB_LIB_H