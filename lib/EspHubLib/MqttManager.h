#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h> // For MQTTS

class MqttManager {
public:
    MqttManager();
    void begin(const char* server, int port, bool use_tls = false);
    void loop();
    void setCallback(MQTT_CALLBACK_SIGNATURE);
    void publish(const char* topic, const char* payload);
    void subscribe(const char* topic);

private:
    WiFiClient wifiClient;
    WiFiClientSecure wifiClientSecure; // For MQTTS
    PubSubClient mqttClient;
    bool _use_tls;
    void reconnect();
};

#endif // MQTT_MANAGER_H