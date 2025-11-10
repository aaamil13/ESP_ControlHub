# Device JSON Schema

## Overview

EspHub използва JSON-based конфигурация за дефиниране на устройства. Това позволява гъвкаво добавяне на нови типове устройства без промяна на firmware кода.

## Architecture

```
┌─────────────────────────────────────┐
│   External Web Configurator         │
│   (Rich UI - React/Vue/Angular)     │
│   - Visual device builder           │
│   - Template library                │
│   - Testing tools                   │
└─────────────┬───────────────────────┘
              │ JSON Config
              ▼
┌─────────────────────────────────────┐
│   ESP32 Hub (Embedded in iframe)    │
│   - Load config via HTTP POST       │
│   - Parse JSON definitions          │
│   - Create endpoints dynamically    │
│   - Register in DeviceRegistry      │
└─────────────────────────────────────┘
```

## Device Definition Schema

### Full Device Configuration

```json
{
  "device_id": "sonoff_basic_1",
  "friendly_name": "Living Room Light",
  "location": "living_room",
  "protocol": "wifi",
  "connection": {
    "type": "http",
    "host": "192.168.1.100",
    "port": 80,
    "auth": {
      "type": "basic",
      "username": "admin",
      "password": "password"
    }
  },
  "endpoints": [
    {
      "name": "relay",
      "type": "bool",
      "access": "rw",
      "polling_interval": 5000,
      "read": {
        "method": "GET",
        "path": "/cm?cmnd=Status",
        "response_format": "json",
        "value_path": "StatusSTS.POWER",
        "value_map": {
          "ON": true,
          "OFF": false
        }
      },
      "write": {
        "method": "GET",
        "path": "/cm?cmnd=Power%20{{value}}",
        "value_format": {
          "true": "On",
          "false": "Off"
        }
      }
    },
    {
      "name": "rssi",
      "type": "int",
      "access": "r",
      "polling_interval": 30000,
      "read": {
        "method": "GET",
        "path": "/cm?cmnd=Status%205",
        "response_format": "json",
        "value_path": "StatusNET.Wifi.RSSI"
      }
    }
  ],
  "metadata": {
    "manufacturer": "ITEAD",
    "model": "Sonoff Basic",
    "firmware": "Tasmota 12.5.0",
    "tags": ["light", "switch", "tasmota"]
  }
}
```

## Schema Components

### 1. Device Identification

```json
{
  "device_id": "unique_device_identifier",
  "friendly_name": "Human readable name",
  "location": "room_or_area"
}
```

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `device_id` | string | Yes | Unique identifier |
| `friendly_name` | string | No | Display name |
| `location` | string | Yes | Physical location |

### 2. Protocol

```json
{
  "protocol": "wifi"
}
```

Supported values: `wifi`, `ble`, `rf433`, `zigbee`, `mesh`, `modbus`

### 3. Connection Configuration

#### HTTP/HTTPS Connection

```json
{
  "connection": {
    "type": "http",
    "host": "192.168.1.100",
    "port": 80,
    "use_ssl": false,
    "auth": {
      "type": "basic",
      "username": "admin",
      "password": "secret"
    }
  }
}
```

#### MQTT Connection

```json
{
  "connection": {
    "type": "mqtt",
    "broker": "mqtt://192.168.1.50:1883",
    "topic_prefix": "home/devices/sensor1",
    "auth": {
      "username": "mqtt_user",
      "password": "mqtt_pass"
    }
  }
}
```

#### BLE Connection

```json
{
  "connection": {
    "type": "ble",
    "mac_address": "AA:BB:CC:DD:EE:FF",
    "service_uuid": "0000180f-0000-1000-8000-00805f9b34fb"
  }
}
```

### 4. Endpoints

Each endpoint represents a readable/writable data point:

```json
{
  "endpoints": [
    {
      "name": "temperature",
      "type": "real",
      "access": "r",
      "unit": "°C",
      "polling_interval": 10000,
      "read": { /* read config */ }
    }
  ]
}
```

