#ifndef MQTT_DISCOVERY_MANAGER_H
#define MQTT_DISCOVERY_MANAGER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include "../Protocols/Mqtt/MqttManager.h"
#include "../PlcEngine/Engine/PlcEngine.h"

class MqttDiscoveryManager {
public:
    MqttDiscoveryManager(MqttManager* mqttManager, PlcEngine* plcEngine);
    void begin(const String& baseTopic, const String& deviceName, const String& deviceId);
    void publishDiscoveryMessages();

private:
    MqttManager* _mqttManager;
    PlcEngine* _plcEngine;
    String _baseTopic;
    String _deviceName;
    String _deviceId;

    void publishSensorDiscovery(const String& varName, const PlcVariable& var);
    void publishBinarySensorDiscovery(const String& varName, const PlcVariable& var);
    void publishSwitchDiscovery(const String& varName, const PlcVariable& var);
    // Add other component types as needed
};

#endif // MQTT_DISCOVERY_MANAGER_H