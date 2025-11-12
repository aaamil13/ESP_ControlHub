#ifndef ESP_HUB_LIB_H
#define ESP_HUB_LIB_H

#include <Arduino.h>
#include <painlessMesh.h>
#include "../Protocols/Mqtt/MqttManager.h"
#include "../PlcEngine/Engine/PlcEngine.h"
#include "../UI/WebManager.h"
#include "../Core/StreamLogger.h"
#include "../Core/TimeManager.h"
#include "../Apps/AppManager.h"
#include "../Protocols/Mesh/MeshDeviceManager.h" // New MeshDeviceManager
#include "../Storage/UserManager.h" // New UserManager
#include "../Export/MqttDiscoveryManager.h" // New MqttDiscoveryManager
#include "../Storage/OtaManager.h" // New OtaManager
#include "../Devices/DeviceConfigManager.h" // Device configuration manager
#include "../Export/VariableRegistry.h" // Variable registry for unified variable access
#include "../Export/MqttExportManager.h" // MQTT export manager for hybrid variable/command export
#include "../Export/MeshExportManager.h" // Mesh export manager for variable sharing between hubs
#include "../PlcEngine/Events/IOEventManager.h" // Event-driven system for I/O and scheduled triggers

// Conditional protocol manager includes
#ifdef USE_WIFI_DEVICES
#include "../Protocols/WiFi/WiFiDeviceManager.h"
#endif

#ifdef USE_RF433
#include "../Protocols/RF433/RF433Manager.h"
#endif

#ifdef USE_ZIGBEE
#include "../Protocols/Zigbee/ZigbeeManager.h"
#endif

extern StreamLogger* EspHubLog;

class EspHub {
public:
    EspHub();
    void begin();
    void loop();
    void setupMesh(const char* password);
    void setupMqtt(const char* server, int port, MQTT_CALLBACK_SIGNATURE, bool use_tls = false, const char* ca_cert_path = "", const char* client_cert_path = "", const char* client_key_path = "");
    void mqttCallback(char* topic, byte* payload, unsigned int length); // New method for MQTT callback
    void setupTime(const char* tz_info);
    void loadPlcConfiguration(const char* jsonConfig);
    void runPlc(const String& programName);
    void pausePlc(const String& programName);
    void stopPlc(const String& programName);
    void deletePlc(const String& programName);
    void factoryReset();
    void restartEsp(); // New method for software restart

    // Event system
    void loadEventConfiguration(const char* jsonConfig); // Load event triggers from JSON
    String getEventHistory(bool unreadOnly = true); // Get event history as JSON
    void clearEventHistory(); // Clear event history
    void markEventsAsRead(); // Mark events as published to MQTT

private:
    painlessMesh mesh;
    MqttManager mqttManager;
    PlcEngine plcEngine;
    WebManager webManager;
    TimeManager timeManager;
    AppManager appManager;
    MeshDeviceManager meshDeviceManager; // New MeshDeviceManager instance
    UserManager userManager; // New UserManager instance
    MqttDiscoveryManager mqttDiscoveryManager; // New MqttDiscoveryManager instance
    OtaManager otaManager; // New OtaManager instance
    DeviceConfigManager deviceConfigManager; // Device configuration manager
    VariableRegistry variableRegistry; // Variable registry for unified access
    MqttExportManager mqttExportManager; // MQTT export manager for hybrid export
    MeshExportManager meshExportManager; // Mesh export manager for variable sharing
    IOEventManager ioEventManager; // Event-driven system for I/O and scheduled triggers

    // Conditional protocol managers
    #ifdef USE_WIFI_DEVICES
    WiFiDeviceManager* wifiDeviceManager;
    #endif

    #ifdef USE_RF433
    RF433Manager* rf433Manager;
    #endif

    #ifdef USE_ZIGBEE
    ZigbeeManager* zigbeeManager;
    #endif

    StreamLogger logger;
    static EspHub* instance;

    // painlessMesh callbacks
    static void receivedCallback(uint32_t from, String &msg);
    static void newConnectionCallback(uint32_t nodeId);
    static void changedConnectionCallback();
    static void nodeTimeAdjustedCallback(int32_t offset);
};

#endif // ESP_HUB_LIB_H