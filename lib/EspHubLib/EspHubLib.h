#ifndef ESP_HUB_LIB_H
#define ESP_HUB_LIB_H

#include <Arduino.h>
#include <painlessMesh.h>
#include "MqttManager.h"
#include "../PlcCore/PlcEngine.h"
#include "WebManager.h"
#include "StreamLogger.h"
#include "TimeManager.h"
#include "AppManager.h"
#include "MeshDeviceManager.h" // New MeshDeviceManager
#include "UserManager.h" // New UserManager
#include "MqttDiscoveryManager.h" // New MqttDiscoveryManager
#include "OtaManager.h" // New OtaManager
#include "DeviceConfigManager.h" // Device configuration manager
#include "VariableRegistry.h" // Variable registry for unified variable access
#include "MqttExportManager.h" // MQTT export manager for hybrid variable/command export

// Conditional protocol manager includes
#ifdef USE_WIFI_DEVICES
#include "WiFiDeviceManager.h"
#endif

#ifdef USE_RF433
#include "RF433Manager.h"
#endif

#ifdef USE_ZIGBEE
#include "ZigbeeManager.h"
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