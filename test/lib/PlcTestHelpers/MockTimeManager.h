#ifndef MOCK_TIME_MANAGER_H
#define MOCK_TIME_MANAGER_H

#include <Arduino.h>
#include "TimeManager.h"

/**
 * @brief Mock Time Manager for deterministic testing
 * 
 * Allows controlling time manually during tests to verify
 * timer-based logic without waiting for real time.
 */
class MockTimeManager : public TimeManager {
private:
    unsigned long mockMillis = 0;

public:
    MockTimeManager() : TimeManager() {}

    /**
     * @brief Override millis() to return mock time
     */
    unsigned long millis() {
        return mockMillis;
    }

    /**
     * @brief Advance time by specific amount
     * @param ms Milliseconds to advance
     */
    void advance(unsigned long ms) {
        mockMillis += ms;
    }

    /**
     * @brief Set specific time
     * @param ms Absolute time in milliseconds
     */
    void setTime(unsigned long ms) {
        mockMillis = ms;
    }

    /**
     * @brief Reset time to zero
     */
    void reset() {
        mockMillis = 0;
    }
};

#endif // MOCK_TIME_MANAGER_H
