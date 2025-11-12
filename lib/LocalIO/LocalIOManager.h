#ifndef LOCAL_IO_MANAGER_H
#define LOCAL_IO_MANAGER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <map>
#include <vector>
#include "IOPinBase.h"
#include "Pins/DigitalInputPin.h"
#include "Pins/DigitalOutputPin.h"
#include "Pins/AnalogInputPin.h"
#include "Pins/PWMOutputPin.h"
#include "Pins/PulseCounterPin.h"
#include "../Core/StreamLogger.h"

// Forward declaration
class PlcMemory;

extern StreamLogger* EspHubLog;

/**
 * @brief Local IO Manager
 *
 * Manages all local hardware IO pins on ESP32.
 * Integrates with PLC engine for automatic I/O mapping.
 *
 * Features:
 * - Configurable via JSON
 * - Multiple IO types (DI, DO, AI, AO, PWM, Pulse Counter)
 * - Automatic PLC variable mapping
 * - Periodic I/O scanning
 * - Error handling and diagnostics
 */
class LocalIOManager {
public:
    LocalIOManager();
    ~LocalIOManager();

    // ============================================================================
    // Initialization
    // ============================================================================

    /**
     * @brief Initialize IO manager
     */
    void begin();

    /**
     * @brief Main loop - updates all IO pins
     * Call this periodically (e.g., every 10ms)
     */
    void loop();

    /**
     * @brief Load IO configuration from JSON
     * @param jsonConfig JSON configuration string
     * @return true if configuration loaded successfully
     */
    bool loadConfig(const String& jsonConfig);

    /**
     * @brief Load IO configuration from file
     * @param filename Path to configuration file
     * @return true if configuration loaded successfully
     */
    bool loadConfigFromFile(const String& filename);

    // ============================================================================
    // PLC Integration
    // ============================================================================

    /**
     * @brief Set PLC memory for automatic variable mapping
     * @param plcMemory Pointer to PLC memory
     */
    void setPlcMemory(PlcMemory* plcMemory);

    /**
     * @brief Synchronize IO with PLC variables
     * Reads inputs and writes to PLC variables
     * Reads PLC variables and writes to outputs
     */
    void syncWithPLC();

    /**
     * @brief Enable/disable auto PLC sync
     * @param enable Auto sync enabled
     */
    void setAutoSync(bool enable) { autoSyncEnabled = enable; }

    /**
     * @brief Check if auto sync is enabled
     */
    bool isAutoSyncEnabled() const { return autoSyncEnabled; }

    // ============================================================================
    // Pin Management
    // ============================================================================

    /**
     * @brief Get IO pin by name
     * @param name Pin name
     * @return Pointer to pin or nullptr if not found
     */
    IOPinBase* getPin(const String& name);

    /**
     * @brief Get all pins
     */
    const std::map<String, IOPinBase*>& getAllPins() const { return ioPins; }

    /**
     * @brief Get pin count
     */
    size_t getPinCount() const { return ioPins.size(); }

    /**
     * @brief Check if pin exists
     */
    bool hasPin(const String& name) const { return ioPins.count(name) > 0; }

    // ============================================================================
    // Digital I/O
    // ============================================================================

    /**
     * @brief Read digital input
     * @param name Pin name
     * @return Pin state (true/false)
     */
    bool readDigital(const String& name);

    /**
     * @brief Write digital output
     * @param name Pin name
     * @param value Value to write
     * @return true if successful
     */
    bool writeDigital(const String& name, bool value);

    // ============================================================================
    // Analog I/O
    // ============================================================================

    /**
     * @brief Read analog input
     * @param name Pin name
     * @return Analog value (engineering units)
     */
    float readAnalog(const String& name);

    /**
     * @brief Write analog output
     * @param name Pin name
     * @param value Value to write (engineering units)
     * @return true if successful
     */
    bool writeAnalog(const String& name, float value);

    // ============================================================================
    // PWM
    // ============================================================================

    /**
     * @brief Set PWM duty cycle
     * @param name Pin name
     * @param dutyCycle Duty cycle (0.0 - 100.0%)
     * @return true if successful
     */
    bool setPWMDutyCycle(const String& name, float dutyCycle);

    /**
     * @brief Set PWM frequency
     * @param name Pin name
     * @param frequency Frequency in Hz
     * @return true if successful
     */
    bool setPWMFrequency(const String& name, uint16_t frequency);

    // ============================================================================
    // Pulse Counter
    // ============================================================================

    /**
     * @brief Get pulse counter value
     * @param name Pin name
     * @return Total pulse count
     */
    int32_t getPulseCount(const String& name);

    /**
     * @brief Get pulse counter frequency
     * @param name Pin name
     * @return Frequency in Hz
     */
    float getPulseFrequency(const String& name);

    /**
     * @brief Reset pulse counter
     * @param name Pin name
     * @return true if successful
     */
    bool resetPulseCounter(const String& name);

    // ============================================================================
    // Status and Diagnostics
    // ============================================================================

    /**
     * @brief Get IO status as JSON
     */
    String getStatusJson() const;

    /**
     * @brief Get pin status as JSON
     * @param name Pin name
     */
    String getPinStatusJson(const String& name) const;

    /**
     * @brief Enable/disable all IO
     */
    void setEnabled(bool enable);

    /**
     * @brief Get memory usage
     */
    size_t getMemoryUsage() const;

    // ============================================================================
    // Safety
    // ============================================================================

    /**
     * @brief Set all outputs to safe state
     * Called on error or shutdown
     */
    void setSafeState();

private:
    // PLC Mapping Structure
    struct PLCMapping {
        String ioPinName;
        String plcVarName;
        String type; // "bool" or "real"
    };

    std::map<String, IOPinBase*> ioPins;
    std::vector<PLCMapping> inputMappings;
    std::vector<PLCMapping> outputMappings;
    PlcMemory* plcMemory;
    bool autoSyncEnabled;
    unsigned long lastUpdateTime;
    bool enabled;
    size_t errorCount;

    // Configuration helpers
    bool loadDigitalInput(const JsonObject& config);
    bool loadDigitalOutput(const JsonObject& config);
    bool loadAnalogInput(const JsonObject& config);
    bool loadAnalogOutput(const JsonObject& config);
    bool loadPWMOutput(const JsonObject& config);
    bool loadPulseCounter(const JsonObject& config);
    bool loadPLCMapping(const JsonObject& config);

    // Pin creation
    template<typename T>
    bool addPin(const String& name, T* pin) {
        if (ioPins.count(name) > 0) {
            EspHubLog->printf("ERROR: Pin '%s' already exists\n", name.c_str());
            delete pin;
            return false;
        }

        if (!pin->begin()) {
            EspHubLog->printf("ERROR: Failed to initialize pin '%s'\n", name.c_str());
            delete pin;
            return false;
        }

        ioPins[name] = pin;
        return true;
    }
};

#endif // LOCAL_IO_MANAGER_H
