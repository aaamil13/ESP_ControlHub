# Unified Device Architecture

## Overview

EspHub използва **unified JSON-based device system** където всички устройства, независимо от протокола, се дефинират и управляват еднакво.

## Architecture Layers

```
┌─────────────────────────────────────────────────┐
│         Web UI (Single Device Page)             │
│  - List all devices (all protocols)             │
│  - Add/Delete devices                           │
│  - Device detail view (IO points)               │
└──────────────────┬──────────────────────────────┘
                   │
┌──────────────────▼──────────────────────────────┐
│       DeviceConfigManager (Central)             │
│  - Load/Save JSON configurations                │
│  - Unified device registry                      │
│  - Route operations to protocol managers        │
└──────────────────┬──────────────────────────────┘
                   │
        ┌──────────┴──────────┐
        │                     │
┌───────▼────────┐   ┌───────▼────────┐
│ Protocol       │   │ Protocol       │
│ Managers       │   │ Managers       │
│ (Transport)    │   │ (Transport)    │
│                │   │                │
│ - RF433        │   │ - BLE          │
│ - Zigbee       │   │ - WiFi         │
│ - Mesh         │   │ - Modbus       │
└────────────────┘   └────────────────┘
        │                     │
        └──────────┬──────────┘
                   │
        ┌──────────▼──────────┐
        │  DeviceRegistry     │
        │  (IO Points)        │
        └─────────────────────┘
```

## Key Concepts

### 1. Separation of Concerns

**DeviceConfigManager**
- Централно управление на device configurations
- JSON parsing и validation
- Device lifecycle (create, update, delete)
- Делегиране към правилния Protocol Manager

**Protocol Managers** (Transport Layer Only)
- Само communication protocol implementation
- Не знаят за device types
- Получават готови команди от DeviceConfigManager
- Примери: `sendRF433Code()`, `bleReadCharacteristic()`, `httpRequest()`

**DeviceRegistry**
- Unified storage на всички IO points
- Не знае за transport protocols
- Предоставя IO points на PLC

### 2. Unified JSON Schema

Всички устройства използват същата базова структура:

```json
{
  "device_id": "unique_id",
  "friendly_name": "Display Name",
  "location": "room",
  "protocol": "wifi|ble|rf433|zigbee|mesh|modbus",
  "connection": {
    /* Protocol-specific connection config */
  },
  "endpoints": [
    {
      "name": "temperature",
      "type": "real",
      "access": "r|w|rw",
      /* Protocol-specific endpoint config */
    }
  ],
  "metadata": {
    "manufacturer": "...",
    "model": "...",
    "tags": ["sensor", "temperature"]
  }
}
```

## Protocol-Specific Connection Configs

### WiFi Devices

```json
{
  "protocol": "wifi",
  "connection": {
    "type": "http",
    "host": "192.168.1.100",
    "port": 80,
    "auth": {
      "username": "admin",
      "password": "pass"
    }
  },
  "endpoints": [
    {
      "name": "relay",
      "type": "bool",
      "access": "rw",
      "read": {
        "method": "GET",
        "path": "/status",
        "value_path": "relay.state"
      },
      "write": {
        "method": "POST",
        "path": "/control",
        "body_template": "{\"relay\":\"{{value}}\"}"
      }
    }
  ]
}
```

### BLE Devices

```json
{
  "protocol": "ble",
  "connection": {
    "mac_address": "AA:BB:CC:DD:EE:FF",
    "service_uuid": "ebe0ccb0-7a0a-4b0c-8a1a-6ff2997da3a6"
  },
  "endpoints": [
    {
      "name": "temperature",
      "type": "real",
      "access": "r",
      "characteristic_uuid": "ebe0ccc1-7a0a-4b0c-8a1a-6ff2997da3a6",
      "format": "int16",
      "offset": 0,
      "scale": 0.01
    }
  ]
}
```

### RF433 Devices

```json
{
  "protocol": "rf433",
  "connection": {
    "rx_pin": 27,
    "tx_pin": 26
  },
  "endpoints": [
    {
      "name": "relay",
      "type": "bool",
      "access": "rw",
      "rf_codes": {
        "on": 5393,
        "off": 5396
      },
      "protocol": 1,
      "bit_length": 24,
      "pulse_length": 189
    }
  ]
}
```

### Zigbee Devices

```json
{
  "protocol": "zigbee",
  "connection": {
    "type": "mqtt",
    "bridge_topic": "zigbee2mqtt",
    "ieee_address": "0x00158d0001a2b3c4",
    "friendly_name": "temp_sensor_1"
  },
  "endpoints": [
    {
      "name": "temperature",
      "type": "real",
      "access": "r",
      "mqtt_topic": "zigbee2mqtt/temp_sensor_1",
      "value_path": "temperature"
    },
    {
      "name": "humidity",
      "type": "int",
      "access": "r",
      "mqtt_topic": "zigbee2mqtt/temp_sensor_1",
      "value_path": "humidity"
    }
  ]
}
```

### Mesh Devices

```json
{
  "protocol": "mesh",
  "connection": {
    "node_id": 12345678
  },
  "endpoints": [
    {
      "name": "gpio_output",
      "type": "bool",
      "access": "rw",
      "gpio_pin": 2
    }
  ]
}
```

## Web UI Design

### Main Device List Page (`/devices`)

