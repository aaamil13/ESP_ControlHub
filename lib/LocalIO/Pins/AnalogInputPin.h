#ifndef ANALOG_INPUT_PIN_H
#define ANALOG_INPUT_PIN_H

#include "../IOPinBase.h"
#include <driver/adc.h>
#include <vector>

/**
 * @brief Analog Input Pin with ADC
 *
 * Supports:
 * - 12-bit ADC (0-4095)
 * - Multiple voltage ranges (0-1.1V, 0-2.2V, 0-3.3V, 0-6V)
 * - Moving average filter
 * - Calibration (offset + scale)
 * - Engineering units conversion
 */
class AnalogInputPin : public IOPinBase {
public:
    AnalogInputPin(const String& name, const AnalogInputConfig& config)
        : IOPinBase(name, IOPinType::ANALOG_INPUT),
          config(config),
          adcChannel(ADC1_CHANNEL_0) {

        // Initialize filter buffer
        if (config.filterSamples > 0) {
            filterBuffer.reserve(config.filterSamples);
        }
    }

    bool begin() override {
        // Map GPIO to ADC channel
        if (!mapGPIOtoADC()) {
            currentState.isValid = false;
            currentState.errorMessage = "Invalid ADC pin";
            return false;
        }

        // Configure ADC
        adc1_config_width(ADC_WIDTH_BIT_12); // 12-bit resolution (0-4095)

        // Set attenuation based on range
        adc_atten_t attenuation;
        switch (config.range) {
            case AnalogInputRange::RANGE_0_1V:
                attenuation = ADC_ATTEN_DB_0;   // 0-1.1V
                break;
            case AnalogInputRange::RANGE_0_2V:
                attenuation = ADC_ATTEN_DB_2_5; // 0-2.2V
                break;
            case AnalogInputRange::RANGE_0_6V:
                attenuation = ADC_ATTEN_DB_11;  // 0-6V (with external divider)
                break;
            default:
                attenuation = ADC_ATTEN_DB_6;   // 0-3.3V (default)
                break;
        }

        adc1_config_channel_atten(adcChannel, attenuation);

        // Initialize state
        update();

        currentState.isValid = true;
        return true;
    }

    void update() override {
        if (!enabled || !currentState.isValid) return;

        // Read raw ADC value
        int rawADC = adc1_get_raw(adcChannel);

        // Apply filter
        filterBuffer.push_back(rawADC);
        if (filterBuffer.size() > config.filterSamples) {
            filterBuffer.erase(filterBuffer.begin());
        }

        // Calculate filtered average
        float sum = 0.0f;
        for (int val : filterBuffer) {
            sum += val;
        }
        float filteredADC = sum / filterBuffer.size();

        // Convert to voltage
        float voltage = adcToVoltage(filteredADC);

        // Apply calibration
        float calibratedVoltage = (voltage * config.calibrationScale) + config.calibrationOffset;

        // Convert to engineering units
        float engineeringValue = voltageToEngineering(calibratedVoltage);

        // Update state
        currentState.floatValue = engineeringValue;
        currentState.intValue = (int32_t)filteredADC;
        currentState.boolValue = (engineeringValue > 0.0f);
        currentState.lastUpdate = millis();
    }

    IOPinState getState() const override {
        return currentState;
    }

    /**
     * @brief Get raw ADC value (0-4095)
     */
    int getRawADC() const {
        return currentState.intValue;
    }

    /**
     * @brief Get voltage value
     */
    float getVoltage() const {
        return adcToVoltage(currentState.intValue);
    }

    String getConfigJson() const override {
        char buffer[384];
        snprintf(buffer, sizeof(buffer),
                 "{\"name\":\"%s\",\"type\":\"ANALOG_INPUT\",\"pin\":%d,"
                 "\"range\":\"%s\",\"resolution\":%u,\"sampleRate\":%u,"
                 "\"filter\":%u,\"calibOffset\":%.3f,\"calibScale\":%.3f,"
                 "\"minValue\":%.2f,\"maxValue\":%.2f}",
                 pinName.c_str(), config.pin,
                 getRangeString().c_str(),
                 config.resolution, config.sampleRate, config.filterSamples,
                 config.calibrationOffset, config.calibrationScale,
                 config.minValue, config.maxValue);
        return String(buffer);
    }

private:
    AnalogInputConfig config;
    adc1_channel_t adcChannel;
    std::vector<int> filterBuffer;

    bool mapGPIOtoADC() {
        // Map GPIO pin to ADC1 channel
        // ESP32 ADC1: GPIO32-39 (channels 4-7, 0-3)
        switch (config.pin) {
            case 36: adcChannel = ADC1_CHANNEL_0; return true; // VP
            case 37: adcChannel = ADC1_CHANNEL_1; return true; // CAPP (not exposed)
            case 38: adcChannel = ADC1_CHANNEL_2; return true; // CAPN (not exposed)
            case 39: adcChannel = ADC1_CHANNEL_3; return true; // VN
            case 32: adcChannel = ADC1_CHANNEL_4; return true;
            case 33: adcChannel = ADC1_CHANNEL_5; return true;
            case 34: adcChannel = ADC1_CHANNEL_6; return true;
            case 35: adcChannel = ADC1_CHANNEL_7; return true;
            default: return false;
        }
    }

    float adcToVoltage(float adcValue) const {
        // ADC is 12-bit (0-4095)
        float maxVoltage;
        switch (config.range) {
            case AnalogInputRange::RANGE_0_1V:
                maxVoltage = 1.1f;
                break;
            case AnalogInputRange::RANGE_0_2V:
                maxVoltage = 2.2f;
                break;
            case AnalogInputRange::RANGE_0_6V:
                maxVoltage = 6.0f;
                break;
            default:
                maxVoltage = 3.3f;
                break;
        }

        return (adcValue / 4095.0f) * maxVoltage;
    }

    float voltageToEngineering(float voltage) const {
        // Map voltage to engineering units
        // Assumes linear relationship
        float voltageRange;
        switch (config.range) {
            case AnalogInputRange::RANGE_0_1V: voltageRange = 1.1f; break;
            case AnalogInputRange::RANGE_0_2V: voltageRange = 2.2f; break;
            case AnalogInputRange::RANGE_0_6V: voltageRange = 6.0f; break;
            default: voltageRange = 3.3f; break;
        }

        float normalized = voltage / voltageRange; // 0.0 to 1.0
        return config.minValue + (normalized * (config.maxValue - config.minValue));
    }

    String getRangeString() const {
        switch (config.range) {
            case AnalogInputRange::RANGE_0_1V: return "0-1.1V";
            case AnalogInputRange::RANGE_0_2V: return "0-2.2V";
            case AnalogInputRange::RANGE_0_6V: return "0-6V";
            default: return "0-3.3V";
        }
    }
};

#endif // ANALOG_INPUT_PIN_H
