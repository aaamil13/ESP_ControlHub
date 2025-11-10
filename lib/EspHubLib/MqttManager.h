#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <WiFiClient.h>

// Forward declaration to avoid including WiFiClientSecure.h in header
class WiFiClientSecure;

class MqttManager {
public:
    MqttManager();
    ~MqttManager();
    void begin(const char* server, int port, bool use_tls = false, const char* ca_cert_path = "", const char* client_cert_path = "", const char* client_key_path = "");
    void loop();
    void setCallback(MQTT_CALLBACK_SIGNATURE);
    void publish(const char* topic, const char* payload);
    void subscribe(const char* topic);

private:
    WiFiClient wifiClient;
    WiFiClientSecure* wifiClientSecure; // For MQTTS (pointer to avoid including header)
    PubSubClient mqttClient;
    bool _use_tls;
    void reconnect();
};

#endif // MQTT_MANAGER_H