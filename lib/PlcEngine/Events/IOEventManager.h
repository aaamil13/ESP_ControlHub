#ifndef IO_EVENT_MANAGER_H
#define IO_EVENT_MANAGER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>
#include <map>
#include "../../Devices/DeviceRegistry.h"
#include "../../Core/TimeManager.h"

// Forward declarations to avoid circular dependencies
class PlcEngine;

// Event priority levels
enum class EventPriority {
    NORMAL,      // Normal priority - processed in order
    CRITICAL     // Critical priority - processed immediately
};

// IO Event types
enum class IOEventType {
    INPUT_CHANGED,      // Input value changed
    INPUT_OFFLINE,      // Input device went offline
    OUTPUT_ERROR,       // Output device error
    VALUE_THRESHOLD,    // Value crossed threshold
    INPUT_ONLINE        // Input device came online
};

// System Event types
enum class SystemEventType {
    SCHEDULED_TIME,     // Scheduled time trigger
    SCHEDULED_DATE,     // Scheduled date trigger
    DEVICE_ONLINE,      // Device connected
    DEVICE_OFFLINE,     // Device disconnected
    MEMORY_LOW,         // Low memory warning
    PROGRAM_ERROR       // Program execution error
};

// Event trigger condition
struct IOEventTrigger {
    String name;                // Trigger name
    IOEventType type;           // Event type
    EventPriority priority;     // Priority level
    String endpoint;            // Endpoint to monitor (for I/O events)
    String programToRun;        // Program to start when triggered
    bool enabled;               // Is trigger enabled

    // Optional parameters
    PlcValue threshold;         // For VALUE_THRESHOLD
    bool thresholdRising;       // True = trigger on rising edge
    uint32_t debounceMs;        // Debounce time in milliseconds

    // State tracking
    unsigned long lastTrigger;  // Last trigger timestamp
    PlcValue lastValue;         // Last value (for change detection)

    IOEventTrigger()
        : type(IOEventType::INPUT_CHANGED),
          priority(EventPriority::NORMAL),
          enabled(true),
          threshold(PlcValueType::BOOL),
          thresholdRising(true),
          debounceMs(0),
          lastTrigger(0),
          lastValue(PlcValueType::BOOL) {}
};

// Scheduled time trigger
struct ScheduledTrigger {
    String name;                // Trigger name
    EventPriority priority;     // Priority level
    String programToRun;        // Program to start
    bool enabled;               // Is trigger enabled

    // Time/Date configuration (JSON format)
    // {"hour": 14, "minute": 30, "days": [1,2,3,4,5], "months": [1,2,3]}
    // days: 1=Monday, 7=Sunday (0 = all days)
    // months: 1-12 (0 = all months)
    int8_t hour;                // 0-23 (-1 = any hour)
    int8_t minute;              // 0-59 (-1 = any minute)
    std::vector<uint8_t> days;  // Days of week (empty = all days)
    std::vector<uint8_t> months;// Months (empty = all months)

    // State tracking
    unsigned long lastTrigger;  // Last trigger timestamp

    ScheduledTrigger()
        : priority(EventPriority::NORMAL),
          enabled(true),
          hour(-1),
          minute(-1),
          lastTrigger(0) {}
};

// Event history record
struct EventRecord {
    String triggerName;         // Name of the trigger
    String programName;         // Program that was started
    EventPriority priority;     // Event priority
    unsigned long timestamp;    // When event occurred
    String eventType;           // Human-readable event type
    String details;             // Additional details
    bool mqttPublished;         // Has been published to MQTT

    EventRecord()
        : priority(EventPriority::NORMAL),
          timestamp(0),
          mqttPublished(false) {}
};

class IOEventManager {
public:
    IOEventManager();
    ~IOEventManager();

    void begin();
    void loop(); // Call periodically to check events

    // ============================================================================
    // Configuration
    // ============================================================================

