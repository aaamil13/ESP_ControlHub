#include "IOEventManager.h"
#include "../Engine/PlcEngine.h"
#include <StreamLogger.h>
#include <LittleFS.h>

extern StreamLogger* EspHubLog;

IOEventManager::IOEventManager()
    : deviceRegistry(nullptr),
      plcEngine(nullptr),
      timeManager(nullptr),
      historyHead(0),
      historyCount(0) {
    memset(&stats, 0, sizeof(stats));
}

IOEventManager::~IOEventManager() {
}

void IOEventManager::begin() {
    EspHubLog->println("IOEventManager: Initializing...");
    ioTriggers.clear();
    scheduledTriggers.clear();
    historyHead = 0;
    historyCount = 0;
    memset(&stats, 0, sizeof(stats));
    EspHubLog->println("IOEventManager: Initialized");
}

void IOEventManager::loop() {
    // Check I/O events
    checkIOEvents();

    // Check scheduled events
    checkScheduledEvents();
}

// ============================================================================
// Configuration
// ============================================================================

bool IOEventManager::loadConfig(const JsonObject& config) {
    EspHubLog->println("IOEventManager: Loading config...");

    // Load I/O triggers
    if (config.containsKey("io_triggers")) {
        JsonArrayConst triggers = config["io_triggers"].as<JsonArrayConst>();
        for (JsonObjectConst triggerObj : triggers) {
            IOEventTrigger trigger;
            trigger.name = triggerObj["name"].as<String>();
            trigger.endpoint = triggerObj["endpoint"].as<String>();
            trigger.programToRun = triggerObj["program"].as<String>();

            // Parse event type
            String typeStr = triggerObj["type"].as<String>();
            if (typeStr == "input_changed") trigger.type = IOEventType::INPUT_CHANGED;
            else if (typeStr == "input_offline") trigger.type = IOEventType::INPUT_OFFLINE;
            else if (typeStr == "input_online") trigger.type = IOEventType::INPUT_ONLINE;
            else if (typeStr == "output_error") trigger.type = IOEventType::OUTPUT_ERROR;
            else if (typeStr == "value_threshold") trigger.type = IOEventType::VALUE_THRESHOLD;

            // Parse priority
            String priorityStr = triggerObj["priority"].as<String>();
            trigger.priority = (priorityStr == "critical") ? EventPriority::CRITICAL : EventPriority::NORMAL;

            trigger.enabled = triggerObj["enabled"] | true;
            trigger.debounceMs = triggerObj["debounce_ms"] | 0;

            // Threshold settings
            if (triggerObj.containsKey("threshold")) {
                trigger.thresholdRising = triggerObj["threshold_rising"] | true;
                // Parse threshold value based on type
                // TODO: Parse threshold value
            }

            addIOTrigger(trigger);
        }
    }

    // Load scheduled triggers
    if (config.containsKey("scheduled_triggers")) {
        JsonArrayConst triggers = config["scheduled_triggers"].as<JsonArrayConst>();
        for (JsonObjectConst triggerObj : triggers) {
            ScheduledTrigger trigger;
            trigger.name = triggerObj["name"].as<String>();
            trigger.programToRun = triggerObj["program"].as<String>();

            // Parse priority
            String priorityStr = triggerObj["priority"].as<String>();
            trigger.priority = (priorityStr == "critical") ? EventPriority::CRITICAL : EventPriority::NORMAL;

            trigger.enabled = triggerObj["enabled"] | true;

            // Parse schedule
            if (triggerObj.containsKey("schedule")) {
                JsonObjectConst schedule = triggerObj["schedule"];
                trigger.hour = schedule["hour"] | -1;
                trigger.minute = schedule["minute"] | -1;

                // Parse days array
                if (schedule.containsKey("days")) {
                    JsonArrayConst days = schedule["days"];
                    for (JsonVariantConst day : days) {
                        trigger.days.push_back(day.as<uint8_t>());
                    }
                }

                // Parse months array
                if (schedule.containsKey("months")) {
                    JsonArrayConst months = schedule["months"];
                    for (JsonVariantConst month : months) {
                        trigger.months.push_back(month.as<uint8_t>());
                    }
                }
            }

            addScheduledTrigger(trigger);
        }
    }

    EspHubLog->printf("IOEventManager: Loaded %d I/O triggers, %d scheduled triggers\n",
                     ioTriggers.size(), scheduledTriggers.size());
    return true;
}

