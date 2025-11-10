# Integration Example: Zigbee Devices via Zigbee2MQTT

This document shows how to use ZigbeeManager to integrate Zigbee devices through Zigbee2MQTT bridge.

## Prerequisites

1. **Zigbee2MQTT** running and connected to MQTT broker
   - See: https://www.zigbee2mqtt.io/
   - Default topic: `zigbee2mqtt`

2. **MQTT Broker** (Mosquitto, HiveMQ, etc.)
   - Must be accessible from ESP32

3. **Zigbee USB Coordinator** (CC2531, CC2652, etc.)
   - Connected to Zigbee2MQTT host

## Setup

### 1. Initialize Components

```cpp
#include <EspHub.h>
#include <ZigbeeManager.h>

// Global instances
MqttManager mqttManager;
ZigbeeManager* zigbeeManager = nullptr;
DeviceRegistry& registry = DeviceRegistry::getInstance();
PlcMemory plcMemory;

void setup() {
    Serial.begin(115200);

    // Initialize MQTT
    mqttManager.begin("mqtt.local", 1883);  // Your MQTT broker
    mqttManager.setCallback(mqtt_callback);

    // Initialize DeviceRegistry
    plcMemory.begin();
    plcMemory.setDeviceRegistry(&registry);
    registry.setPlcMemory(&plcMemory);

    // Initialize ZigbeeManager
    zigbeeManager = new ZigbeeManager(&mqttManager, "living_room");
    zigbeeManager->begin();

    Serial.println("Setup complete");
}

void loop() {
    mqttManager.loop();
    zigbeeManager->loop();

    // Sync IO points periodically
    plcMemory.syncIOPoints();

    delay(100);
}

// MQTT callback - route Zigbee messages
void mqtt_callback(char* topic, byte* payload, unsigned int length) {
    String topicStr = String(topic);

    // Parse JSON payload
    JsonDocument doc;
    deserializeJson(doc, payload, length);

    // Route to ZigbeeManager if it's a zigbee2mqtt topic
    if (topicStr.startsWith("zigbee2mqtt/")) {
        zigbeeManager->handleMqttMessage(topicStr, doc.as<JsonObject>());
    }
}
```

### 2. Automatic Device Discovery

When Zigbee2MQTT publishes its device list, ZigbeeManager automatically:
1. Creates DeviceStatus entries
2. Registers all device endpoints
3. Maps Zigbee properties to PLC variables

Example Zigbee2MQTT device list message:
```json
{
  "type": "devices",
  "devices": [
    {
      "ieee_address": "0x00124b001f8e9abc",
      "friendly_name": "temp_sensor",
      "type": "EndDevice",
      "definition": {
        "model": "SNZB-02",
        "vendor": "SONOFF",
        "exposes": [
          {
            "type": "numeric",
            "name": "temperature",
            "property": "temperature",
            "unit": "°C",
            "access": 1
          },
          {
            "type": "numeric",
            "name": "humidity",
            "property": "humidity",
            "unit": "%",
            "access": 1
          }
        ]
      }
    }
  ]
}
```

This creates endpoints:
- `living_room.zigbee.temp_sensor.temperature.real`
- `living_room.zigbee.temp_sensor.humidity.real`

### 3. Manual Pairing

Enable pairing mode to add new devices:

```cpp
// Enable pairing for 60 seconds
zigbeeManager->enablePairing(60);

// Check pairing status
if (zigbeeManager->isPairingEnabled()) {
    Serial.println("Pairing mode active");
}

// Disable pairing
zigbeeManager->disablePairing();
```

### 4. Creating PLC IO Points

Map Zigbee endpoints to PLC variables:

```cpp
void setupPlcIOPoints() {
    // Declare PLC variables
    plcMemory.declareVariable("living_room_temp", PlcValueType::REAL);
    plcMemory.declareVariable("living_room_humidity", PlcValueType::REAL);
    plcMemory.declareVariable("kitchen_light", PlcValueType::BOOL);

    // Map INPUT: temperature sensor → PLC
    plcMemory.registerIOPoint(
        "living_room_temp",
        "living_room.zigbee.temp_sensor.temperature.real",
        IODirection::IO_INPUT,
        false,
        "",
        true  // Auto-sync
    );

    // Map INPUT: humidity sensor → PLC
    plcMemory.registerIOPoint(
        "living_room_humidity",
        "living_room.zigbee.temp_sensor.humidity.real",
        IODirection::IO_INPUT,
        false,
        "",
        true
    );

    // Map OUTPUT: PLC → light control
    plcMemory.registerIOPoint(
        "kitchen_light",
        "kitchen.zigbee.smart_bulb.state.bool",
        IODirection::IO_OUTPUT,
        false,
        "",
        true
    );

    Serial.println("PLC IO points configured");
}
```

### 5. Monitor Device Status with StatusHandlerBlock

