# Modular Build System Design

## Overview

EspHub поддържа модулна компилация чрез compile-time feature flags. Това позволява да се включат само необходимите протоколи и функционалности за конкретния ESP32 модул, намалявайки размера на firmware и RAM използването.

## Feature Flags

Следните feature flags могат да се активират в `platformio.ini`:

| Flag | Описание | Зависимости |
|------|----------|-------------|
| `USE_MESH` | ESP-MESH мрежа | painlessMesh |
| `USE_ZIGBEE` | Zigbee чрез Zigbee2MQTT | MQTT, WiFi |
| `USE_BLE` | Bluetooth Low Energy | ESP32 BLE |
| `USE_RF433` | RF 433MHz устройства | RCSwitch |
| `USE_WIFI_DEVICES` | WiFi Smart устройства | WiFi |
| `USE_MQTT` | MQTT клиент | WiFi |
| `USE_WEB` | Web интерфейс | WiFi, AsyncWebServer |
| `USE_OTA` | Over-The-Air updates | WiFi |

## PlatformIO Environments

### Пример конфигурации

#### Full Gateway (всички протоколи)
```ini
[env:esp32_full]
platform = espressif32@^6.0.0
board = esp32dev
framework = arduino
build_flags =
    -DUSE_MESH
    -DUSE_ZIGBEE
    -DUSE_BLE
    -DUSE_RF433
    -DUSE_WIFI_DEVICES
    -DUSE_MQTT
    -DUSE_WEB
    -DUSE_OTA
lib_deps =
    knolleary/PubSubClient
    bblanchon/ArduinoJson
    esphome/ESPAsyncWebServer-esphome
    painlessMesh/painlessMesh
    sui77/rc-switch
```

#### Zigbee Gateway (само Zigbee)
```ini
[env:esp32_zigbee]
platform = espressif32@^6.0.0
board = esp32dev
framework = arduino
build_flags =
    -DUSE_ZIGBEE
    -DUSE_MQTT
    -DUSE_WEB
lib_deps =
    knolleary/PubSubClient
    bblanchon/ArduinoJson
    esphome/ESPAsyncWebServer-esphome
```

#### BLE + RF433 Hub
```ini
[env:esp32_ble_rf]
platform = espressif32@^6.0.0
board = esp32dev
framework = arduino
build_flags =
    -DUSE_BLE
    -DUSE_RF433
    -DUSE_WEB
lib_deps =
    bblanchon/ArduinoJson
    esphome/ESPAsyncWebServer-esphome
    sui77/rc-switch
```

#### Minimal (само PLC)
```ini
[env:esp32_minimal]
platform = espressif32@^6.0.0
board = esp32dev
framework = arduino
build_flags =
    -DUSE_WEB
lib_deps =
    bblanchon/ArduinoJson
    esphome/ESPAsyncWebServer-esphome
```

## Code Structure

### Conditional Manager Inclusion

**EspHubLib.h**
```cpp
#ifndef ESPHUB_LIB_H
#define ESPHUB_LIB_H

#include <Arduino.h>
#include "PlcCore/PlcEngine.h"
#include "DeviceRegistry.h"
#include "StreamLogger.h"

// Core managers (always included)
#ifdef USE_WEB
#include "WebManager.h"
#endif

#ifdef USE_MQTT
#include "MqttManager.h"
#endif

#ifdef USE_MESH
#include <painlessMesh.h>
#include "MeshDeviceManager.h"
#endif

#ifdef USE_ZIGBEE
#include "ZigbeeManager.h"
#endif

#ifdef USE_BLE
#include "BLEManager.h"
#endif

#ifdef USE_RF433
#include "RF433Manager.h"
#endif

#ifdef USE_WIFI_DEVICES
#include "WiFiDeviceManager.h"
#endif

#ifdef USE_OTA
#include "OtaManager.h"
#endif
```

### Conditional Manager Instantiation

**EspHubLib.cpp**
```cpp
void EspHub::begin() {
    // Core initialization
    plcEngine.begin();

#ifdef USE_WEB
    webManager = new WebManager(&plcEngine, ...);
    webManager->begin();
#endif

#ifdef USE_MQTT
    mqttManager = new MqttManager();
    mqttManager->begin(mqttServer, mqttPort);
#endif

#ifdef USE_MESH
    meshDeviceManager = new MeshDeviceManager();
    mesh.begin();
#endif

#ifdef USE_ZIGBEE
    zigbeeManager = new ZigbeeManager(mqttManager, defaultLocation);
    zigbeeManager->begin();
#endif

#ifdef USE_BLE
    bleManager = new BLEManager();
    bleManager->begin();
#endif

#ifdef USE_RF433
    rf433Manager = new RF433Manager(rxPin, txPin);
    rf433Manager->begin();
#endif
}
```