bool IOEventManager::loadConfigFromFile(const String& filepath) {
    if (!LittleFS.begin()) {
        EspHubLog->println("ERROR: LittleFS not mounted");
        return false;
    }

    File file = LittleFS.open(filepath, "r");
    if (!file) {
        EspHubLog->println("ERROR: Failed to open event config: " + filepath);
        return false;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) {
        EspHubLog->printf("ERROR: Failed to parse event config: %s\n", error.c_str());
        return false;
    }

    return loadConfig(doc.as<JsonObject>());
}

bool IOEventManager::saveConfig(const String& filepath) {
    JsonDocument doc;
    JsonObject config = doc.to<JsonObject>();

    // Save I/O triggers
    JsonArray ioTriggersArray = config["io_triggers"].to<JsonArray>();
    for (auto& pair : ioTriggers) {
        IOEventTrigger& trigger = pair.second;
        JsonObject triggerObj = ioTriggersArray.add<JsonObject>();
        triggerObj["name"] = trigger.name;
        triggerObj["endpoint"] = trigger.endpoint;
        triggerObj["program"] = trigger.programToRun;
        triggerObj["type"] = getEventTypeString(trigger.type);
        triggerObj["priority"] = (trigger.priority == EventPriority::CRITICAL) ? "critical" : "normal";
        triggerObj["enabled"] = trigger.enabled;
        triggerObj["debounce_ms"] = trigger.debounceMs;
    }

    // Save scheduled triggers
    JsonArray scheduledTriggersArray = config["scheduled_triggers"].to<JsonArray>();
    for (auto& pair : scheduledTriggers) {
        ScheduledTrigger& trigger = pair.second;
        JsonObject triggerObj = scheduledTriggersArray.add<JsonObject>();
        triggerObj["name"] = trigger.name;
        triggerObj["program"] = trigger.programToRun;
        triggerObj["priority"] = (trigger.priority == EventPriority::CRITICAL) ? "critical" : "normal";
        triggerObj["enabled"] = trigger.enabled;

        JsonObject schedule = triggerObj["schedule"].to<JsonObject>();
        if (trigger.hour >= 0) schedule["hour"] = trigger.hour;
        if (trigger.minute >= 0) schedule["minute"] = trigger.minute;

        if (!trigger.days.empty()) {
            JsonArray days = schedule["days"].to<JsonArray>();
            for (uint8_t day : trigger.days) {
                days.add(day);
            }
        }

        if (!trigger.months.empty()) {
            JsonArray months = schedule["months"].to<JsonArray>();
            for (uint8_t month : trigger.months) {
                months.add(month);
            }
        }
    }

    File file = LittleFS.open(filepath, "w");
    if (!file) {
        EspHubLog->println("ERROR: Failed to open for writing: " + filepath);
        return false;
    }

    serializeJson(doc, file);
    file.close();

    EspHubLog->println("IOEventManager: Config saved to: " + filepath);
    return true;
}

// ============================================================================
// IO Event Triggers
// ============================================================================

bool IOEventManager::addIOTrigger(const IOEventTrigger& trigger) {
    if (trigger.name.isEmpty()) {
        EspHubLog->println("ERROR: IO trigger name cannot be empty");
        return false;
    }

    ioTriggers[trigger.name] = trigger;
    EspHubLog->printf("IOEventManager: Added I/O trigger '%s' for endpoint '%s'\n",
                     trigger.name.c_str(), trigger.endpoint.c_str());
    return true;
}

bool IOEventManager::removeIOTrigger(const String& name) {
    auto it = ioTriggers.find(name);
    if (it == ioTriggers.end()) {
        return false;
    }
    ioTriggers.erase(it);
    EspHubLog->printf("IOEventManager: Removed I/O trigger '%s'\n", name.c_str());
    return true;
}

IOEventTrigger* IOEventManager::getIOTrigger(const String& name) {
    auto it = ioTriggers.find(name);
    if (it == ioTriggers.end()) {
        return nullptr;
    }
    return &it->second;
}

