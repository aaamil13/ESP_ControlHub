#include "MqttDiscoveryManager.h"
#include "StreamLogger.h" // For Log

extern StreamLogger* Log;

MqttDiscoveryManager::MqttDiscoveryManager(MqttManager* mqttManager, PlcEngine* plcEngine)
    : _mqttManager(mqttManager), _plcEngine(plcEngine) {
}

void MqttDiscoveryManager::begin(const String& baseTopic, const String& deviceName, const String& deviceId) {
    _baseTopic = baseTopic;
    _deviceName = deviceName;
    _deviceId = deviceId;
    Log->printf("MQTT Discovery Manager initialized for device %s (%s)\n", _deviceName.c_str(), _deviceId.c_str());
}

void MqttDiscoveryManager::publishDiscoveryMessages() {
    if (!_mqttManager || !_plcEngine || !_mqttManager->isConnected()) {
        Log->println("MQTT not connected, skipping discovery message publishing.");
        return;
    }

    // Iterate through all PLC variables and publish discovery messages based on metadata
    for (const auto& pair : _plcEngine->getMemory().getAllVariables()) { // Assuming getAllVariables() exists
        const String& varName = pair.first;
        const PlcVariable& var = pair.second;

        // Example: Publish sensor discovery for REAL type variables
        if (var.type == PlcDataType::REAL) {
            publishSensorDiscovery(varName, var);
        } else if (var.type == PlcDataType::BOOL) {
            // Decide if it's a binary sensor or a switch
            // For now, let's assume all bools are binary sensors
            publishBinarySensorDiscovery(varName, var);
        }
        // Add more conditions for other types and component types
    }
}

void MqttDiscoveryManager::publishSensorDiscovery(const String& varName, const PlcVariable& var) {
    StaticJsonDocument<512> doc;
    String unique_id = _deviceId + "_" + varName;
    String state_topic = _baseTopic + "/" + _deviceId + "/sensor/" + varName + "/state";
    String config_topic = String("homeassistant/sensor/") + _deviceId + "/" + varName + "/config";

    doc["name"] = varName;
    doc["unique_id"] = unique_id;
    doc["state_topic"] = state_topic;
    doc["value_template"] = "{{ value }}";
    doc["device"]["identifiers"] = _deviceId;
    doc["device"]["name"] = _deviceName;
    doc["device"]["model"] = "EspHub PLC";
    doc["device"]["manufacturer"] = "Custom";

    // Add unit_of_measurement if available in var metadata
    // doc["unit_of_measurement"] = "°C";

    String payload;
    serializeJson(doc, payload);
    _mqttManager->publish(config_topic.c_str(), payload.c_str());
    Log->printf("Published sensor discovery for %s\n", varName.c_str());
}

void MqttDiscoveryManager::publishBinarySensorDiscovery(const String& varName, const PlcVariable& var) {
    StaticJsonDocument<512> doc;
    String unique_id = _deviceId + "_" + varName;
    String state_topic = _baseTopic + "/" + _deviceId + "/binary_sensor/" + varName + "/state";
    String config_topic = String("homeassistant/binary_sensor/") + _deviceId + "/" + varName + "/config";

    doc["name"] = varName;
    doc["unique_id"] = unique_id;
    doc["state_topic"] = state_topic;
    doc["payload_on"] = "true";
    doc["payload_off"] = "false";
    doc["device"]["identifiers"] = _deviceId;
    doc["device"]["name"] = _deviceName;
    doc["device"]["model"] = "EspHub PLC";
    doc["device"]["manufacturer"] = "Custom";

    String payload;
    serializeJson(doc, payload);
    _mqttManager->publish(config_topic.c_str(), payload.c_str());
    Log->printf("Published binary sensor discovery for %s\n", varName.c_str());
}