### Conditional Member Variables

**EspHubLib.h (private members)**
```cpp
class EspHub {
private:
    PlcEngine plcEngine;
    DeviceRegistry& registry;

#ifdef USE_WEB
    WebManager* webManager;
#endif

#ifdef USE_MQTT
    MqttManager* mqttManager;
#endif

#ifdef USE_MESH
    painlessMesh mesh;
    MeshDeviceManager* meshDeviceManager;
#endif

#ifdef USE_ZIGBEE
    ZigbeeManager* zigbeeManager;
#endif

#ifdef USE_BLE
    BLEManager* bleManager;
#endif

#ifdef USE_RF433
    RF433Manager* rf433Manager;
#endif
};
```

## Protocol Managers

### RF433Manager

RF433Manager управлява 433MHz RF устройства (превключватели, сензори, дистанционни).

**Capabilities:**
- Приемане на RF сигнали
- Изпращане на RF команди
- Автоматично регистриране на устройства
- Поддръжка за различни протоколи (PT2262, EV1527, и др.)

**Hardware:**
- RX модул на GPIO (напр. GPIO 27)
- TX модул на GPIO (напр. GPIO 26)

**Naming Convention:**
```
{location}.rf433.{device_id}.{endpoint}.{datatype}
Example: living_room.rf433.switch_1.state.bool
```

### BLEManager

BLEManager управлява Bluetooth Low Energy устройства.

**Capabilities:**
- BLE сканиране и откриване
- GATT характеристики четене/запис
- iBeacon/Eddystone поддръжка
- Xiaomi Mi Home сензори
- Custom BLE устройства

**Naming Convention:**
```
{location}.ble.{device_name}.{characteristic}.{datatype}
Example: bedroom.ble.temp_sensor.temperature.real
```

### WiFiDeviceManager

WiFiDeviceManager управлява WiFi smart устройства (Sonoff, Shelly, Tasmota, ESPHome).

**Capabilities:**
- HTTP REST API контрол
- MQTT интеграция
- mDNS discovery
- Tasmota/ESPHome native support

**Naming Convention:**
```
{location}.wifi.{device_name}.{endpoint}.{datatype}
Example: kitchen.wifi.sonoff_basic.relay.bool
```

## Build Commands

### Compile specific environment
```bash
# Full gateway
pio run -e esp32_full

# Zigbee only
pio run -e esp32_zigbee

# BLE + RF433
pio run -e esp32_ble_rf

# Minimal
pio run -e esp32_minimal
```

### Upload specific environment
```bash
pio run -e esp32_zigbee --target upload
```

### Monitor
```bash
pio device monitor
```

## Memory Optimization

Приблизителни flash размери за различните модули:

| Configuration | Flash | RAM |
|---------------|-------|-----|
| Minimal (PLC only) | ~400KB | ~30KB |
| + Web | ~600KB | ~45KB |
| + MQTT | ~650KB | ~50KB |
| + Zigbee | ~700KB | ~55KB |
| + BLE | ~800KB | ~65KB |
| + RF433 | ~820KB | ~66KB |
| + Mesh | ~900KB | ~75KB |
| Full (всички) | ~1.2MB | ~95KB |

## Migration Guide

За съществуващ код, който използва всички модули:

1. **Добави build flags в platformio.ini:**
```ini
build_flags =
    -DUSE_MESH
    -DUSE_ZIGBEE
    -DUSE_BLE
    -DUSE_RF433
    -DUSE_WIFI_DEVICES
    -DUSE_MQTT
    -DUSE_WEB
    -DUSE_OTA
```

2. **Обнови main.cpp:**
```cpp
void setup() {
    espHub.begin();

    // Conditional initialization
#ifdef USE_ZIGBEE
    espHub.setZigbeeBridge("zigbee2mqtt");
#endif

#ifdef USE_RF433
    espHub.setRF433Pins(27, 26); // RX, TX
#endif
}
```

## Benefits

1. **Reduced Flash Usage**: Включвай само необходимите протоколи
2. **Reduced RAM Usage**: По-малко статични обекти
3. **Faster Compilation**: По-малко код за компилиране
4. **Easier Deployment**: Специализирани firmware за специфични задачи
5. **Better Maintainability**: Ясна зависимост между компоненти

## Future Enhancements

- Auto-detection на наличен хардуер
- Runtime configuration за enable/disable протоколи
- Web UI за избор на активни протоколи
- Binary distribution за популярни конфигурации