bool IOEventManager::setIOTriggerEnabled(const String& name, bool enabled) {
    IOEventTrigger* trigger = getIOTrigger(name);
    if (!trigger) {
        return false;
    }
    trigger->enabled = enabled;
    return true;
}

std::vector<String> IOEventManager::getIOTriggerNames() const {
    std::vector<String> names;
    for (const auto& pair : ioTriggers) {
        names.push_back(pair.first);
    }
    return names;
}

// ============================================================================
// Scheduled Triggers
// ============================================================================

bool IOEventManager::addScheduledTrigger(const ScheduledTrigger& trigger) {
    if (trigger.name.isEmpty()) {
        EspHubLog->println("ERROR: Scheduled trigger name cannot be empty");
        return false;
    }

    scheduledTriggers[trigger.name] = trigger;
    EspHubLog->printf("IOEventManager: Added scheduled trigger '%s' for program '%s'\n",
                     trigger.name.c_str(), trigger.programToRun.c_str());
    return true;
}

bool IOEventManager::removeScheduledTrigger(const String& name) {
    auto it = scheduledTriggers.find(name);
    if (it == scheduledTriggers.end()) {
        return false;
    }
    scheduledTriggers.erase(it);
    EspHubLog->printf("IOEventManager: Removed scheduled trigger '%s'\n", name.c_str());
    return true;
}

ScheduledTrigger* IOEventManager::getScheduledTrigger(const String& name) {
    auto it = scheduledTriggers.find(name);
    if (it == scheduledTriggers.end()) {
        return nullptr;
    }
    return &it->second;
}

bool IOEventManager::setScheduledTriggerEnabled(const String& name, bool enabled) {
    ScheduledTrigger* trigger = getScheduledTrigger(name);
    if (!trigger) {
        return false;
    }
    trigger->enabled = enabled;
    return true;
}

std::vector<String> IOEventManager::getScheduledTriggerNames() const {
    std::vector<String> names;
    for (const auto& pair : scheduledTriggers) {
        names.push_back(pair.first);
    }
    return names;
}

// ============================================================================
// Event History
// ============================================================================

std::vector<EventRecord> IOEventManager::getEventHistory(bool unreadOnly) {
    std::vector<EventRecord> result;

    size_t count = historyCount;
    size_t pos = (historyHead + MAX_HISTORY - count) % MAX_HISTORY;

    for (size_t i = 0; i < count; i++) {
        const EventRecord& record = eventHistory[pos];
        if (!unreadOnly || !record.mqttPublished) {
            result.push_back(record);
        }
        pos = (pos + 1) % MAX_HISTORY;
    }

    return result;
}

void IOEventManager::markEventsAsRead() {
    size_t count = historyCount;
    size_t pos = (historyHead + MAX_HISTORY - count) % MAX_HISTORY;

    for (size_t i = 0; i < count; i++) {
        eventHistory[pos].mqttPublished = true;
        pos = (pos + 1) % MAX_HISTORY;
    }

    EspHubLog->printf("IOEventManager: Marked %d events as read\n", count);
}

void IOEventManager::clearHistory() {
    historyHead = 0;
    historyCount = 0;
    EspHubLog->println("IOEventManager: Event history cleared");
}

IOEventManager::EventStats IOEventManager::getStatistics() const {
    return stats;
}

String IOEventManager::serializeEventsToJson(bool unreadOnly) {
    JsonDocument doc;
    JsonArray eventsArray = doc["events"].to<JsonArray>();

    size_t count = historyCount;
    size_t pos = (historyHead + MAX_HISTORY - count) % MAX_HISTORY;

    for (size_t i = 0; i < count; i++) {
        const EventRecord& record = eventHistory[pos];

        // Skip if filtering for unread and event is already published
        if (unreadOnly && record.mqttPublished) {
            pos = (pos + 1) % MAX_HISTORY;
            continue;
        }

        JsonObject eventObj = eventsArray.add<JsonObject>();
        eventObj["trigger"] = record.triggerName;
        eventObj["program"] = record.programName;
        eventObj["priority"] = (record.priority == EventPriority::CRITICAL) ? "critical" : "normal";
        eventObj["timestamp"] = record.timestamp;
        eventObj["type"] = record.eventType;
        eventObj["details"] = record.details;

        pos = (pos + 1) % MAX_HISTORY;
    }

    // Add statistics
    JsonObject statsObj = doc["stats"].to<JsonObject>();
    statsObj["total"] = stats.totalEvents;
    statsObj["critical"] = stats.criticalEvents;
    statsObj["normal"] = stats.normalEvents;
    statsObj["unread"] = stats.unreadEvents;

    String output;
    serializeJson(doc, output);
    return output;
}

