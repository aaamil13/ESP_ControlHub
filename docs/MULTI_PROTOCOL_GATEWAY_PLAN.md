# Multi-Protocol IoT Gateway - Architecture Plan

## Общ преглед

EspHub се разширява до пълнофункционален Multi-Protocol IoT Gateway с PLC управление и защитен достъп до изходи.

## Основни принципи

1. **Йерархично именуване**: `{location}.{protocol}.{device}.{endpoint}.{datatype}`
2. **Статус проследяване**: Всички устройства и крайни точки имат онлайн/офлайн статус
3. **Защитен достъп**: Изходи се управляват само чрез функции, не директно
4. **PLC интеграция**: Всички променливи достъпни за PLC логика
5. **Event-driven**: PLC процедури се изпълняват при смяна на статус

---

## Фаза 1: Архитектура и базова структура

### 1.1 Йерархично именуване на точки

```
Формат: {location}.{protocol}.{device}.{endpoint}.{datatype}

Примери:
  - livingroom.zigbee.temp_sensor.temperature.float
  - kitchen.wifi.relay.switch1.bool
  - bedroom.ble.motion.state.bool
  - garage.mesh.esp_node.gpio2.bool
  - outdoor.zigbee.weather_station.humidity.float
```

### 1.2 Структури на данни

#### Endpoint Definition
```cpp
struct Endpoint {
    String fullName;           // kitchen.zigbee.relay.switch1.bool
    String location;           // kitchen
    String protocol;           // zigbee, ble, wifi, mesh
    String deviceId;           // relay
    String endpoint;           // switch1
    PlcValueType datatype;     // BOOL, INT, REAL, etc.
    bool isOnline;             // Device online status
    unsigned long lastSeen;    // Last communication timestamp
    bool isWritable;           // Input (read-only) vs Output (writable)
    String mqttTopic;          // Auto-generated MQTT topic
};
```

#### Device Status
```cpp
struct DeviceStatus {
    String deviceId;           // Full device path
    String protocol;           // Protocol type
    bool isOnline;             // Overall device status
    unsigned long lastSeen;    // Last seen timestamp
    uint32_t offlineThreshold; // Timeout before marking offline (ms)
    std::vector<String> endpoints; // List of endpoints
};
```

### 1.3 Базови класове

#### DeviceManager (Abstract Base)
```cpp
class DeviceManager {
public:
    virtual void begin() = 0;
    virtual void loop() = 0;
    virtual bool registerDevice(const String& deviceId, const JsonObject& config) = 0;
    virtual void removeDevice(const String& deviceId) = 0;
    virtual std::vector<DeviceStatus> getAllDevices() = 0;
    virtual void updateDeviceStatus(const String& deviceId, bool isOnline) = 0;

protected:
    String protocolName;
    std::map<String, DeviceStatus> devices;
    DeviceRegistry* registry;  // Reference to global registry
};
```

#### DeviceRegistry (Singleton)
```cpp
class DeviceRegistry {
public:
    static DeviceRegistry& getInstance();

    // Device management
    bool registerEndpoint(const Endpoint& endpoint);
    bool removeEndpoint(const String& fullName);
    Endpoint* getEndpoint(const String& fullName);
    std::vector<Endpoint> getAllEndpoints();
    std::vector<Endpoint> getEndpointsByProtocol(const String& protocol);
    std::vector<Endpoint> getEndpointsByLocation(const String& location);

    // Status management
    void updateEndpointStatus(const String& fullName, bool isOnline);
    void updateEndpointValue(const String& fullName, const PlcValue& value);

    // PLC Integration
    void syncToPLC(PlcMemory& memory);  // Auto-create PLC variables
    void syncFromPLC(PlcMemory& memory); // Read PLC outputs

    // Callbacks
    typedef std::function<void(const String&, bool)> StatusCallback;
    void onStatusChange(StatusCallback callback);

private:
    DeviceRegistry();
    std::map<String, Endpoint> endpoints;
    std::vector<StatusCallback> statusCallbacks;
    PlcMemory* plcMemory;
};
```

---

## Фаза 2: Protocol Managers

### 2.1 ZigbeeManager

**Интеграция**: Zigbee2MQTT чрез MQTT bridge