    /**
     * Load event configuration from JSON
     */
    bool loadConfig(const JsonObject& config);

    /**
     * Load configuration from file
     */
    bool loadConfigFromFile(const String& filepath);

    /**
     * Save configuration to file
     */
    bool saveConfig(const String& filepath);

    // ============================================================================
    // IO Event Triggers
    // ============================================================================

    /**
     * Add I/O event trigger
     */
    bool addIOTrigger(const IOEventTrigger& trigger);

    /**
     * Remove I/O event trigger
     */
    bool removeIOTrigger(const String& name);

    /**
     * Get I/O trigger by name
     */
    IOEventTrigger* getIOTrigger(const String& name);

    /**
     * Enable/disable I/O trigger
     */
    bool setIOTriggerEnabled(const String& name, bool enabled);

    /**
     * Get all I/O triggers
     */
    std::vector<String> getIOTriggerNames() const;

    // ============================================================================
    // Scheduled Triggers
    // ============================================================================

    /**
     * Add scheduled trigger
     */
    bool addScheduledTrigger(const ScheduledTrigger& trigger);

    /**
     * Remove scheduled trigger
     */
    bool removeScheduledTrigger(const String& name);

    /**
     * Get scheduled trigger by name
     */
    ScheduledTrigger* getScheduledTrigger(const String& name);

    /**
     * Enable/disable scheduled trigger
     */
    bool setScheduledTriggerEnabled(const String& name, bool enabled);

    /**
     * Get all scheduled triggers
     */
    std::vector<String> getScheduledTriggerNames() const;

    // ============================================================================
    // Event History
    // ============================================================================

    /**
     * Get event history (unread events)
     */
    std::vector<EventRecord> getEventHistory(bool unreadOnly = true);

    /**
     * Mark events as read (published to MQTT)
     */
    void markEventsAsRead();

    /**
     * Clear event history
     */
    void clearHistory();

    /**
     * Get event history statistics
     */
    struct EventStats {
        uint32_t totalEvents;
        uint32_t criticalEvents;
        uint32_t normalEvents;
        uint32_t unreadEvents;
        unsigned long lastEventTime;
    };

    EventStats getStatistics() const;

    /**
     * Serialize event history to JSON for MQTT publishing
     * @param unreadOnly - only include unread events
     * @return JSON string with event array
     */
    String serializeEventsToJson(bool unreadOnly = true);

    // ============================================================================
    // Integration Hooks
    // ============================================================================

    void setDeviceRegistry(DeviceRegistry* registry);
    void setPlcEngine(PlcEngine* engine);
    void setTimeManager(TimeManager* manager);

private:
    // Integration references
    DeviceRegistry* deviceRegistry;
    PlcEngine* plcEngine;
    TimeManager* timeManager;

    // Trigger storage
    std::map<String, IOEventTrigger> ioTriggers;
    std::map<String, ScheduledTrigger> scheduledTriggers;

    // Event history - circular buffer (100 events)
    static const size_t MAX_HISTORY = 100;
    EventRecord eventHistory[MAX_HISTORY];
    size_t historyHead;  // Next write position
    size_t historyCount; // Number of events stored

    // Statistics
    EventStats stats;

    // Processing methods
    void checkIOEvents();
    void checkScheduledEvents();
    bool shouldTriggerIO(IOEventTrigger& trigger);
    bool shouldTriggerScheduled(ScheduledTrigger& trigger);
    void executeEvent(const String& triggerName, const String& programName,
                     EventPriority priority, const String& eventType, const String& details);
    void addToHistory(const EventRecord& record);

    // Helper methods
    bool compareThreshold(const PlcValue& currentValue, const PlcValue& threshold, bool rising);
    bool isScheduleMatch(const ScheduledTrigger& trigger, const tm& currentTime);
    String getEventTypeString(IOEventType type);
};

#endif // IO_EVENT_MANAGER_H
