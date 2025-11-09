#ifndef ESP_HUB_LIB_H
#define ESP_HUB_LIB_H

#include <Arduino.h>
#include <esp_now.h>
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

private:
    DeviceManager deviceManager;
    MqttManager mqttManager;
    PlcEngine plcEngine;
    WebManager webManager;
    StreamLogger logger;
    static EspHub* instance;

    static void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len);
    static void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status);
};

#endif // ESP_HUB_LIB_H