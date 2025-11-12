# IOEventManager - Event-Driven PLC System Guide

## Описание

IOEventManager е event-driven система за автоматично стартиране на PLC програми при определени условия. Системата намалява процесорното натоварване като използва събития вместо непрекъснато проверяване.

## Възможности

### 1. I/O Event Triggers

Автоматично стартиране на програми при:
- **INPUT_CHANGED** - Промяна на вход
- **INPUT_OFFLINE** - Вход е недостъпен
- **INPUT_ONLINE** - Вход се е свързал
- **OUTPUT_ERROR** - Проблем с изход
- **VALUE_THRESHOLD** - Достигната е прагова стойност

### 2. Scheduled Triggers

Стартиране на програми по график:
- По час и минута
- По дни от седмицата (1=Понеделник, 7=Неделя)
- По месеци (1-12)
- Комбинации от горните

### 3. Event Priority

- **NORMAL** - обработва се в ред
- **CRITICAL** - обработва се незабавно

### 4. Event History

- Circular buffer с последните 100 събития
- Съхранява се в RAM
- MQTT публикуване на непрочетени събития
- Автоматично изчистване след публикуване

## Конфигурация

### JSON формат

```json
{
  "io_triggers": [
    {
      "name": "temperature_alarm",
      "type": "value_threshold",
      "endpoint": "kitchen.zigbee.temp_sensor.temperature.real",
      "program": "cooling_program",
      "priority": "critical",
      "threshold": 30.0,
      "threshold_rising": true,
      "debounce_ms": 5000,
      "enabled": true
    }
  ],
  "scheduled_triggers": [
    {
      "name": "morning_routine",
      "program": "morning_startup",
      "priority": "normal",
      "schedule": {
        "hour": 6,
        "minute": 30,
        "days": [1, 2, 3, 4, 5],
        "months": []
      },
      "enabled": true
    }
  ]
}
```

### I/O Trigger параметри

| Параметър | Тип | Описание |
|-----------|-----|----------|
| `name` | String | Уникално име на trigger |
| `type` | String | Тип събитие (вижте по-горе) |
| `endpoint` | String | Endpoint за наблюдение |
| `program` | String | Име на програма за стартиране |
| `priority` | String | "normal" или "critical" |
| `threshold` | Number | Прагова стойност (за VALUE_THRESHOLD) |
| `threshold_rising` | Boolean | true = rising edge, false = falling edge |
| `debounce_ms` | Number | Debounce време в ms |
| `enabled` | Boolean | Включен ли е trigger |

### Scheduled Trigger параметри

| Параметър | Тип | Описание |
|-----------|-----|----------|
| `name` | String | Уникално име на trigger |
| `program` | String | Име на програма за стартиране |
| `priority` | String | "normal" или "critical" |
| `schedule.hour` | Number | Час 0-23 (-1 = всеки час) |
| `schedule.minute` | Number | Минута 0-59 (-1 = всяка минута) |
| `schedule.days` | Array | Дни 1-7 (празен = всички дни) |
| `schedule.months` | Array | Месеци 1-12 (празен = всички месеци) |
| `enabled` | Boolean | Включен ли е trigger |

## Използване в код

### Инициализация

```cpp
#include <EspHub.h>

EspHub hub;

void setup() {
    hub.begin();
    hub.setupTime("EET-2EEST,M3.5.0/3,M10.5.0/4"); // Bulgaria timezone

    // Зареждане на event конфигурация
    const char* eventConfig = R"({
      "io_triggers": [...],
      "scheduled_triggers": [...]
    })";

    hub.loadEventConfiguration(eventConfig);
}

void loop() {
    hub.loop(); // Автоматично извиква ioEventManager.loop()
}
```

### Зареждане от файл

```cpp
void setup() {
    hub.begin();

    // Зареждане от LittleFS
    File file = LittleFS.open("/config/events.json", "r");
    if (file) {
        String config = file.readString();
        file.close();
        hub.loadEventConfiguration(config.c_str());
    }
}
```

### Получаване на Event History

```cpp
void publishEvents() {
    // Вземи само непрочетените събития
    String eventsJson = hub.getEventHistory(true);

    // Публикувай в MQTT
    mqttClient.publish("esphub/events", eventsJson.c_str());

    // Маркирай като прочетени
    hub.markEventsAsRead();
}
```

### Event History JSON формат

```json
{
  "events": [
    {
      "trigger": "temperature_alarm",
      "program": "cooling_program",
      "priority": "critical",
      "timestamp": 123456789,
      "type": "value_threshold",
      "details": "Endpoint: kitchen.zigbee.temp_sensor.temperature.real"
    }
  ],
  "stats": {
    "total": 150,
    "critical": 5,
    "normal": 145,
    "unread": 12
  }
}
```

## Примери

