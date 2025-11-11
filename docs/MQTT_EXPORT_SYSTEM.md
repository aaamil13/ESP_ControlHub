# MQTT Export System

## Преглед

MQTT Export System предоставя хибриден подход за експортиране на променливи и команди през MQTT. Системата поддържа три нива на достъп:

1. **Read-only променливи** - Сензори, статус информация (auto-publish при промяна)
2. **Read-write променливи** - Setpoints, конфигурация (двупосочна синхронизация с валидация)
3. **Команди** - Изпълнение на PLC функции през MQTT

## Архитектура

```
┌─────────────────────────────────────────────────────────┐
│                   MqttExportManager                      │
├─────────────────────────────────────────────────────────┤
│                                                          │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  │
│  │   Variables   │  │  Validation  │  │   Commands   │  │
│  │   Registry    │  │    Rules     │  │   Registry   │  │
│  └──────────────┘  └──────────────┘  └──────────────┘  │
│                                                          │
└────────┬──────────────────────────────────────┬─────────┘
         │                                      │
         ▼                                      ▼
  ┌─────────────┐                      ┌─────────────┐
  │  Variable   │                      │     PLC     │
  │  Registry   │                      │   Engine    │
  └─────────────┘                      └─────────────┘
         │
         ▼
  ┌─────────────┐
  │    MQTT     │
  │   Manager   │
  └─────────────┘
```

## Конфигурация

### Variable Export

```json
{
  "mqtt_export": {
    "variables": {
      "main.temperature": {
        "topic": "sensors/temperature",
        "access": "read",
        "publish_on_change": true,
        "min_interval_ms": 5000,
        "retained": true,
        "qos": 0
      }
    }
  }
}
```

#### Параметри

- **topic**: MQTT topic за променливата
- **access**: Ниво на достъп
  - `"read"` - Само четене (публикува се в MQTT)
  - `"write"` - Само запис (приема се от MQTT)
  - `"read_write"` - Двупосочна синхронизация
- **publish_on_change**: Auto-publish при промяна на стойността
- **min_interval_ms**: Минимален интервал между публикации (throttling)
- **retained**: MQTT retained flag (NOTE: Not currently implemented in MqttManager)
- **qos**: MQTT QoS level (NOTE: Not currently implemented in MqttManager)
- **validation**: Правила за валидация (опционално)

### Validation Rules

```json
{
  "main.setpoint": {
    "topic": "control/setpoint",
    "access": "read_write",
    "validation": {
      "min": 15.0,
      "max": 30.0
    }
  }
}
```

#### Типове валидация

- **Numeric Range**: `min` и `max` за числови стойности
- **Regex**: `regex` за string стойности (бъдеща функционалност)
- **Custom**: Custom validation функция (програмно)

### Command Definitions

```json
{
  "mqtt_export": {
    "commands": {
      "activateScene": {
        "topic": "commands/scene/activate",
        "handler": "plc_activateScene",
        "parameters": ["scene_name"],
        "require_auth": false
      }
    }
  }
}
```

#### Параметри

- **topic**: MQTT topic за командата
- **handler**: Име на PLC функция за изпълнение
- **parameters**: Списък с очаквани параметри
- **require_auth**: Изисква ли се authentication (бъдеща функционалност)

## Използване

### Инициализация

```cpp
// В EspHubLib.cpp
mqttExportManager.begin();
mqttExportManager.setMqttManager(&mqttManager);
mqttExportManager.setVariableRegistry(&variableRegistry);
mqttExportManager.setPlcEngine(&plcEngine);
```

### Зареждане на конфигурация

```cpp
// От JSON файл
mqttExportManager.loadConfigFromFile("/config/mqtt_export.json");

// От JSON обект
JsonDocument doc;
// ... parse JSON
mqttExportManager.loadConfig(doc["mqtt_export"].as<JsonObject>());
```

### Програмно добавяне на export rules

```cpp
// Добавяне на read-only променлива
mqttExportManager.addExportRule(
    "main.temperature",
    "sensors/temperature",
    ExportAccessLevel::READ_ONLY
);

// Настройка на валидация
mqttExportManager.setValidationRange("main.setpoint", 15.0, 30.0);

// Добавяне на команда
std::vector<String> params = {"scene_name"};
mqttExportManager.registerCommand(
    "activateScene",
    "commands/scene/activate",
    "plc_activateScene",
    params
);
```

### MQTT Message Handling

