#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFiClient.h>

class MqttManager {
public:
    MqttManager();
    void begin(const char* server, int port);
    void loop();
    void setCallback(MQTT_CALLBACK_SIGNATURE);
    void publish(const char* topic, const char* payload);
    void subscribe(const char* topic);

private:
    WiFiClient wifiClient;
    PubSubClient mqttClient;
    void reconnect();
};

#endif // MQTT_MANAGER_H