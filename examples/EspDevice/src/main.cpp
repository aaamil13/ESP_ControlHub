#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include "esp_hub_protocol.h"

// REPLACE WITH THE MAC ADDRESS OF YOUR ESPHUB
uint8_t hubAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// Define a unique ID for this device
#define DEVICE_ID 1

esp_now_peer_info_t peerInfo;

// Callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("\r\nLast Packet Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}
 
void setup() {
  Serial.begin(115200);
 
  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  esp_now_register_send_cb(OnDataSent);
  
  // Register peer
  memcpy(peerInfo.peer_addr, hubAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer");
    return;
  }

  // Send registration message
  registration_message_t regMsg;
  regMsg.type = MESSAGE_TYPE_REGISTRATION;
  regMsg.id = DEVICE_ID;
  esp_now_send(hubAddress, (uint8_t *) &regMsg, sizeof(regMsg));
}
 
void loop() {
  // Send a data message every 10 seconds
  data_message_t dataMsg;
  dataMsg.type = MESSAGE_TYPE_DATA;
  dataMsg.id = DEVICE_ID;
  dataMsg.value = random(10, 30); // Simulate temperature reading

  esp_err_t result = esp_now_send(hubAddress, (uint8_t *) &dataMsg, sizeof(dataMsg));
   
  if (result == ESP_OK) {
    Serial.println("Sent with success");
  }
  else {
    Serial.println("Error sending the data");
  }
  delay(10000);
}