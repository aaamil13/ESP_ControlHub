#ifndef DIGITAL_OUTPUT_PIN_H
#define DIGITAL_OUTPUT_PIN_H

#include "../IOPinBase.h"

/**
 * @brief Digital Output Pin
 *
 * Supports:
 * - Standard output
 * - Pulse output (one-shot)
 * - Toggle output
 * - Safe state on error
 */
class DigitalOutputPin : public IOPinBase {
public:
    DigitalOutputPin(const String& name, const DigitalOutputConfig& config)
        : IOPinBase(name, IOPinType::DIGITAL_OUTPUT),
          config(config),
          pulseActive(false),
          pulseStartTime(0) {}

    bool begin() override {
        pinMode(config.pin, OUTPUT);

        // Set initial state
        bool initialValue = config.initialState;
        if (config.invert) {
            initialValue = !initialValue;
        }
        digitalWrite(config.pin, initialValue ? HIGH : LOW);

        currentState.boolValue = config.initialState;
        currentState.floatValue = config.initialState ? 1.0f : 0.0f;
        currentState.intValue = config.initialState ? 1 : 0;
        currentState.isValid = true;
        currentState.lastUpdate = millis();

        return true;
    }

    void update() override {
        if (!enabled) return;

        unsigned long now = millis();

        // Handle pulse mode
        if (pulseActive) {
            if (now - pulseStartTime >= config.pulseWidthMs) {
                // Pulse completed
                pulseActive = false;
                setOutputState(false);
            }
        }

        currentState.lastUpdate = now;
    }

    IOPinState getState() const override {
        return currentState;
    }

    bool setValue(const IOPinState& value) override {
        if (!enabled) {
            return false;
        }

        setOutputState(value.boolValue);
        return true;
    }

    /**
     * @brief Trigger pulse output
     * @param durationMs Pulse duration in milliseconds (0 = use config value)
     */
    void triggerPulse(uint32_t durationMs = 0) {
        if (!enabled) return;

        pulseActive = true;
        pulseStartTime = millis();

        if (durationMs > 0) {
            config.pulseWidthMs = durationMs;
        }

        setOutputState(true);
    }

    /**
     * @brief Toggle output state
     */
    void toggle() {
        if (!enabled) return;

        setOutputState(!currentState.boolValue);
    }

    /**
     * @brief Set safe state (called on error/shutdown)
     */
    void setSafeState() {
        setOutputState(config.safeState);
    }

    String getConfigJson() const override {
        char buffer[256];
        snprintf(buffer, sizeof(buffer),
                 "{\"name\":\"%s\",\"type\":\"DIGITAL_OUTPUT\",\"pin\":%d,"
                 "\"invert\":%s,\"initialState\":%s,\"pulseWidth\":%u,\"safeState\":%s}",
                 pinName.c_str(), config.pin,
                 config.invert ? "true" : "false",
                 config.initialState ? "true" : "false",
                 config.pulseWidthMs,
                 config.safeState ? "true" : "false");
        return String(buffer);
    }

private:
    DigitalOutputConfig config;
    bool pulseActive;
    unsigned long pulseStartTime;

    void setOutputState(bool state) {
        currentState.boolValue = state;
        currentState.floatValue = state ? 1.0f : 0.0f;
        currentState.intValue = state ? 1 : 0;

        bool physicalState = state;
        if (config.invert) {
            physicalState = !physicalState;
        }

        digitalWrite(config.pin, physicalState ? HIGH : LOW);
        currentState.lastUpdate = millis();
    }
};

#endif // DIGITAL_OUTPUT_PIN_H
