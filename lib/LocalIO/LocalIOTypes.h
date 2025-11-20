#ifndef LOCAL_IO_TYPES_H
#define LOCAL_IO_TYPES_H

#include <Arduino.h>

// ============================================================================
// IO Pin Types
// ============================================================================

enum class IOPinType : uint8_t {
    // Digital
    DIGITAL_INPUT,              // Simple GPIO input
    DIGITAL_INPUT_DEBOUNCED,    // Input with software debounce
    DIGITAL_INPUT_FILTERED,     // Input with filtering (moving average)
    DIGITAL_OUTPUT,             // Simple GPIO output
    DIGITAL_OUTPUT_PULSE,       // Pulse output (one-shot)

    // Analog
    ANALOG_INPUT,               // ADC input (raw)
    ANALOG_INPUT_FILTERED,      // ADC with moving average filter
    ANALOG_INPUT_CALIBRATED,    // ADC with voltage/current calibration
    ANALOG_OUTPUT_DAC,          // True DAC output (GPIO25/26 only)
    ANALOG_OUTPUT_PWM,          // PWM as analog output

    // PWM
    PWM_OUTPUT,                 // Standard PWM output

    // Pulse/Frequency
    PULSE_COUNTER,              // Count pulses (rising/falling/both edges)
    FREQUENCY_INPUT,            // Measure frequency
    PERIOD_INPUT,               // Measure period

    // Special
    TOUCH_SENSOR,               // Touch sensor input
    HALL_SENSOR,                // Hall sensor (internal)
    TEMPERATURE_SENSOR          // Temperature sensor (internal)
};

// ============================================================================
// Digital Input Configuration
// ============================================================================

enum class DigitalInputMode : uint8_t {
    MODE_INPUT,                      // High impedance input
    MODE_INPUT_PULLUP,               // Input with internal pullup
    MODE_INPUT_PULLDOWN              // Input with internal pulldown
};

enum class DigitalInputEdge : uint8_t {
    NONE,                       // No edge detection
    EDGE_RISING,                     // Rising edge
    EDGE_FALLING,                    // Falling edge
    BOTH                        // Both edges
};

struct DigitalInputConfig {
    uint8_t pin;
    DigitalInputMode mode;
    bool invert;                // Invert logic (true = active low)
    uint16_t debounceMs;        // Debounce time in milliseconds
    uint8_t filterSamples;      // Number of samples for filtering (0 = no filter)
    DigitalInputEdge edgeDetect;// Edge detection mode

    DigitalInputConfig()
        : pin(0), mode(DigitalInputMode::MODE_INPUT),
          invert(false), debounceMs(50), filterSamples(0),
          edgeDetect(DigitalInputEdge::NONE) {}
};

// ============================================================================
// Digital Output Configuration
// ============================================================================

enum class DigitalOutputMode : uint8_t {
    STANDARD,                   // Normal output
    PULSE,                      // Pulse output (one-shot)
    TOGGLE                      // Toggle on command
};

struct DigitalOutputConfig {
    uint8_t pin;
    bool invert;                // Invert logic (true = active low)
    bool initialState;          // Initial state on boot
    uint32_t pulseWidthMs;      // Pulse width for PULSE mode
    bool safeState;             // Safe state on error/shutdown

    DigitalOutputConfig()
        : pin(0), invert(false), initialState(false),
          pulseWidthMs(100), safeState(false) {}
};

// ============================================================================
// Analog Input Configuration
// ============================================================================

enum class AnalogInputRange : uint8_t {
    RANGE_0_1V,                 // 0-1.1V (internal attenuation)
    RANGE_0_2V,                 // 0-2.2V
    RANGE_0_3V3,                // 0-3.3V (default)
    RANGE_0_6V                  // 0-6V (external divider)
};

struct AnalogInputConfig {
    uint8_t pin;
    AnalogInputRange range;
    uint8_t resolution;         // ADC resolution in bits (9-12)
    uint16_t sampleRate;        // Sample rate in Hz
    uint8_t filterSamples;      // Moving average filter size
    float calibrationOffset;    // Calibration offset
    float calibrationScale;     // Calibration scale factor
    float minValue;             // Minimum engineering value
    float maxValue;             // Maximum engineering value

    AnalogInputConfig()
        : pin(0), range(AnalogInputRange::RANGE_0_3V3),
          resolution(12), sampleRate(100), filterSamples(10),
          calibrationOffset(0.0f), calibrationScale(1.0f),
          minValue(0.0f), maxValue(100.0f) {}
};

// ============================================================================
// Analog Output Configuration
// ============================================================================

struct AnalogOutputConfig {
    uint8_t pin;
    bool useDAC;                // true = use DAC, false = use PWM
    uint8_t resolution;         // DAC: 8-bit, PWM: configurable
    uint16_t pwmFrequency;      // PWM frequency (if useDAC = false)
    float minValue;             // Minimum engineering value
    float maxValue;             // Maximum engineering value
    float initialValue;         // Initial value on boot

    AnalogOutputConfig()
        : pin(0), useDAC(false), resolution(8),
          pwmFrequency(1000), minValue(0.0f), maxValue(100.0f),
          initialValue(0.0f) {}
};

// ============================================================================
// PWM Output Configuration
// ============================================================================

struct PWMOutputConfig {
};

// ============================================================================
// IO Pin State
// ============================================================================

struct IOPinState {
    bool boolValue;             // For digital I/O
    float floatValue;           // For analog I/O
    int32_t intValue;           // For counters
    unsigned long lastUpdate;   // Last update timestamp
    bool isValid;               // Is value valid?
    String errorMessage;        // Error message (if any)

    IOPinState()
        : boolValue(false), floatValue(0.0f), intValue(0),
          lastUpdate(0), isValid(false) {}
};

#endif // LOCAL_IO_TYPES_H