```html
┌─────────────────────────────────────────┐
│ EspHub Devices                    [+Add]│
├─────────────────────────────────────────┤
│                                         │
│ 📡 RF433 Switch 1         Living Room   │
│    Protocol: RF433         [View][Del]  │
│                                         │
│ 🌡️  Xiaomi Temp           Bedroom      │
│    Protocol: BLE           [View][Del]  │
│                                         │
│ 💡 Sonoff Basic           Kitchen       │
│    Protocol: WiFi          [View][Del]  │
│                                         │
│ 🔌 Zigbee Plug            Office        │
│    Protocol: Zigbee        [View][Del]  │
│                                         │
└─────────────────────────────────────────┘
```

### Device Detail View (`/device/{id}`)

```html
┌─────────────────────────────────────────┐
│ ← Back to Devices                       │
├─────────────────────────────────────────┤
│ RF433 Switch 1                          │
│ Living Room • RF433 • Online            │
├─────────────────────────────────────────┤
│ Connection Info:                        │
│  • RX Pin: GPIO 27                      │
│  • TX Pin: GPIO 26                      │
│                                         │
│ IO Points:                              │
│  relay (bool, rw)                       │
│    Current: ON                          │
│    RF Code ON: 5393                     │
│    RF Code OFF: 5396                    │
│    [Toggle] [Test]                      │
│                                         │
│ Metadata:                               │
│  • Manufacturer: Generic                │
│  • Model: 433MHz Switch                 │
│  • Tags: switch, rf433                  │
│                                         │
│ [Edit Config] [Delete Device]           │
└─────────────────────────────────────────┘
```

## DeviceConfigManager API

### C++ Interface

```cpp
class DeviceConfigManager {
public:
    // Device lifecycle
    bool loadDevice(const JsonObject& config);
    bool loadDeviceFromFile(const String& filepath);
    bool loadAllDevices(); // Load from /config/devices/
    bool saveDevice(const String& deviceId);
    bool deleteDevice(const String& deviceId);

    // Device access
    JsonDocument getDeviceConfig(const String& deviceId);
    std::vector<String> getAllDeviceIds();
    std::vector<String> getDevicesByProtocol(const String& protocol);
    std::vector<String> getDevicesByLocation(const String& location);

    // IO operations (delegate to protocol managers)
    bool readEndpoint(const String& deviceId, const String& endpoint);
    bool writeEndpoint(const String& deviceId, const String& endpoint, const PlcValue& value);

    // Testing
    bool testDevice(const String& deviceId);
    bool testEndpoint(const String& deviceId, const String& endpoint);

private:
    std::map<String, JsonDocument> deviceConfigs;

    // Protocol managers (registered)
    std::map<String, ProtocolManagerInterface*> protocolManagers;
};
```

### Protocol Manager Interface

```cpp
class ProtocolManagerInterface {
public:
    virtual bool initialize(const JsonObject& connectionConfig) = 0;
    virtual bool readEndpoint(const JsonObject& endpointConfig, PlcValue& value) = 0;
    virtual bool writeEndpoint(const JsonObject& endpointConfig, const PlcValue& value) = 0;
    virtual bool testConnection(const JsonObject& connectionConfig) = 0;
    virtual void loop() = 0;
};
```

## REST API Endpoints

### Device Management

```
GET    /api/devices
       Returns: List of all devices (all protocols)

GET    /api/devices/{id}
       Returns: Full device config + current state

POST   /api/devices
       Body: JSON device config
       Creates new device

PUT    /api/devices/{id}
       Body: Updated JSON config
       Updates device config

DELETE /api/devices/{id}
       Removes device

POST   /api/devices/test
       Body: JSON device config
       Tests device without saving
```

### Device Operations

```
GET    /api/devices/{id}/endpoints
       Returns: List of device endpoints with current values

GET    /api/devices/{id}/endpoints/{endpoint}
       Returns: Endpoint current value

POST   /api/devices/{id}/endpoints/{endpoint}
       Body: {"value": <value>}
       Writes to endpoint

POST   /api/devices/{id}/test
       Tests device connection
```

### Filtering

```
GET    /api/devices?protocol=wifi
       Returns: Only WiFi devices

GET    /api/devices?location=kitchen
       Returns: Only kitchen devices

GET    /api/devices?tag=sensor
       Returns: Devices with "sensor" tag
```

## File Structure

```
/config/
  devices/
    rf_switch_1.json
    xiaomi_temp_1.json
    sonoff_basic_1.json
    zigbee_plug_1.json
  templates/
    tasmota_switch.json
    xiaomi_lywsd03mmc.json
    rf433_generic_switch.json
```

## Benefits

1. **Unified UI** - Една страница за всички устройства
2. **Protocol Abstraction** - IO points независими от transport
3. **Easy Extension** - Добавяй нови протоколи без UI промени
4. **Configuration as Code** - JSON файлове могат да се version control
5. **Template Library** - Преизползваеми device definitions
6. **Testing Isolation** - Тествай device config преди deployment
7. **Multi-Hub** - Един конфигуратор за множество hub-ове

## Implementation Order

1. ✅ WiFiDeviceManager (already JSON-based)
2. 🔄 DeviceConfigManager (central coordinator)
3. 🔄 Update RF433Manager to use JSON
4. 🔄 Update ZigbeeManager to use JSON
5. 🔄 Create unified Web UI
6. 🔄 REST API endpoints
7. 🔄 BLEManager (JSON-based from start)
8. 🔄 External Configurator integration

## Migration Path

Existing code:
- WiFiDeviceManager вече е JSON-based ✅
- ZigbeeManager има hardcoded Zigbee2MQTT - трябва JSON refactor
- RF433Manager има hardcoded registration - трябва JSON refactor
- MeshDeviceManager - трябва JSON support

Next steps:
1. Create DeviceConfigManager
2. Create ProtocolManagerInterface
3. Refactor existing managers to implement interface
4. Create unified web UI