```cpp
class ZigbeeManager : public DeviceManager {
public:
    ZigbeeManager(MqttManager* mqtt);
    void begin() override;
    void loop() override;

    // Device discovery
    void enablePairing(uint32_t duration_sec);
    void disablePairing();

    // MQTT subscriptions
    void subscribeToBridge();
    void handleBridgeMessage(const String& topic, const JsonObject& payload);

    // Auto-discovery
    void discoverDevices();
    void registerZigbeeDevice(const String& ieeeAddr, const JsonObject& definition);

private:
    MqttManager* mqtt;
    String bridgeTopic;  // Default: "zigbee2mqtt/bridge"
    bool pairingEnabled;
};
```

**MQTT Topics**:
```
zigbee2mqtt/bridge/devices        - Device list
zigbee2mqtt/bridge/state          - Bridge status
zigbee2mqtt/{deviceName}          - Device data
zigbee2mqtt/{deviceName}/set      - Commands
zigbee2mqtt/{deviceName}/get      - State request
```

### 2.2 BleManager

```cpp
class BleManager : public DeviceManager {
public:
    BleManager();
    void begin() override;
    void loop() override;

    // Scanning
    void startScan(uint32_t duration_sec);
    void stopScan();

    // Device whitelist
    void addWhitelistDevice(const String& mac, const String& name);
    void removeWhitelistDevice(const String& mac);

    // Connection management
    bool connectToDevice(const String& mac);
    void disconnectDevice(const String& mac);

    // Characteristic discovery
    void discoverCharacteristics(const String& mac);

private:
    BLEScan* scanner;
    std::map<String, BLEClient*> clients;
    std::vector<String> whitelist;
};
```

**Supported BLE profiles**:
- Generic sensors (temperature, humidity, etc.)
- BLE beacon detection
- Custom GATT services

### 2.3 WiFiDeviceManager

За WiFi устройства извън painlessMesh (Tasmota, ESPHome, HTTP API)

```cpp
class WiFiDeviceManager : public DeviceManager {
public:
    WiFiDeviceManager();
    void begin() override;
    void loop() override;

    // Discovery
    void scanNetwork();  // mDNS/UDP discovery

    // Integration types
    void addTasmotaDevice(const String& ip, const String& name);
    void addESPHomeDevice(const String& ip, const String& name);
    void addHTTPDevice(const String& ip, const JsonObject& config);

    // Polling
    void pollDevices();

private:
    std::vector<String> discoveredDevices;
    uint32_t pollInterval;
};
```

---

## Фаза 3: PLC Memory Extensions

### 3.1 IO Point Definition

```cpp
enum IODirection {
    IO_INPUT,    // Read from external device
    IO_OUTPUT    // Write to external device
};

struct PlcIOPoint {
    String plcVarName;         // PLC variable name
    String mappedEndpoint;     // kitchen.zigbee.relay.switch1.bool
    IODirection direction;     // INPUT or OUTPUT
    bool requiresFunction;     // For outputs - requires function call
    String functionName;       // Required function for this output
    bool autoSync;             // Auto-sync with endpoint
};
```

### 3.2 PlcMemory Extensions

```cpp
class PlcMemory {
    // Existing methods...

    // IO Point management
    bool registerIOPoint(const String& plcVarName,
                        const String& endpoint,
                        IODirection direction,
                        bool requiresFunction = false);

    bool unregisterIOPoint(const String& plcVarName);

    PlcIOPoint* getIOPoint(const String& plcVarName);
    std::vector<PlcIOPoint> getAllIOPoints();

    // Status queries
    bool isEndpointOnline(const String& endpoint);
    unsigned long getEndpointLastSeen(const String& endpoint);

    // Auto-sync
    void syncIOPoints();  // Called in PLC loop

private:
    std::map<String, PlcIOPoint> ioPoints;
    DeviceRegistry* registry;
};
```

### 3.3 Status Event Handlers

```cpp
// PLC configuration JSON
{
  "status_handlers": [
    {
      "endpoint_pattern": "kitchen.zigbee.relay.*",
      "on_offline": {
        "action": "call_procedure",
        "procedure": "handle_relay_offline",
        "params": {
          "location": "$location",
          "device": "$device"
        }
      },
      "on_online": {
        "action": "call_procedure",
        "procedure": "handle_relay_online"
      }
    },
    {
      "endpoint_pattern": "*.*.*.temperature.float",
      "on_offline": {
        "action": "set_variable",
        "variable": "temp_sensor_fault",
        "value": true
      }
    }
  ]
}
```

