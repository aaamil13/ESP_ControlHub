#include "MqttManager.h"
#include "LITTLEFS.h" // For loading certificates

MqttManager::MqttManager() : mqttClient(wifiClient) {
}

void MqttManager::begin(const char* server, int port, bool use_tls, const char* ca_cert_path, const char* client_cert_path, const char* client_key_path) {
    _use_tls = use_tls;
    if (strlen(server) == 0) {
        Log->println("WARNING: MQTT server not configured. MQTT client will not connect.");
        return;
    }

    if (_use_tls) {
        // Load certificates from LittleFS
        if (strlen(ca_cert_path) > 0) {
            File ca = LITTLEFS.open(ca_cert_path, "r");
            if (ca) {
                wifiClientSecure.setCACert(ca.readString().c_str());
                ca.close();
                Log->printf("Loaded CA cert from %s\n", ca_cert_path);
            } else {
                Log->printf("ERROR: Failed to open CA cert file %s\n", ca_cert_path);
            }
        }
        if (strlen(client_cert_path) > 0 && strlen(client_key_path) > 0) {
            File client_cert = LITTLEFS.open(client_cert_path, "r");
            File client_key = LITTLEFS.open(client_key_path, "r");
            if (client_cert && client_key) {
                wifiClientSecure.setCertificate(client_cert.readString().c_str());
                wifiClientSecure.setPrivateKey(client_key.readString().c_str());
                client_cert.close();
                client_key.close();
                Log->printf("Loaded client cert and key from %s and %s\n", client_cert_path, client_key_path);
            } else {
                Log->printf("ERROR: Failed to open client cert/key files %s, %s\n", client_cert_path, client_key_path);
            }
        }
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