```cpp
// В MQTT callback функцията
void mqtt_callback(char* topic, byte* payload, unsigned int length) {
    char* message = (char*)malloc(length + 1);
    if (message) {
        memcpy(message, payload, length);
        message[length] = '\0';

        // Предава се на export manager
        mqttExportManager.handleMqttMessage(String(topic), String(message));

        free(message);
    }
}
```

## MQTT Message Format

### Variable Read (Hub -> MQTT)

**Topic**: Конфигурираният topic (напр. `sensors/temperature`)
**Payload**: JSON или plain value

```json
{
  "value": 23.5,
  "timestamp": 1234567890
}
```

Или просто:
```
23.5
```

### Variable Write (MQTT -> Hub)

**Topic**: Конфигурираният topic (напр. `control/setpoint`)
**Payload**: Новата стойност

```
25.0
```

Или JSON:
```json
{
  "value": 25.0
}
```

### Command Execution (MQTT -> Hub)

**Topic**: Конфигурираният command topic (напр. `commands/scene/activate`)
**Payload**: JSON с параметри

```json
{
  "scene_name": "evening"
}
```

## Auto-Publishing

Променливите с `publish_on_change: true` се публикуват автоматично при промяна на стойността им.

```cpp
// В PLC програма
plcMemory.setValue("temperature", 24.5f);
// Автоматично публикува в MQTT ако е конфигурирано
```

### Throttling

`min_interval_ms` предотвратява прекалено честите публикации:

```json
{
  "min_interval_ms": 5000  // Минимум 5 секунди между публикации
}
```

## Statistics

```cpp
auto stats = mqttExportManager.getStatistics();
Serial.printf("Total exports: %d\n", stats.totalExports);
Serial.printf("Total publishes: %lu\n", stats.totalPublishes);
Serial.printf("Total writes: %lu\n", stats.totalWrites);
Serial.printf("Total command executions: %lu\n", stats.totalCommandExecutions);
```

## Примери за употреба

### Сензорна система

```json
{
  "mqtt_export": {
    "variables": {
      "main.room1_temp": {
        "topic": "home/room1/temperature",
        "access": "read",
        "publish_on_change": true,
        "min_interval_ms": 10000
      },
      "main.room1_humidity": {
        "topic": "home/room1/humidity",
        "access": "read",
        "publish_on_change": true,
        "min_interval_ms": 10000
      }
    }
  }
}
```

### Контролна система

```json
{
  "mqtt_export": {
    "variables": {
      "main.hvac_setpoint": {
        "topic": "home/hvac/setpoint",
        "access": "read_write",
        "publish_on_change": true,
        "validation": {
          "min": 18.0,
          "max": 28.0
        }
      },
      "main.hvac_mode": {
        "topic": "home/hvac/mode",
        "access": "read_write",
        "publish_on_change": true
      }
    }
  }
}
```

### Команди

```json
{
  "mqtt_export": {
    "commands": {
      "startCycle": {
        "topic": "machine/commands/start_cycle",
        "handler": "plc_startCycle",
        "parameters": ["cycle_type", "duration"]
      },
      "stopCycle": {
        "topic": "machine/commands/stop",
        "handler": "plc_stopCycle",
        "parameters": []
      }
    }
  }
}
```

## Интеграция с Home Assistant

```yaml
# Home Assistant configuration.yaml
mqtt:
  sensor:
    - name: "Room Temperature"
      state_topic: "home/room1/temperature"
      unit_of_measurement: "°C"

  number:
    - name: "HVAC Setpoint"
      state_topic: "home/hvac/setpoint"
      command_topic: "home/hvac/setpoint"
      min: 18
      max: 28
      step: 0.5
      unit_of_measurement: "°C"
```

## Известни ограничения

1. **MqttManager Limitations**:
   - `retained` flag не се поддържа (publish винаги е non-retained)
   - `qos` не се поддържа (винаги е QoS 0)

2. **Firmware Size**:
   - Кодът е успешно компилиран но е 13KB над лимита за стандартната partition схема
   - Решение: Използване на custom partition схема с повече app space

## Бъдещи подобрения

1. ✅ Variable Registry Integration
2. ✅ Hybrid Export System (variables + commands)
3. ✅ Validation Rules
4. ✅ Auto-publishing with throttling
5. ⏳ MqttManager retained/QoS support
6. ⏳ Authentication for commands
7. ⏳ Custom validators via lambda functions
8. ⏳ Firmware size optimization