#### Endpoint Types

| Type | PLC Type | Description |
|------|----------|-------------|
| `bool` | BOOL | Boolean on/off |
| `int` | INT | Integer number |
| `real` | REAL | Floating point |
| `string` | STRING | Text value |

#### Access Modes

| Mode | Description |
|------|-------------|
| `r` | Read-only |
| `w` | Write-only |
| `rw` | Read and write |

### 5. Read Configuration

#### JSON Response

```json
{
  "read": {
    "method": "GET",
    "path": "/api/sensor/temperature",
    "response_format": "json",
    "value_path": "data.temperature",
    "headers": {
      "Authorization": "Bearer token123"
    }
  }
}
```

#### Plain Text Response

```json
{
  "read": {
    "method": "GET",
    "path": "/temperature.txt",
    "response_format": "text",
    "regex": "Temp: ([0-9.]+)°C"
  }
}
```

#### MQTT Subscription

```json
{
  "read": {
    "method": "SUBSCRIBE",
    "topic": "sensors/temp1/temperature",
    "response_format": "json",
    "value_path": "value"
  }
}
```

### 6. Write Configuration

#### HTTP POST

```json
{
  "write": {
    "method": "POST",
    "path": "/api/control/relay",
    "content_type": "application/json",
    "body_template": "{\"state\": \"{{value}}\"}"
  }
}
```

#### HTTP GET with Query Parameters

```json
{
  "write": {
    "method": "GET",
    "path": "/control?relay={{value}}"
  }
}
```

#### MQTT Publish

```json
{
  "write": {
    "method": "PUBLISH",
    "topic": "devices/relay1/set",
    "payload_template": "{{value}}"
  }
}
```

## Device Templates Library

### Tasmota Switch

```json
{
  "template_id": "tasmota_switch",
  "name": "Tasmota Switch/Relay",
  "protocol": "wifi",
  "connection": {
    "type": "http",
    "port": 80
  },
  "endpoints": [
    {
      "name": "power",
      "type": "bool",
      "access": "rw",
      "read": {
        "method": "GET",
        "path": "/cm?cmnd=Status",
        "response_format": "json",
        "value_path": "StatusSTS.POWER",
        "value_map": {"ON": true, "OFF": false}
      },
      "write": {
        "method": "GET",
        "path": "/cm?cmnd=Power%20{{value}}",
        "value_format": {"true": "On", "false": "Off"}
      }
    }
  ]
}
```

### ESPHome Binary Sensor

```json
{
  "template_id": "esphome_binary_sensor",
  "name": "ESPHome Binary Sensor",
  "protocol": "wifi",
  "connection": {
    "type": "http",
    "port": 80
  },
  "endpoints": [
    {
      "name": "state",
      "type": "bool",
      "access": "r",
      "polling_interval": 5000,
      "read": {
        "method": "GET",
        "path": "/sensor/{{sensor_id}}",
        "response_format": "json",
        "value_path": "state"
      }
    }
  ]
}
```

### Xiaomi Mi Temperature Sensor (BLE)

```json
{
  "template_id": "xiaomi_lywsd03mmc",
  "name": "Xiaomi Mi Temperature & Humidity Sensor",
  "protocol": "ble",
  "connection": {
    "type": "ble",
    "service_uuid": "ebe0ccb0-7a0a-4b0c-8a1a-6ff2997da3a6"
  },
  "endpoints": [
    {
      "name": "temperature",
      "type": "real",
      "access": "r",
      "unit": "°C",
      "read": {
        "characteristic_uuid": "ebe0ccc1-7a0a-4b0c-8a1a-6ff2997da3a6",
        "format": "int16",
        "scale": 0.01
      }
    },
    {
      "name": "humidity",
      "type": "int",
      "access": "r",
      "unit": "%",
      "read": {
        "characteristic_uuid": "ebe0ccc1-7a0a-4b0c-8a1a-6ff2997da3a6",
        "format": "uint8",
        "offset": 2
      }
    }
  ]
}
```

