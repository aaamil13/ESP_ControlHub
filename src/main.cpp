#include <Arduino.h>
#include <EspHubLib.h>
#include <WiFi.h>
#include <WiFiManager.h>

// --- MQTT Configuration ---
const char* mqtt_server = "YOUR_MQTT_BROKER_IP";
const int mqtt_port = 1883;
// -----------------------------------

EspHub hub;

void mqtt_callback(char* topic, byte* payload, unsigned int length) {
  Log->print("Message arrived [");
  Log->print(topic);
  Log->print("] ");
  
  // Null-terminate the payload
  char* message = (char*)payload;
  message[length] = '\0';
  Log->println(message);

  if (strcmp(topic, "esphub/config/plc") == 0) {
    hub.loadPlcConfiguration(message);
  } else if (strcmp(topic, "esphub/plc/control") == 0) {
    if (strcmp(message, "run") == 0) {
      hub.runPlc();
    } else if (strcmp(message, "stop") == 0) {
      hub.stopPlc();
    }
  }
}

void setup() {
  Serial.begin(115200);
  
  hub.begin();
  
  // WiFiManager
  WiFiManager wm;
  // wm.resetSettings(); // Uncomment to reset saved settings
  
  if (!wm.autoConnect("EspHub-Config")) {
    Log->println("Failed to connect and hit timeout");
    ESP.restart();
  }

  Log->println("\nWiFi connected");

  hub.setupMqtt(mqtt_server, mqtt_port, mqtt_callback);
}

void loop() {
  hub.loop();
}