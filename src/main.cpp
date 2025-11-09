#include <Arduino.h>
#include <EspHubLib.h>
#include <WiFi.h>
// --- MQTT, Time and Mesh Configuration ---
const char* mqtt_server = "YOUR_MQTT_BROKER_IP";
const int mqtt_port = 1883;
char tz_info[64] = "EET-2EEST,M3.5.0/3,M10.5.0/4"; // Default to Europe/Sofia
char mesh_password[64] = "your_mesh_password"; // Hardcoded for now
char mqtt_ca_cert_path[64] = ""; // Path to CA certificate in LittleFS
char mqtt_client_cert_path[64] = ""; // Path to client certificate in LittleFS
char mqtt_client_key_path[64] = ""; // Path to client private key in LittleFS
// -----------------------------------

EspHub hub;

void mqtt_callback(char* topic, byte* payload, unsigned int length) {
  EspHubLog->print("Message arrived [");
  EspHubLog->print(topic);
  EspHubLog->print("] ");
  
  // Null-terminate the payload
  char* message = (char*)payload;
  message[length] = '\0';
  EspHubLog->println(message);

  if (strcmp(topic, "esphub/config/plc") == 0) {
    // For now, assume a default program name "main_program"
    hub.loadPlcConfiguration(message);
  } else if (strcmp(topic, "esphub/plc/control") == 0) {
    // Example: {"program": "main_program", "command": "run"}
    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, message);
    if (error) {
        EspHubLog->printf("deserializeJson() failed for PLC control message: %s\n", error.c_str());
        return;
    }
    String programName = doc["program"].as<String>();
    String command = doc["command"].as<String>();

    if (command == "run") {
      hub.runPlc(programName);
    } else if (command == "stop") {
      hub.stopPlc(programName);
    } else if (command == "pause") {
      hub.pausePlc(programName);
    } else if (command == "delete") {
      hub.deletePlc(programName);
    }
  } else if (strcmp(topic, "esphub/system/control") == 0) {
    if (strcmp(message, "factory_reset") == 0) {
      hub.factoryReset();
    } else if (strcmp(message, "restart") == 0) {
      hub.restartEsp();
    }
  } else if (strcmp(topic, "esphub/ota/update") == 0) {
    // The payload should be the URL of the firmware file
    hub.mqttCallback(topic, payload, length); // Forward to EspHub for OTA handling
  }
}

void setup() {
  Serial.begin(115200);
  
  hub.begin();
  
  // Hardcoded Wi-Fi credentials for now
  const char* ssid = "YOUR_WIFI_SSID";
  const char* password = "YOUR_WIFI_PASSWORD";

  WiFi.begin(ssid, password);
  EspHubLog->print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    EspHubLog->print(".");
  }
  EspHubLog->println("\nWiFi connected");
  EspHubLog->printf("IP Address: %s\n", WiFi.localIP().toString().c_str());

  // Save the custom parameters (no WiFiManager)
  // strcpy(tz_info, custom_tz.getValue()); // No custom_tz
  // strcpy(mesh_password, custom_mesh_pass.getValue()); // No custom_mesh_pass
  // strcpy(mqtt_ca_cert_path, custom_mqtt_ca.getValue()); // No custom_mqtt_ca
  // strcpy(mqtt_client_cert_path, custom_mqtt_client_cert.getValue()); // No custom_mqtt_client_cert
  // strcpy(mqtt_client_key_path, custom_mqtt_client_key.getValue()); // No custom_mqtt_client_key
  // TODO: Save all these parameters to NVS for persistence
  
  EspHubLog->println("\nWiFi connected");
  EspHubLog->printf("Timezone: %s\n", tz_info);
  EspHubLog->printf("Mesh Password: %s\n", mesh_password);
  EspHubLog->printf("MQTT CA Cert Path: %s\n", mqtt_ca_cert_path);
  EspHubLog->printf("MQTT Client Cert Path: %s\n", mqtt_client_cert_path);
  EspHubLog->printf("MQTT Client Key Path: %s\n", mqtt_client_key_path);

  if (strlen(mesh_password) > 0) {
    hub.setupMesh(mesh_password);
  } else {
    EspHubLog->println("WARNING: Mesh password not set. Mesh network will not be started.");
  }
  
  // The mesh will attempt to connect to the MQTT broker
  // ONLY if it is the root node.
  // TODO: Add MQTT username/password to WiFiManager config
  bool use_tls = (strlen(mqtt_ca_cert_path) > 0 && strlen(mqtt_client_cert_path) > 0 && strlen(mqtt_client_key_path) > 0);
  hub.setupMqtt(mqtt_server, mqtt_port, mqtt_callback, use_tls, mqtt_ca_cert_path, mqtt_client_cert_path, mqtt_client_key_path);
  hub.setupTime(tz_info);

  // Check for factory reset button press (e.g., BOOT button on ESP32)
  // This assumes GPIO0 is connected to the BOOT button and pulled up internally.
  // Holding it for >5 seconds will trigger a factory reset.
  // A short press (e.g., <1 second) could trigger a soft restart.
  pinMode(0, INPUT_PULLUP);
  unsigned long buttonPressStartTime = 0;
  bool buttonPressed = false;

  // This loop runs very quickly, so we need to debounce and measure press time
  // This is a simplified example, a proper implementation would use a state machine or timer.
  if (digitalRead(0) == LOW) { // Button is pressed
    if (!buttonPressed) {
      buttonPressed = true;
      buttonPressStartTime = millis();
    }
    if (millis() - buttonPressStartTime > 5000) { // Held for 5 seconds
      hub.factoryReset();
    }
  } else {
    if (buttonPressed) { // Button was just released
      if (millis() - buttonPressStartTime < 1000) { // Short press (<1 second)
        hub.restartEsp();
      }
      buttonPressed = false;
    }
  }
}

void loop() {
  hub.loop();
}