**Примерна PLC процедура**:
```cpp
// Auto-generated PLC block for status handling
class StatusHandlerBlock : public PlcBlock {
    String endpointPattern;
    String onOfflineProcedure;
    String onOnlineProcedure;

    void evaluate(PlcMemory& memory) override {
        // Check all matching endpoints
        // Call procedures on status change
    }
};
```

---

## Фаза 4: MQTT Function Registry

### 4.1 Function Definition

```cpp
struct MqttFunction {
    String name;               // "turn_on_kitchen_light"
    String description;        // User-friendly description
    String plcProcedure;       // PLC procedure to call
    std::vector<String> outputs; // Affected outputs
    UserRole minRole;          // Minimum required role
    uint32_t cooldownMs;       // Prevent spam
    unsigned long lastCalled;  // Last execution timestamp
    JsonObject params;         // Default parameters
};
```

### 4.2 MqttFunctionRegistry

```cpp
class MqttFunctionRegistry {
public:
    MqttFunctionRegistry(MqttManager* mqtt, PlcEngine* plc, UserManager* users);

    void begin();

    // Function management
    bool registerFunction(const MqttFunction& function);
    bool unregisterFunction(const String& name);
    MqttFunction* getFunction(const String& name);

    // Execution
    bool executeFunction(const String& name,
                        const String& username,
                        const JsonObject& params = JsonObject());

    // MQTT integration
    void subscribeFunctions();
    void handleFunctionCall(const String& topic, const JsonObject& payload);

    // Publishing
    void publishFunctionList();
    void publishFunctionResult(const String& name, bool success, const String& message);

private:
    MqttManager* mqtt;
    PlcEngine* plc;
    UserManager* users;
    std::map<String, MqttFunction> functions;

    bool validatePermissions(const String& username, const MqttFunction& func);
    bool checkCooldown(const MqttFunction& func);
};
```

### 4.3 MQTT Topics

```
esphub/functions/list                    - Published function list
esphub/function/call                     - Execute function
  Payload: {
    "name": "turn_on_kitchen_light",
    "user": "admin",
    "token": "auth_token",
    "params": {}
  }

esphub/function/result/{name}            - Execution result
  Payload: {
    "success": true,
    "message": "Kitchen light turned on",
    "timestamp": 1234567890
  }
```

### 4.4 Function Configuration

```json
{
  "mqtt_functions": [
    {
      "name": "turn_on_kitchen_light",
      "description": "Turn on kitchen main light",
      "plc_procedure": "kitchen_light_on",
      "outputs": ["kitchen.zigbee.relay.switch1"],
      "min_role": "operator",
      "cooldown_ms": 1000
    },
    {
      "name": "emergency_stop",
      "description": "Emergency stop all outputs",
      "plc_procedure": "emergency_stop_all",
      "outputs": ["*.*.*.*"],
      "min_role": "admin",
      "cooldown_ms": 0,
      "confirmation_required": true
    },
    {
      "name": "set_thermostat",
      "description": "Set thermostat temperature",
      "plc_procedure": "set_thermostat_temp",
      "outputs": [],
      "min_role": "operator",
      "params": {
        "temperature": {
          "type": "float",
          "min": 15.0,
          "max": 30.0,
          "default": 22.0
        },
        "location": {
          "type": "string",
          "enum": ["livingroom", "bedroom", "kitchen"]
        }
      }
    }
  ]
}
```

---

## Фаза 5: Web UI Pages

### 5.1 Device Discovery & Pairing

#### /devices/zigbee
```html
- Device list with status
- Pairing mode toggle
- Real-time device discovery
- Endpoint mapping interface
- Force refresh button
```

#### /devices/ble
```html
- BLE scanning interface
- Whitelist management
- Connection status
- Characteristic explorer
- Signal strength indicator
```

#### /devices/wifi
```html
- Network scanner
- Manual device addition
- Integration type selector (Tasmota/ESPHome/Custom)
- Connection test
- Endpoint configuration
```

#### /devices/all
```html
Unified view:
- All devices across protocols
- Status indicators
- Last seen timestamps
- Quick actions (refresh, remove)
- Export configuration
```

