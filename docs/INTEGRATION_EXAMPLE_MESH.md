# Integration Example: Mesh Devices with DeviceRegistry

This document shows how to integrate the existing MeshDeviceManager with the new DeviceRegistry architecture.

## Overview

The new architecture provides:
- **DeviceRegistry**: Central registry for all endpoints across protocols
- **PlcMemory IO Points**: Automatic synchronization between endpoints and PLC variables
- **StatusHandlerBlock**: PLC block for handling device status changes

## Integration Steps

### 1. Register Mesh Devices in DeviceRegistry

When a mesh device is added via MeshDeviceManager, it should also be registered in DeviceRegistry:

```cpp
// In MeshDeviceManager::addDevice()
void MeshDeviceManager::addDevice(uint32_t nodeId, const String& name, const String& location) {
    // Existing MeshDeviceManager logic
    MeshDevice newDevice;
    newDevice.nodeId = nodeId;
    newDevice.name = name;
    newDevice.lastSeen = millis();
    newDevice.isOnline = true;
    devices[nodeId] = newDevice;

    // NEW: Register in DeviceRegistry
    DeviceRegistry& registry = DeviceRegistry::getInstance();

    // Register device status
    DeviceStatus deviceStatus;
    deviceStatus.deviceId = location + ".mesh." + name;
    deviceStatus.protocol = ProtocolType::MESH;
    deviceStatus.isOnline = true;
    deviceStatus.lastSeen = millis();
    deviceStatus.offlineThreshold = 60000; // 60 seconds
    registry.registerDevice(deviceStatus);

    // Register endpoints for this device
    // Example: GPIO endpoint
    Endpoint gpioEndpoint;
    gpioEndpoint.fullName = location + ".mesh." + name + ".gpio.bool";
    gpioEndpoint.location = location;
    gpioEndpoint.protocol = ProtocolType::MESH;
    gpioEndpoint.deviceId = location + ".mesh." + name;
    gpioEndpoint.endpoint = "gpio";
    gpioEndpoint.datatype = PlcValueType::BOOL;
    gpioEndpoint.isOnline = true;
    gpioEndpoint.lastSeen = millis();
    gpioEndpoint.isWritable = true;
    gpioEndpoint.mqttTopic = "esphub/mesh/" + name + "/gpio";
    registry.registerEndpoint(gpioEndpoint);

    EspHubLog->printf("Registered mesh device %s in DeviceRegistry\n", deviceStatus.deviceId.c_str());
}
```

### 2. Update Device Status on Heartbeat

```cpp
// In MeshDeviceManager::updateDeviceLastSeen()
void MeshDeviceManager::updateDeviceLastSeen(uint32_t nodeId, const String& location) {
    if (devices.count(nodeId)) {
        devices[nodeId].lastSeen = millis();

        // NEW: Update in DeviceRegistry
        DeviceRegistry& registry = DeviceRegistry::getInstance();
        String deviceId = location + ".mesh." + devices[nodeId].name;
        registry.updateDeviceStatus(deviceId, true); // Mark as online

        if (!devices[nodeId].isOnline) {
            devices[nodeId].isOnline = true;
            EspHubLog->printf("Device %s is back online\n", deviceId.c_str());
        }
    }
}
```

### 3. Check Offline Devices

```cpp
// In MeshDeviceManager::checkOfflineDevices()
void MeshDeviceManager::checkOfflineDevices(unsigned long offlineTimeoutMs, const String& location) {
    unsigned long currentMillis = millis();
    DeviceRegistry& registry = DeviceRegistry::getInstance();

    for (auto& pair : devices) {
        MeshDevice& device = pair.second;
        if (device.isOnline && (currentMillis - device.lastSeen > offlineTimeoutMs)) {
            device.isOnline = false;

            // NEW: Update in DeviceRegistry
            String deviceId = location + ".mesh." + device.name;
            registry.updateDeviceStatus(deviceId, false); // Mark as offline

            EspHubLog->printf("Device %s is offline\n", deviceId.c_str());
        }
    }
}
```

### 4. Create PLC IO Points

In EspHub main setup, create PLC variables and map them to endpoints:

