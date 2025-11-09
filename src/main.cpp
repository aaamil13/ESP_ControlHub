#include <Arduino.h>
#include <EspHubLib.h>
#include <WiFi.h>
#include <WiFiManager.h>

// --- MQTT, Time and Mesh Configuration ---
const char* mqtt_server = "YOUR_MQTT_BROKER_IP";
const int mqtt_port = 1883;
char tz_info[64] = "EET-2EEST,M3.5.0/3,M10.5.0/4"; // Default to Europe/Sofia
char mesh_password[64] = "password1234"; // Default mesh password
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
  
  WiFiManagerParameter custom_tz("tz", "Timezone String", tz_info, sizeof(tz_info));
  WiFiManagerParameter custom_mesh_pass("mesh_pass", "Mesh Password", mesh_password, sizeof(mesh_password));
  wm.addParameter(&custom_tz);
  wm.addParameter(&custom_mesh_pass);

  if (!wm.autoConnect("EspHub-Config")) {
    Log->println("Failed to connect and hit timeout");
    ESP.restart();
  }

  // Save the custom parameters
  strcpy(tz_info, custom_tz.getValue());
  strcpy(mesh_password, custom_mesh_pass.getValue());
  // Here you would save tz_info and mesh_password to NVS for persistence

  Log->println("\nWiFi connected");
  Log->printf("Timezone: %s\n", tz_info);
  Log->printf("Mesh Password: %s\n", mesh_password);

  hub.setupMesh(mesh_password);
  // The mesh will attempt to connect to the MQTT broker
  // ONLY if it is the root node.
  hub.setupMqtt(mqtt_server, mqtt_port, mqtt_callback, false); // Set to true for MQTTS
  hub.setupTime(tz_info);
}

void loop() {
  hub.loop();
}