## External Configurator Integration

### Loading Configurator

ESP32 Hub web interface може да зареди външния конфигуратор в iframe:

```html
<iframe
  id="device-configurator"
  src="https://esphub-configurator.example.com"
  sandbox="allow-scripts allow-same-origin"
  style="width: 100%; height: 600px; border: none;">
</iframe>
```

### PostMessage Communication

Комуникация между Hub и Configurator:

#### Hub → Configurator: Request Current Config

```javascript
// Hub sends
configuratorFrame.postMessage({
  type: 'REQUEST_CONFIG',
  hubId: 'esp32_kitchen'
}, 'https://esphub-configurator.example.com');
```

#### Configurator → Hub: Send Device Config

```javascript
// Configurator sends
parent.postMessage({
  type: 'DEVICE_CONFIG',
  config: {
    device_id: 'sonoff_1',
    // ... full device config
  }
}, '*');
```

#### Hub → Configurator: Config Save Confirmation

```javascript
// Hub confirms
configuratorFrame.postMessage({
  type: 'CONFIG_SAVED',
  success: true,
  device_id: 'sonoff_1'
}, 'https://esphub-configurator.example.com');
```

### HTTP API Endpoints

Hub предоставя REST API за управление на конфигурации:

#### GET /api/devices
Връща списък с всички конфигурирани устройства.

```json
{
  "devices": [
    {
      "device_id": "sonoff_1",
      "friendly_name": "Living Room Light",
      "location": "living_room",
      "protocol": "wifi",
      "status": "online"
    }
  ]
}
```

#### GET /api/devices/{device_id}
Връща пълната конфигурация за конкретно устройство.

#### POST /api/devices
Създава ново устройство от JSON конфигурация.

```bash
curl -X POST http://esp32-hub.local/api/devices \
  -H "Content-Type: application/json" \
  -d @device_config.json
```

#### PUT /api/devices/{device_id}
Обновява конфигурация на съществуващо устройство.

#### DELETE /api/devices/{device_id}
Изтрива устройство и неговите endpoints.

#### GET /api/templates
Връща достъпни device templates.

#### POST /api/devices/test
Тества конфигурация без да я запазва.

```json
{
  "config": { /* device config */ },
  "test_endpoint": "relay"
}
```

Response:
```json
{
  "success": true,
  "connection": "ok",
  "endpoint_test": {
    "relay": {
      "read": "success",
      "value": true
    }
  }
}
```

## Configuration Storage

Конфигурациите се запазват в LittleFS:

```
/config/
  devices.json           # List of all devices
  device_sonoff_1.json   # Individual device configs
  device_sensor_1.json
  templates/
    tasmota_switch.json
    esphome_sensor.json
```

## Benefits

1. **Flexibility**: Добавяй нови типове устройства без firmware update
2. **Rich UI**: Външният конфигуратор може да има много по-богат интерфейс
3. **Template Library**: Споделяй готови шаблони за популярни устройства
4. **Version Control**: JSON конфигурациите могат лесно да се версионират
5. **Testing**: Тествай конфигурации преди deployment
6. **Backup/Restore**: Лесен import/export на конфигурации
7. **Multi-Hub**: Един конфигуратор за множество ESP32 хъбове

## Example Workflow

1. Потребителят отваря Hub web interface
2. Избира "Add Device" → зарежда външния Configurator в iframe
3. В Configurator избира template (напр. "Tasmota Switch")
4. Попълва IP адрес и други параметри
5. Тества връзката
6. Потвърждава → Configurator изпраща JSON към Hub
7. Hub валидира, създава endpoints и запазва конфигурацията
8. Устройството се появява в Device Registry и е достъпно в PLC

## Security Considerations

- Валидация на JSON schema преди прилагане
- Sandbox на iframe за Configurator
- Rate limiting на API endpoints
- Encryption на credentials в saved configs
- HTTPS за Configurator communication