```cpp
void setup() {
    // ... existing setup code ...

    // Get DeviceRegistry and PlcMemory instances
    DeviceRegistry& registry = DeviceRegistry::getInstance();
    PlcEngine* plcEngine = espHub.getPlcEngine();
    PlcMemory& memory = plcEngine->getProgram("main_program")->getMemory();

    // Connect PlcMemory to DeviceRegistry
    memory.setDeviceRegistry(&registry);

    // Declare PLC variables for mesh device
    memory.declareVariable("mesh_node1_gpio", PlcValueType::BOOL, false);
    memory.declareVariable("mesh_node1_online", PlcValueType::BOOL, false);
    memory.declareVariable("mesh_node1_online_trigger", PlcValueType::BOOL, false);
    memory.declareVariable("mesh_node1_offline_trigger", PlcValueType::BOOL, false);
    memory.declareVariable("mesh_node1_endpoint_name", PlcValueType::STRING_TYPE, false);

    // Set the endpoint name to monitor
    memory.setValue<String>("mesh_node1_endpoint_name", "garage.mesh.node1.gpio.bool");

    // Register IO point: mesh device GPIO as INPUT
    memory.registerIOPoint(
        "mesh_node1_gpio",                    // PLC variable
        "garage.mesh.node1.gpio.bool",        // Endpoint name
        IODirection::IO_INPUT,                 // Direction: read from device
        false,                                 // Not function-protected
        "",                                    // No function name
        true                                   // Auto-sync enabled
    );

    EspHubLog->println("PLC IO points configured for mesh devices");
}

void loop() {
    // ... existing loop code ...

    // Sync IO points periodically
    PlcMemory& memory = espHub.getPlcEngine()->getProgram("main_program")->getMemory();
    memory.syncIOPoints();
}
```

### 5. Use StatusHandlerBlock in PLC Program

Create a PLC program JSON that uses the StatusHandlerBlock:

```json
{
  "name": "mesh_monitor",
  "variables": [
    {
      "name": "mesh_node1_endpoint_name",
      "type": "STRING",
      "init_value": "garage.mesh.node1.gpio.bool"
    },
    {
      "name": "mesh_node1_online",
      "type": "BOOL",
      "init_value": false
    },
    {
      "name": "on_node1_online",
      "type": "BOOL",
      "init_value": false
    },
    {
      "name": "on_node1_offline",
      "type": "BOOL",
      "init_value": false
    },
    {
      "name": "notification_sent",
      "type": "BOOL",
      "init_value": false
    }
  ],
  "logic": [
    {
      "block_type": "StatusHandler",
      "inputs": {
        "endpoint_name": "mesh_node1_endpoint_name"
      },
      "outputs": {
        "is_online": "mesh_node1_online",
        "on_online": "on_node1_online",
        "on_offline": "on_node1_offline"
      }
    },
    {
      "block_type": "AND",
      "inputs": {
        "in1": "on_node1_offline",
        "in2": "notification_sent"
      },
      "outputs": {
        "out": "send_notification"
      }
    }
  ]
}
```

### 6. Handle Status Change Events

The StatusHandlerBlock provides three outputs:

1. **is_online** (BOOL): Current status - always reflects the current online/offline state
2. **on_online** (BOOL): Trigger - set to TRUE for one scan cycle when device goes online
3. **on_offline** (BOOL): Trigger - set to TRUE for one scan cycle when device goes offline

Use these triggers to execute event-driven logic:

```cpp
// In PLC scan cycle
void PlcProgram::executeScanCycle() {
    // Evaluate all blocks
    for (auto& block : logic_blocks) {
        block->evaluate(memory);
    }

    // Check for offline trigger
    if (memory.getValue<bool>("on_node1_offline", false)) {
        EspHubLog->println("ALERT: Mesh node 1 went offline!");
        // Send MQTT notification
        // Activate backup system
        // etc.
    }

    // Check for online trigger
    if (memory.getValue<bool>("on_node1_online", false)) {
        EspHubLog->println("INFO: Mesh node 1 is back online");
        // Restore normal operation
        // etc.
    }
}
```

## Testing

1. **Add a mesh device**:
   ```cpp
   meshManager->addDevice(12345, "node1", "garage");
   ```

2. **Simulate heartbeat**:
   ```cpp
   meshManager->updateDeviceLastSeen(12345, "garage");
   ```

3. **Simulate offline** (wait for timeout or manually call):
   ```cpp
   meshManager->checkOfflineDevices(60000, "garage");
   ```

4. **Monitor PLC variables**:
   ```cpp
   bool online = memory.getValue<bool>("mesh_node1_online", false);
   bool offline_trigger = memory.getValue<bool>("on_node1_offline", false);
   EspHubLog->printf("Node1 online: %d, offline_trigger: %d\n", online, offline_trigger);
   ```

## Benefits

✅ **Centralized Management**: All endpoints visible in one registry
✅ **Automatic Sync**: PLC variables auto-update from device endpoints
✅ **Event-Driven**: StatusHandlerBlock triggers on status changes
✅ **Protocol Agnostic**: Same pattern works for Zigbee, BLE, WiFi
✅ **MQTT Integration**: Auto-generated MQTT topics for all endpoints

## Next Steps

- Implement ZigbeeManager using the same pattern
- Add BleManager for Bluetooth devices
- Create WifiManager for WiFi-connected devices
- Add Web UI for device configuration
