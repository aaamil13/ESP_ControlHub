#ifndef DIGITAL_INPUT_PIN_H
#define DIGITAL_INPUT_PIN_H

#include "../IOPinBase.h"
#include <vector>

/**
 * @brief Digital Input Pin with debounce and filtering
 *
 * Supports:
 * - Direct GPIO read
 * - Software debounce
 * - Moving average filter
 * - Edge detection
 * - Pull-up/pull-down configuration
 */
class DigitalInputPin : public IOPinBase {
public:
    DigitalInputPin(const String& name, const DigitalInputConfig& config)
        : IOPinBase(name, IOPinType::DIGITAL_INPUT),
          config(config),
          lastRawValue(false),
          lastDebouncedValue(false),
          lastDebounceTime(0),
          lastEdgeTime(0),
          edgeDetected(false) {

        // Initialize filter buffer
        if (config.filterSamples > 0) {
            filterBuffer.reserve(config.filterSamples);
        }
    }

    bool begin() override {
        // Configure pin mode
        uint8_t mode;
        switch (config.mode) {
            case DigitalInputMode::INPUT_PULLUP:
                mode = INPUT_PULLUP;
                break;
            case DigitalInputMode::INPUT_PULLDOWN:
                mode = INPUT_PULLDOWN;
                break;
            default:
                mode = INPUT;
                break;
        }

        pinMode(config.pin, mode);

        // Initialize state
        update();

        currentState.isValid = true;
        return true;
    }

    void update() override {
        if (!enabled) return;

        unsigned long now = millis();

        // Read raw value
        bool rawValue = digitalRead(config.pin);
        if (config.invert) {
            rawValue = !rawValue;
        }

        // Apply debounce
        bool debouncedValue = rawValue;
        if (config.debounceMs > 0) {
            if (rawValue != lastRawValue) {
                lastDebounceTime = now;
            }

            if ((now - lastDebounceTime) >= config.debounceMs) {
                debouncedValue = rawValue;
            } else {
                debouncedValue = lastDebouncedValue;
            }

            lastDebouncedValue = debouncedValue;
        }

        lastRawValue = rawValue;

        // Apply filter
        bool filteredValue = debouncedValue;
        if (config.filterSamples > 0) {
            filterBuffer.push_back(debouncedValue ? 1 : 0);
            if (filterBuffer.size() > config.filterSamples) {
                filterBuffer.erase(filterBuffer.begin());
            }

            // Calculate average
            int sum = 0;
            for (int val : filterBuffer) {
                sum += val;
            }
            filteredValue = (sum > (config.filterSamples / 2));
        }

        // Edge detection
        edgeDetected = false;
        if (config.edgeDetect != DigitalInputEdge::NONE) {
            bool prevValue = currentState.boolValue;
            bool rising = filteredValue && !prevValue;
            bool falling = !filteredValue && prevValue;

            if ((config.edgeDetect == DigitalInputEdge::RISING && rising) ||
                (config.edgeDetect == DigitalInputEdge::FALLING && falling) ||
                (config.edgeDetect == DigitalInputEdge::BOTH && (rising || falling))) {
                edgeDetected = true;
                lastEdgeTime = now;
            }
        }

        // Update state
        currentState.boolValue = filteredValue;
        currentState.floatValue = filteredValue ? 1.0f : 0.0f;
        currentState.intValue = filteredValue ? 1 : 0;
        currentState.lastUpdate = now;
    }

    IOPinState getState() const override {
        return currentState;
    }

    /**
     * @brief Check if edge was detected since last call
     */
    bool getEdgeDetected() {
        bool result = edgeDetected;
        edgeDetected = false;
        return result;
    }

    /**
     * @brief Get time of last edge detection
     */
    unsigned long getLastEdgeTime() const {
        return lastEdgeTime;
    }

    String getConfigJson() const override {
        char buffer[256];
        snprintf(buffer, sizeof(buffer),
                 "{\"name\":\"%s\",\"type\":\"DIGITAL_INPUT\",\"pin\":%d,"
                 "\"mode\":\"%s\",\"invert\":%s,\"debounce\":%u,\"filter\":%u}",
                 pinName.c_str(), config.pin,
                 getModeString().c_str(),
                 config.invert ? "true" : "false",
                 config.debounceMs, config.filterSamples);
        return String(buffer);
    }

private:
    DigitalInputConfig config;
    bool lastRawValue;
    bool lastDebouncedValue;
    unsigned long lastDebounceTime;
    unsigned long lastEdgeTime;
    bool edgeDetected;
    std::vector<int> filterBuffer;

    String getModeString() const {
        switch (config.mode) {
            case DigitalInputMode::INPUT_PULLUP: return "INPUT_PULLUP";
            case DigitalInputMode::INPUT_PULLDOWN: return "INPUT_PULLDOWN";
            default: return "INPUT";
        }
    }
};

#endif // DIGITAL_INPUT_PIN_H