```json
{
  "name": "zigbee_monitor",
  "variables": [
    {
      "name": "temp_sensor_endpoint",
      "type": "STRING",
      "init_value": "living_room.zigbee.temp_sensor.temperature.real"
    },
    {
      "name": "sensor_online",
      "type": "BOOL"
    },
    {
      "name": "sensor_offline_trigger",
      "type": "BOOL"
    },
    {
      "name": "low_battery_alert",
      "type": "BOOL"
    }
  ],
  "logic": [
    {
      "block_type": "StatusHandler",
      "inputs": {
        "endpoint_name": "temp_sensor_endpoint"
      },
      "outputs": {
        "is_online": "sensor_online",
        "on_offline": "sensor_offline_trigger"
      }
    }
  ]
}
```

### 6. Control Zigbee Devices

To control a Zigbee device, update the PLC variable and sync:

```cpp
// Turn on kitchen light
plcMemory.setValue<bool>("kitchen_light", true);
plcMemory.syncIOPoints();  // Updates endpoint value

// This will trigger MQTT publish to:
// Topic: zigbee2mqtt/smart_bulb/set
// Payload: {"state": "ON"}
```

## Device Update Flow

### Zigbee → PLC (INPUT)

1. Zigbee device sends update
2. Zigbee2MQTT publishes to `zigbee2mqtt/[device]/state`
3. ZigbeeManager receives MQTT message
4. Updates endpoint value in DeviceRegistry
5. `syncIOPoints()` copies to PLC variable
6. PLC logic can use the value

### PLC → Zigbee (OUTPUT)

1. PLC logic sets output variable
2. `syncIOPoints()` updates endpoint in DeviceRegistry
3. ZigbeeManager publishes to `zigbee2mqtt/[device]/set`
4. Zigbee2MQTT sends command to device
5. Device acknowledges with state update

## Supported Zigbee Device Types

ZigbeeManager automatically handles:

### Sensors
- Temperature/Humidity sensors (SNZB-02, Aqara, etc.)
- Motion sensors (PIR)
- Door/Window sensors
- Water leak sensors
- Smoke detectors

### Actuators
- Smart bulbs (on/off, dimming, color)
- Smart plugs
- Relays
- Thermostats
- Window covers

### Type Mapping

| Zigbee Type | PlcValueType | Example |
|-------------|--------------|---------|
| binary | BOOL | on/off, open/closed |
| numeric | REAL | temperature, humidity |
| enum | INT | mode selection |
| text | STRING_TYPE | status messages |

## Configuration Examples

### Temperature-based Automation

```cpp
// PLC program: turn on fan when temperature > 25°C
if (plcMemory.getValue<float>("living_room_temp") > 25.0f) {
    plcMemory.setValue<bool>("living_room_fan", true);
} else {
    plcMemory.setValue<bool>("living_room_fan", false);
}
plcMemory.syncIOPoints();
```

### Presence Detection

```cpp
// Declare variables
plcMemory.declareVariable("motion_detected", PlcValueType::BOOL);
plcMemory.declareVariable("lights_auto", PlcValueType::BOOL);

// Map motion sensor
plcMemory.registerIOPoint(
    "motion_detected",
    "hallway.zigbee.motion_sensor.occupancy.bool",
    IODirection::IO_INPUT
);

// Map lights
plcMemory.registerIOPoint(
    "lights_auto",
    "hallway.zigbee.ceiling_light.state.bool",
    IODirection::IO_OUTPUT
);

// In loop:
if (plcMemory.getValue<bool>("motion_detected")) {
    plcMemory.setValue<bool>("lights_auto", true);
    lastMotionTime = millis();
} else if (millis() - lastMotionTime > 300000) {  // 5 min timeout
    plcMemory.setValue<bool>("lights_auto", false);
}
```

## Troubleshooting

### Bridge Not Responding

```cpp
if (!zigbeeManager->isBridgeOnline()) {
    Serial.println("Zigbee2MQTT bridge is offline!");
    // Check MQTT connection
    // Check Zigbee2MQTT logs
}
```

### Device Not Appearing

1. Check if device is paired in Zigbee2MQTT
2. Enable debug logging:
   ```cpp
   EspHubLog->setLogLevel(LOG_DEBUG);
   ```
3. Request device list manually:
   ```cpp
   zigbeeManager->requestDeviceList();
   ```

### Endpoint Not Updating

1. Verify endpoint exists:
   ```cpp
   Endpoint* ep = registry.getEndpoint("living_room.zigbee.sensor.temp.real");
   if (!ep) {
       Serial.println("Endpoint not found!");
   }
   ```

2. Check if endpoint is online:
   ```cpp
   if (!ep->isOnline) {
       Serial.println("Endpoint offline!");
   }
   ```

3. Verify MQTT messages are being received

## Advanced: Custom Device Handlers

For devices with non-standard Zigbee2MQTT definitions:

```cpp
// Override default handling
class CustomZigbeeManager : public ZigbeeManager {
protected:
    void handleDeviceUpdate(const String& deviceName, const JsonObject& state) override {
        // Custom logic for specific devices
        if (deviceName == "custom_device") {
            // Parse custom format
            float customValue = state["my_property"];
            // Update endpoint manually
        } else {
            // Call base implementation
            ZigbeeManager::handleDeviceUpdate(deviceName, state);
        }
    }
};
```

## Next Steps

- Add Web UI for Zigbee device management
- Implement device grouping
- Add scene support
- Create Zigbee OTA update handler
