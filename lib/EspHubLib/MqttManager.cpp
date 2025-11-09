#include "MqttManager.h"

MqttManager::MqttManager() : mqttClient(wifiClient) {
}

void MqttManager::begin(const char* server, int port) {
    mqttClient.setServer(server, port);
}

void MqttManager::setCallback(MQTT_CALLBACK_SIGNATURE) {
    mqttClient.setCallback(callback);
}

void MqttManager::loop() {
    if (!mqttClient.connected()) {
        reconnect();
    }
    mqttClient.loop();
}

void MqttManager::publish(const char* topic, const char* payload) {
    mqttClient.publish(topic, payload);
}

void MqttManager::subscribe(const char* topic) {
    mqttClient.subscribe(topic);
}

void MqttManager::reconnect() {
    // Loop until we're reconnected
    while (!mqttClient.connected()) {
        Log->print("Attempting MQTT connection...");
        // Attempt to connect
        if (mqttClient.connect("EspHubClient")) {
            Log->println("connected");
            // Once connected, publish an announcement...
            mqttClient.publish("esphub/status", "online");
            // ... and resubscribe
            subscribe("esphub/config/plc");
            subscribe("esphub/plc/control");
        } else {
            Log->print("failed, rc=");
            Log->print(mqttClient.state());
            Log->println(" try again in 5 seconds");
            // Wait 5 seconds before retrying
            delay(5000);
        }
    }
}