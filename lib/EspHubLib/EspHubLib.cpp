#include "EspHubLib.h"
#include "esp_hub_protocol.h"
#include <WiFi.h>

StreamLogger* Log = nullptr;
EspHub* EspHub::instance = nullptr;

EspHub::EspHub() : logger(webManager) {
    instance = this;
    Log = &logger;
}

void EspHub::begin() {
    deviceManager.begin();
    webManager.begin();
    plcEngine.begin();

    // Set device as a Wi-Fi Station
    WiFi.mode(WIFI_STA);

    // Init ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Log->println("Error initializing ESP-NOW");
        return;
    }

    // Register for recv CB to get recv packer info
    esp_now_register_recv_cb(OnDataRecv);
    esp_now_register_send_cb(OnDataSent);

    Log->println("EspHub Library Initialized with ESP-NOW");
}

void EspHub::setupMqtt(const char* server, int port, MQTT_CALLBACK_SIGNATURE) {
    mqttManager.begin(server, port);
    mqttManager.setCallback(callback);
}

void EspHub::loadPlcConfiguration(const char* jsonConfig) {
    plcEngine.loadConfiguration(jsonConfig);
}

void EspHub::loop() {
    mqttManager.loop();
    plcEngine.evaluate();
}

void EspHub::OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
    if (instance == nullptr) return;

    message_type_t type = ((message_type_t *)incomingData)[0];
    
    Log->print("Packet received from: ");
    for (int i = 0; i < 6; i++) {
        Log->print(mac[i], HEX);
        if (i < 5) Log->print(":");
    }
    Log->println();

    switch (type) {
        case MESSAGE_TYPE_REGISTRATION: {
            registration_message_t* msg = (registration_message_t*)incomingData;
            Log->printf("Registration request from device %d\n", msg->id);
            instance->deviceManager.addDevice(mac, msg->id);
            break;
        }
        case MESSAGE_TYPE_DATA: {
            data_message_t* msg = (data_message_t*)incomingData;
            Log->printf("Data from device %d: %f\n", msg->id, msg->value);
            
            // Publish data to MQTT
            char topic[50];
            sprintf(topic, "esphub/device/%d/value", msg->id);
            char payload[20];
            dtostrf(msg->value, 1, 2, payload);
            instance->mqttManager.publish(topic, payload);
            
            break;
        }
    }
}

void EspHub::OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    Log->print("Last Packet Send Status: ");
    Log->println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}