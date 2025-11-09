#include <Arduino.h>
#include <EspHubLib.h>
#include <WiFi.h>
#include <WiFiManager.h>

// --- MQTT, Time and Mesh Configuration ---
const char* mqtt_server = "YOUR_MQTT_BROKER_IP";
const int mqtt_port = 1883;
char tz_info[64] = "EET-2EEST,M3.5.0/3,M10.5.0/4"; // Default to Europe/Sofia
char mesh_password[64] = ""; // Mesh password, will be configured via WiFiManager
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
    // For now, assume a default program name "main_program"
    hub.loadPlcConfiguration(message);
  } else if (strcmp(topic, "esphub/plc/control") == 0) {
    // Example: {"program": "main_program", "command": "run"}
    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, message);
    if (error) {
        Log->printf("deserializeJson() failed for PLC control message: %s\n", error.c_str());
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
  // TODO: Save tz_info and mesh_password to NVS for persistence

  Log->println("\nWiFi connected");
  Log->printf("Timezone: %s\n", tz_info);
  Log->printf("Mesh Password: %s\n", mesh_password);

  if (strlen(mesh_password) > 0) {
    hub.setupMesh(mesh_password);
  } else {
    Log->println("WARNING: Mesh password not set. Mesh network will not be started.");
  }
  
  // The mesh will attempt to connect to the MQTT broker
  // ONLY if it is the root node.
  // TODO: Add MQTT username/password to WiFiManager config
  hub.setupMqtt(mqtt_server, mqtt_port, mqtt_callback, false); // Set to true for MQTTS
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