### 5.2 Endpoint Mapping - /plc/io_mapping

```html
Features:
- Drag & drop endpoint to PLC variable
- Auto-suggest PLC variable names
- Direction selection (input/output)
- Function requirement toggle
- Wildcard patterns
- Bulk operations
- Import/Export mappings

UI Layout:
┌─────────────────────┬─────────────────────┐
│ Available Endpoints │  PLC Variables      │
│                     │                     │
│ 📍 livingroom       │  [M] temp_living    │
│   └ zigbee          │  [M] relay_kitchen  │
│     └ temp_sensor   │  [ ] spare_input_1  │
│       └ temp.float  │                     │
│                     │  [+] Add Variable   │
│ 🔍 Search           │                     │
└─────────────────────┴─────────────────────┘

Mapping Rules:
- Input endpoints → Read-only PLC vars
- Output endpoints → Writable PLC vars with function protection
```

### 5.3 Function Manager - /mqtt/functions

```html
Function List:
┌──────────────────────────────────────────┐
│ Function Name          | Status | Actions│
│ turn_on_kitchen_light | ✓     | ⚙️ 🗑️  │
│ emergency_stop        | ✓     | ⚙️ 🗑️  │
│ set_thermostat        | ✓     | ⚙️ 🗑️  │
├──────────────────────────────────────────┤
│ [+] Add New Function                     │
└──────────────────────────────────────────┘

Function Editor:
- Name & description
- PLC procedure selector
- Output selection (with wildcard support)
- Permission level
- Cooldown setting
- Parameter definition (type, validation, default)
- Test execution panel
- MQTT topic preview
```

### 5.4 Status Handlers - /plc/status_handlers

```html
Handler Configuration:
- Endpoint pattern matcher
- Online/Offline actions
- Procedure selector
- Variable manipulation
- Notification settings
- Test simulation
```

---

## Фаза 6: Implementation Timeline

### Week 1: Foundation
**Days 1-3: Core Structure** ✅ COMPLETED
- [x] Create Endpoint and DeviceStatus structs
- [x] Implement DeviceRegistry singleton
- [x] Add DeviceManager abstract base class
- [x] Extend PlcMemory with IO point support
- [x] Update PlcMemory to track endpoint status

**Days 4-5: PLC Integration** ✅ COMPLETED
- [x] Implement auto-sync between endpoints and PLC
- [x] Add status change callbacks
- [x] Create StatusHandlerBlock
- [x] Create integration example with MeshDeviceManager

**Days 6-7: Testing & Documentation**
- [ ] Unit tests for core structures
- [ ] Integration tests
- [ ] API documentation

### Week 2: Zigbee Integration
**Days 1-2: ZigbeeManager**
- [ ] Implement ZigbeeManager class
- [ ] MQTT bridge subscription
- [ ] Device discovery
- [ ] Endpoint auto-registration

**Days 3-4: Web UI**
- [ ] /devices/zigbee page
- [ ] Pairing interface
- [ ] Real-time device updates via WebSocket
- [ ] Device configuration

**Day 5: Testing**
- [ ] Test with real Zigbee coordinator
- [ ] Device pairing workflow
- [ ] PLC variable auto-creation
- [ ] Status monitoring

### Week 3: BLE & WiFi
**Days 1-3: BleManager**
- [ ] Implement BleManager
- [ ] Scanning & whitelisting
- [ ] Characteristic discovery
- [ ] Web UI (/devices/ble)

**Days 4-5: WiFiDeviceManager**
- [ ] HTTP/REST integration
- [ ] Tasmota support
- [ ] ESPHome support
- [ ] Web UI (/devices/wifi)

### Week 4: Function Registry & Advanced Features
**Days 1-3: MqttFunctionRegistry**
- [ ] Function definition & storage
- [ ] Permission validation
- [ ] MQTT integration
- [ ] Cooldown management
- [ ] Parameter validation

**Days 4-5: Web UI**
- [ ] /mqtt/functions page
- [ ] Function editor
- [ ] Test execution panel
- [ ] /plc/io_mapping interface

**Days 6-7: Polish & Testing**
- [ ] Full system integration test
- [ ] Performance optimization
- [ ] Security audit
- [ ] Documentation update

---

## Security Considerations

