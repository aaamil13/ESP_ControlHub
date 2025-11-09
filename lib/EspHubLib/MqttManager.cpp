#include "MqttManager.h"

MqttManager::MqttManager() : mqttClient(wifiClient) {
}

void MqttManager::begin(const char* server, int port, bool use_tls) {
    _use_tls = use_tls;
    if (strlen(server) == 0) {
        Log->println("WARNING: MQTT server not configured. MQTT client will not connect.");
        return;
    }

    if (_use_tls) {
        // Configure for MQTTS
        // In a real application, you would load CA cert, client cert, and private key here.
        // For example:
        // wifiClientSecure.setCACert(ca_cert);
        // wifiClientSecure.setCertificate(client_cert);
        // wifiClientSecure.setPrivateKey(private_key);
        mqttClient.setClient(wifiClientSecure);
    } else {
        mqttClient.setClient(wifiClient);
    }
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
    static unsigned long lastReconnectAttempt = 0;
    static unsigned long reconnectInterval = 1000; // Start with 1 second

    // Loop until we're reconnected
    while (!mqttClient.connected()) {
        unsigned long currentMillis = millis();
        if (currentMillis - lastReconnectAttempt > reconnectInterval) {
            lastReconnectAttempt = currentMillis;
            Log->print("Attempting MQTT connection...");
            // Attempt to connect
            bool connected;
            if (_use_tls) {
                connected = mqttClient.connect("EspHubClient-TLS");
            } else {
                connected = mqttClient.connect("EspHubClient");
            }

            if (connected) {
                Log->println("connected");
                // Once connected, publish an announcement...
                mqttClient.publish("esphub/status", "online");
                // ... and resubscribe
                subscribe("esphub/config/plc");
                subscribe("esphub/plc/control");
                reconnectInterval = 1000; // Reset interval on success
            } else {
                Log->print("failed, rc=");
                Log->print(mqttClient.state());
                Log->print(" retrying in ");
                Log->print(reconnectInterval / 1000);
                Log->println(" seconds");
                // Increase interval exponentially, up to a limit
                reconnectInterval *= 2;
                if (reconnectInterval > 60000) reconnectInterval = 60000; // Max 1 minute
            }
        }
        // Allow other tasks to run
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}