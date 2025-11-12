#ifndef PULSE_COUNTER_PIN_H
#define PULSE_COUNTER_PIN_H

#include "../IOPinBase.h"
#include <driver/pcnt.h>

/**
 * @brief Pulse Counter Pin using ESP32 PCNT
 *
 * Supports:
 * - Hardware pulse counting (up to 8 channels)
 * - Rising/falling/both edge detection
 * - Frequency measurement
 * - Period measurement
 * - Glitch filter
 * - 16-bit counter with overflow handling
 */
class PulseCounterPin : public IOPinBase {
public:
    PulseCounterPin(const String& name, const PulseCounterConfig& config)
        : IOPinBase(name, IOPinType::PULSE_COUNTER),
          config(config),
          pcntUnit(PCNT_UNIT_0),
          totalCount(0),
          lastCount(0),
          lastSampleTime(0),
          frequency(0.0f),
          period(0.0f) {}

    bool begin() override {
        // Find available PCNT unit
        if (!findAvailablePCNTUnit()) {
            currentState.isValid = false;
            currentState.errorMessage = "No available PCNT units";
            return false;
        }

        // Configure PCNT unit
        pcnt_config_t pcnt_config = {
            .pulse_gpio_num = config.pin,
            .ctrl_gpio_num = PCNT_PIN_NOT_USED,
            .lctrl_mode = PCNT_MODE_KEEP,
            .hctrl_mode = PCNT_MODE_KEEP,
            .pos_mode = PCNT_COUNT_DIS,
            .neg_mode = PCNT_COUNT_DIS,
            .counter_h_lim = 32767,
            .counter_l_lim = -32768,
            .unit = pcntUnit,
            .channel = PCNT_CHANNEL_0,
        };

        // Configure edge counting
        switch (config.edge) {
            case PulseCounterEdge::RISING:
                pcnt_config.pos_mode = PCNT_COUNT_INC;
                pcnt_config.neg_mode = PCNT_COUNT_DIS;
                break;
            case PulseCounterEdge::FALLING:
                pcnt_config.pos_mode = PCNT_COUNT_DIS;
                pcnt_config.neg_mode = PCNT_COUNT_INC;
                break;
            case PulseCounterEdge::BOTH:
                pcnt_config.pos_mode = PCNT_COUNT_INC;
                pcnt_config.neg_mode = PCNT_COUNT_INC;
                break;
        }

        esp_err_t ret = pcnt_unit_config(&pcnt_config);
        if (ret != ESP_OK) {
            currentState.isValid = false;
            currentState.errorMessage = "PCNT config failed";
            return false;
        }

        // Set glitch filter
        if (config.enableFilter) {
            pcnt_set_filter_value(pcntUnit, config.filterThresholdNs);
            pcnt_filter_enable(pcntUnit);
        }

        // Initialize counter
        pcnt_counter_pause(pcntUnit);
        pcnt_counter_clear(pcntUnit);
        pcnt_counter_resume(pcntUnit);

        lastSampleTime = millis();
        currentState.isValid = true;

        return true;
    }

    void update() override {
        if (!enabled || !currentState.isValid) return;

        unsigned long now = millis();

        // Read counter
        int16_t count;
        pcnt_get_counter_value(pcntUnit, &count);

        // Calculate delta
        int32_t delta = count - lastCount;
        lastCount = count;

        // Update total count
        totalCount += delta;

        // Calculate frequency if in FREQUENCY mode
        if (config.mode == PulseCounterMode::FREQUENCY) {
            unsigned long elapsed = now - lastSampleTime;
            if (elapsed >= config.sampleWindowMs) {
                // Frequency = pulses / time
                frequency = (float)delta / (elapsed / 1000.0f);
                lastSampleTime = now;

                // Reset counter for next sample
                pcnt_counter_clear(pcntUnit);
                lastCount = 0;
            }
        }

        // Calculate period if in PERIOD mode
        if (config.mode == PulseCounterMode::PERIOD && delta > 0) {
            unsigned long elapsed = now - lastSampleTime;
            // Period = time / pulses
            period = (float)elapsed / delta;
            lastSampleTime = now;
        }

        // Update state
        currentState.intValue = totalCount;
        currentState.floatValue = (config.mode == PulseCounterMode::FREQUENCY) ? frequency : period;
        currentState.boolValue = (count > 0);
        currentState.lastUpdate = now;
    }

    IOPinState getState() const override {
        return currentState;
    }

    /**
     * @brief Get total pulse count
     */
    int32_t getTotalCount() const {
        return totalCount;
    }

    /**
     * @brief Get current frequency (Hz)
     */
    float getFrequency() const {
        return frequency;
    }

    /**
     * @brief Get current period (ms)
     */
    float getPeriod() const {
        return period;
    }

    /**
     * @brief Reset counter to zero
     */
    void resetCounter() {
        if (!currentState.isValid) return;

        pcnt_counter_clear(pcntUnit);
        totalCount = 0;
        lastCount = 0;
        lastSampleTime = millis();
    }

    String getConfigJson() const override {
        char buffer[256];
        snprintf(buffer, sizeof(buffer),
                 "{\"name\":\"%s\",\"type\":\"PULSE_COUNTER\",\"pin\":%d,"
                 "\"edge\":\"%s\",\"mode\":\"%s\",\"sampleWindow\":%u,"
                 "\"filter\":%s,\"totalCount\":%d,\"frequency\":%.2f}",
                 pinName.c_str(), config.pin,
                 getEdgeString().c_str(), getModeString().c_str(),
                 config.sampleWindowMs,
                 config.enableFilter ? "true" : "false",
                 totalCount, frequency);
        return String(buffer);
    }

private:
    PulseCounterConfig config;
    pcnt_unit_t pcntUnit;
    int32_t totalCount;
    int16_t lastCount;
    unsigned long lastSampleTime;
    float frequency;
    float period;

    static bool pcntUnitsUsed[PCNT_UNIT_MAX];

    bool findAvailablePCNTUnit() {
        for (int i = 0; i < PCNT_UNIT_MAX; i++) {
            if (!pcntUnitsUsed[i]) {
                pcntUnit = (pcnt_unit_t)i;
                pcntUnitsUsed[i] = true;
                return true;
            }
        }
        return false;
    }

    String getEdgeString() const {
        switch (config.edge) {
            case PulseCounterEdge::RISING: return "RISING";
            case PulseCounterEdge::FALLING: return "FALLING";
            case PulseCounterEdge::BOTH: return "BOTH";
            default: return "UNKNOWN";
        }
    }

    String getModeString() const {
        switch (config.mode) {
            case PulseCounterMode::COUNTER: return "COUNTER";
            case PulseCounterMode::FREQUENCY: return "FREQUENCY";
            case PulseCounterMode::PERIOD: return "PERIOD";
            default: return "UNKNOWN";
        }
    }
};

// Static member initialization
bool PulseCounterPin::pcntUnitsUsed[PCNT_UNIT_MAX] = {false};

#endif // PULSE_COUNTER_PIN_H