### Пример 1: Температурна аларма

Стартиране на охлаждаща програма когато температурата надвиши 30°C:

```json
{
  "name": "high_temp_alarm",
  "type": "value_threshold",
  "endpoint": "room.temp.value.real",
  "program": "cooling_program",
  "priority": "critical",
  "threshold": 30.0,
  "threshold_rising": true,
  "debounce_ms": 5000,
  "enabled": true
}
```

### Пример 2: Сензор за изтичане на вода

Алармена програма при изтичане или недостъпност на сензор:

```json
{
  "name": "leak_sensor_offline",
  "type": "input_offline",
  "endpoint": "basement.leak.state.bool",
  "program": "water_leak_alert",
  "priority": "critical",
  "enabled": true
}
```

### Пример 3: Сутрешна рутина

Стартиране на програма всеки делничен ден в 6:30:

```json
{
  "name": "morning_routine",
  "program": "morning_startup",
  "priority": "normal",
  "schedule": {
    "hour": 6,
    "minute": 30,
    "days": [1, 2, 3, 4, 5]
  },
  "enabled": true
}
```

### Пример 4: Седмична поддръжка

Програма за поддръжка всяка неделя в 2:00 сутринта:

```json
{
  "name": "weekly_maintenance",
  "program": "maintenance_check",
  "priority": "normal",
  "schedule": {
    "hour": 2,
    "minute": 0,
    "days": [7]
  },
  "enabled": true
}
```

### Пример 5: Сезонен климатичен контрол

Летен режим само през юни, юли и август:

```json
{
  "name": "summer_cooling",
  "program": "climate_control",
  "priority": "normal",
  "schedule": {
    "hour": 12,
    "minute": 0,
    "months": [6, 7, 8]
  },
  "enabled": true
}
```

## Производителност

### Паметна консумация

- **RAM**: ~7.7KB за IOEventManager
  - Event history: 100 събития × ~60 bytes = 6KB
  - Trigger maps: ~1KB
  - Statistics: ~100 bytes

- **Flash**: ~5KB код

### Производителност

- Event проверка: <1ms за всички triggers
- Scheduled проверка: веднъж на минута
- Без CPU overhead между проверките

## Ограничения

1. **Event history**: Максимум 100 събития в RAM
2. **Scheduled precision**: ±1 минута
3. **Едновременни програми**: Само една копия на програма
4. **No event chaining**: Една програма не може да тригерира друго събитие (може да се добави в бъдеще)

## Файлова структура

```
lib/PlcEngine/Events/
├── IOEventManager.h    - Header file
└── IOEventManager.cpp  - Implementation

data/config/
└── events_example.json - Примерна конфигурация
```

## API Reference

### EspHub методи

```cpp
// Зареждане на event конфигурация от JSON
void loadEventConfiguration(const char* jsonConfig);

// Получаване на event history
String getEventHistory(bool unreadOnly = true);

// Изчистване на event history
void clearEventHistory();

// Маркиране като прочетени
void markEventsAsRead();
```

### IOEventManager методи

```cpp
// Добавяне на I/O trigger
bool addIOTrigger(const IOEventTrigger& trigger);

// Премахване на I/O trigger
bool removeIOTrigger(const String& name);

// Добавяне на scheduled trigger
bool addScheduledTrigger(const ScheduledTrigger& trigger);

// Премахване на scheduled trigger
bool removeScheduledTrigger(const String& name);

// Зареждане от файл
bool loadConfigFromFile(const String& filepath);

// Запазване във файл
bool saveConfig(const String& filepath);

// Статистика
EventStats getStatistics() const;
```

## Съвети за използване

1. **Debounce**: Използвайте debounce за входове склонни към шум
2. **Priority**: Използвайте CRITICAL само за критични събития
3. **Event history**: Периодично публикувайте в MQTT за да освободите памет
4. **Testing**: Тествайте triggers преди production
5. **Monitoring**: Наблюдавайте статистиката за проблеми

## Troubleshooting

### Trigger не се задейства

1. Проверете дали е `enabled: true`
2. Проверете дали endpoint съществува и е online
3. Проверете debounce времето
4. Проверете threshold стойността и посоката

### Scheduled trigger не работи

1. Проверете дали TimeManager е инициализиран с NTP
2. Проверете timezone конфигурацията
3. Проверете дали hour/minute са правилни
4. Проверете days/months масивите

### Памет се запълва

1. Публикувайте event history редовно
2. Извикайте `markEventsAsRead()` след публикуване
3. Намалете броя на triggers ако е възможно

## Бъдещи подобрения

- [ ] Web UI за конфигуриране на triggers
- [ ] REST API endpoints
- [ ] Event chaining (една програма тригерира друго събитие)
- [ ] Custom event типове
- [ ] Event filtering
- [ ] Event aggregation
