#ifndef IO_PIN_BASE_H
#define IO_PIN_BASE_H

#include <Arduino.h>
#include "LocalIOTypes.h"

/**
 * @brief Base class for all IO pins
 *
 * Provides common interface for different IO pin types.
 * All specific pin classes inherit from this base.
 */
class IOPinBase {
public:
    IOPinBase(const String& name, IOPinType type)
        : pinName(name), pinType(type), enabled(true) {}

    virtual ~IOPinBase() {}

    /**
     * @brief Initialize the IO pin
     * @return true if initialization successful
     */
    virtual bool begin() = 0;

    /**
     * @brief Update pin state (read inputs, update outputs)
     * Called periodically by LocalIOManager
     */
    virtual void update() = 0;

    /**
     * @brief Get current pin state
     */
    virtual IOPinState getState() const = 0;

    /**
     * @brief Set pin value (for outputs only)
     * @param value Value to set (bool or float depending on type)
     * @return true if successful
     */
    virtual bool setValue(const IOPinState& value) { return false; }

    /**
     * @brief Get pin name
     */
    String getName() const { return pinName; }

    /**
     * @brief Get pin type
     */
    IOPinType getType() const { return pinType; }

    /**
     * @brief Enable/disable pin
     */
    void setEnabled(bool enable) { enabled = enable; }
    bool isEnabled() const { return enabled; }

    /**
     * @brief Get pin configuration as JSON
     */
    virtual String getConfigJson() const = 0;

protected:
    String pinName;
    IOPinType pinType;
    bool enabled;
    IOPinState currentState;
};

#endif // IO_PIN_BASE_H