// ============================================================================
// Integration Hooks
// ============================================================================

void IOEventManager::setDeviceRegistry(DeviceRegistry* registry) {
    deviceRegistry = registry;
}

void IOEventManager::setPlcEngine(PlcEngine* engine) {
    plcEngine = engine;
}

void IOEventManager::setTimeManager(TimeManager* manager) {
    timeManager = manager;
}

// ============================================================================
// Private Processing Methods
// ============================================================================

void IOEventManager::checkIOEvents() {
    if (!deviceRegistry || !plcEngine) {
        return;
    }

    for (auto& pair : ioTriggers) {
        IOEventTrigger& trigger = pair.second;

        if (!trigger.enabled) {
            continue;
        }

        if (shouldTriggerIO(trigger)) {
            String eventType = getEventTypeString(trigger.type);
            String details = "Endpoint: " + trigger.endpoint;
            executeEvent(trigger.name, trigger.programToRun, trigger.priority, eventType, details);
            trigger.lastTrigger = millis();
        }
    }
}

void IOEventManager::checkScheduledEvents() {
    if (!timeManager || !plcEngine) {
        return;
    }

    tm currentTime = timeManager->getCurrentTime();

    for (auto& pair : scheduledTriggers) {
        ScheduledTrigger& trigger = pair.second;

        if (!trigger.enabled) {
            continue;
        }

        if (shouldTriggerScheduled(trigger)) {
            String details = String("Scheduled at ") + String(currentTime.tm_hour) + ":" + String(currentTime.tm_min);
            executeEvent(trigger.name, trigger.programToRun, trigger.priority, "scheduled_time", details);
            trigger.lastTrigger = millis();
        }
    }
}

bool IOEventManager::shouldTriggerIO(IOEventTrigger& trigger) {
    // Check debounce
    if (trigger.debounceMs > 0) {
        if (millis() - trigger.lastTrigger < trigger.debounceMs) {
            return false;
        }
    }

    // Get endpoint
    Endpoint* endpoint = deviceRegistry->getEndpoint(trigger.endpoint);
    if (!endpoint) {
        return false;
    }

    switch (trigger.type) {
        case IOEventType::INPUT_CHANGED: {
            // Check if value changed
            if (endpoint->currentValue.type != trigger.lastValue.type) {
                trigger.lastValue = endpoint->currentValue;
                return true;
            }

            // Compare values based on type
            bool changed = false;
            switch (endpoint->currentValue.type) {
                case PlcValueType::BOOL:
                    changed = (endpoint->currentValue.value.bVal != trigger.lastValue.value.bVal);
                    break;
                case PlcValueType::INT:
                    changed = (endpoint->currentValue.value.i16Val != trigger.lastValue.value.i16Val);
                    break;
                case PlcValueType::REAL:
                    changed = (abs(endpoint->currentValue.value.fVal - trigger.lastValue.value.fVal) > 0.001f);
                    break;
                default:
                    break;
            }

            if (changed) {
                trigger.lastValue = endpoint->currentValue;
                return true;
            }
            break;
        }

        case IOEventType::INPUT_OFFLINE:
            return !endpoint->isOnline;

        case IOEventType::INPUT_ONLINE:
            return endpoint->isOnline;

        case IOEventType::OUTPUT_ERROR:
            // Check if output endpoint is offline (error condition)
            return !endpoint->isOnline && endpoint->isWritable;

        case IOEventType::VALUE_THRESHOLD:
            return compareThreshold(endpoint->currentValue, trigger.threshold, trigger.thresholdRising);

        default:
            break;
    }

    return false;
}