### Authentication & Authorization
1. **User roles**: Admin, Operator, Monitor
2. **Token-based auth** for MQTT function calls
3. **Rate limiting** on function execution
4. **Audit logging** for all output changes

### Output Protection
1. **Function-only access**: Direct output write blocked
2. **Confirmation required** for critical functions
3. **Cooldown periods** prevent spam
4. **Interlock support**: PLC can block unsafe operations

### Network Security
1. **MQTT TLS/SSL** support (already implemented)
2. **Client certificates** for authentication
3. **Endpoint validation**: Only whitelisted devices
4. **Protocol isolation**: Each protocol manager sandboxed

---

## Performance Considerations

### Memory Management
- **Endpoint limit**: ~100 endpoints per device
- **Dynamic allocation**: Use std::vector with reserve()
- **Cache optimization**: Frequently accessed endpoints cached
- **NVS storage**: Persist device configuration

### Timing
- **PLC cycle**: Target 50-100ms max
- **Status updates**: 1-5 second intervals
- **MQTT QoS**: Use QoS 1 for critical messages
- **Watchdog**: Monitor for stuck protocol managers

### Scalability
- **Multi-core**: Protocol managers on Core 0, PLC on Core 1
- **Queue-based**: Async message processing
- **Batch operations**: Group MQTT publishes
- **Lazy loading**: Only active endpoints in memory

---

## Testing Strategy

### Unit Tests
- Endpoint naming validation
- Status tracking accuracy
- Permission checking
- Function parameter validation

### Integration Tests
- Multi-protocol device registration
- PLC variable auto-creation
- Status change callbacks
- MQTT function execution
- Web UI workflows

### Performance Tests
- 50+ endpoints simultaneous
- Rapid status changes
- Function spam protection
- Memory leak detection

### Real-world Tests
- Zigbee device pairing
- BLE beacon detection
- WiFi device polling
- PLC procedure execution on events
- MQTT function calls from Home Assistant

---

## Future Enhancements

### Phase 2 (Post-MVP)
- **Modbus TCP/RTU support**
- **KNX integration**
- **LoRaWAN gateway**
- **Matter/Thread support**
- **Graphical PLC editor** (web-based ladder/FBD)
- **Trend logging** & visualization
- **Alarm management** system
- **Recipe management** for complex sequences
- **Remote access** via secure tunnel
- **Mobile app** (Flutter/React Native)

### Advanced Features
- **Machine learning** for predictive maintenance
- **Energy monitoring** & optimization
- **Voice control** integration (Alexa/Google)
- **Backup/restore** full configuration
- **Multi-hub synchronization**
- **Cloud integration** (AWS IoT, Azure IoT)

---

## Configuration File Structure

### Main configuration
```json
{
  "system": {
    "device_name": "EspHub Gateway",
    "location": "Home",
    "timezone": "Europe/Sofia"
  },

  "protocols": {
    "zigbee": {
      "enabled": true,
      "mqtt_bridge": "zigbee2mqtt",
      "coordinator_ieee": "00:11:22:33:44:55:66:77"
    },
    "ble": {
      "enabled": true,
      "scan_interval": 30,
      "whitelist_only": true
    },
    "wifi": {
      "enabled": true,
      "discovery_enabled": true,
      "poll_interval": 5
    },
    "mesh": {
      "enabled": true,
      "password": "mesh_password"
    }
  },

  "io_mappings": [
    {
      "endpoint": "livingroom.zigbee.temp_sensor.temperature.float",
      "plc_variable": "temp_living",
      "direction": "input"
    },
    {
      "endpoint": "kitchen.zigbee.relay.switch1.bool",
      "plc_variable": "relay_kitchen",
      "direction": "output",
      "requires_function": true,
      "function": "turn_on_kitchen_light"
    }
  ],

  "status_handlers": [...],
  "mqtt_functions": [...]
}
```

---

## Conclusion

Този план предоставя пълна архитектура за Multi-Protocol IoT Gateway с:
- ✅ Универсално именуване на устройства
- ✅ Централизиран регистър
- ✅ PLC интеграция
- ✅ Защитено управление на изходи
- ✅ Event-driven архитектура
- ✅ Разширяема структура
- ✅ Web-базиран UI
- ✅ MQTT API

Готови за имплементация! 🚀