bool IOEventManager::shouldTriggerScheduled(ScheduledTrigger& trigger) {
    tm currentTime = timeManager->getCurrentTime();

    // Prevent triggering multiple times in the same minute
    unsigned long currentMinute = currentTime.tm_hour * 60 + currentTime.tm_min;
    unsigned long lastMinute = trigger.lastTrigger / 60000; // Convert ms to minutes

    if (currentMinute == lastMinute) {
        return false; // Already triggered this minute
    }

    return isScheduleMatch(trigger, currentTime);
}

void IOEventManager::executeEvent(const String& triggerName, const String& programName,
                                  EventPriority priority, const String& eventType, const String& details) {
    EspHubLog->printf("IOEventManager: Event triggered '%s' -> running program '%s' (%s priority)\n",
                     triggerName.c_str(), programName.c_str(),
                     (priority == EventPriority::CRITICAL) ? "CRITICAL" : "normal");

    // Start the program
    if (plcEngine) {
        plcEngine->runProgram(programName);
    }

    // Add to history
    EventRecord record;
    record.triggerName = triggerName;
    record.programName = programName;
    record.priority = priority;
    record.timestamp = millis();
    record.eventType = eventType;
    record.details = details;
    record.mqttPublished = false;

    addToHistory(record);

    // Update statistics
    stats.totalEvents++;
    if (priority == EventPriority::CRITICAL) {
        stats.criticalEvents++;
    } else {
        stats.normalEvents++;
    }
    stats.lastEventTime = millis();
}

void IOEventManager::addToHistory(const EventRecord& record) {
    eventHistory[historyHead] = record;
    historyHead = (historyHead + 1) % MAX_HISTORY;

    if (historyCount < MAX_HISTORY) {
        historyCount++;
        stats.unreadEvents++;
    }
}

// ============================================================================
// Helper Methods
// ============================================================================

bool IOEventManager::compareThreshold(const PlcValue& currentValue, const PlcValue& threshold, bool rising) {
    if (currentValue.type != threshold.type) {
        return false;
    }

    switch (currentValue.type) {
        case PlcValueType::INT:
            if (rising) {
                return currentValue.value.i16Val > threshold.value.i16Val;
            } else {
                return currentValue.value.i16Val < threshold.value.i16Val;
            }

        case PlcValueType::REAL:
            if (rising) {
                return currentValue.value.fVal > threshold.value.fVal;
            } else {
                return currentValue.value.fVal < threshold.value.fVal;
            }

        case PlcValueType::BOOL:
            if (rising) {
                return currentValue.value.bVal && !threshold.value.bVal;
            } else {
                return !currentValue.value.bVal && threshold.value.bVal;
            }

        default:
            return false;
    }
}

bool IOEventManager::isScheduleMatch(const ScheduledTrigger& trigger, const tm& currentTime) {
    // Check hour
    if (trigger.hour >= 0 && currentTime.tm_hour != trigger.hour) {
        return false;
    }

    // Check minute
    if (trigger.minute >= 0 && currentTime.tm_min != trigger.minute) {
        return false;
    }

    // Check day of week (tm_wday: 0=Sunday, 1=Monday, ... 6=Saturday)
    // Convert to our format: 1=Monday, 7=Sunday
    if (!trigger.days.empty()) {
        uint8_t dayOfWeek = (currentTime.tm_wday == 0) ? 7 : currentTime.tm_wday;
        bool dayMatch = false;
        for (uint8_t day : trigger.days) {
            if (day == dayOfWeek) {
                dayMatch = true;
                break;
            }
        }
        if (!dayMatch) {
            return false;
        }
    }

    // Check month (tm_mon: 0-11, convert to 1-12)
    if (!trigger.months.empty()) {
        uint8_t month = currentTime.tm_mon + 1;
        bool monthMatch = false;
        for (uint8_t m : trigger.months) {
            if (m == month) {
                monthMatch = true;
                break;
            }
        }
        if (!monthMatch) {
            return false;
        }
    }

    return true;
}

String IOEventManager::getEventTypeString(IOEventType type) {
    switch (type) {
        case IOEventType::INPUT_CHANGED: return "input_changed";
        case IOEventType::INPUT_OFFLINE: return "input_offline";
        case IOEventType::INPUT_ONLINE: return "input_online";
        case IOEventType::OUTPUT_ERROR: return "output_error";
        case IOEventType::VALUE_THRESHOLD: return "value_threshold";
        default: return "unknown";
    }
}
