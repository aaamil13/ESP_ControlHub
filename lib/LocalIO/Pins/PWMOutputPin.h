#ifndef PWM_OUTPUT_PIN_H
#define PWM_OUTPUT_PIN_H

#include "../IOPinBase.h"
#include <driver/ledc.h>

/**
 * @brief PWM Output Pin using ESP32 LEDC
 *
 * Supports:
 * - Configurable frequency (1Hz - 40MHz)
 * - Configurable resolution (1-16 bits)
 * - Duty cycle control (0-100%)
 * - 16 independent channels (8 high-speed + 8 low-speed)
 */
class PWMOutputPin : public IOPinBase {
public:
    PWMOutputPin(const String& name, const PWMOutputConfig& config)
        : IOPinBase(name, IOPinType::PWM_OUTPUT),
          config(config),
          currentDutyCycle(0.0f),
          maxDutyValue(0) {}

    bool begin() override {
        // Configure LEDC timer
        ledc_timer_config_t timer_config = {
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .duty_resolution = (ledc_timer_bit_t)config.resolution,
            .timer_num = (ledc_timer_t)(config.channel / 8), // Timer 0 for channels 0-7, Timer 1 for 8-15
            .freq_hz = config.frequency,
            .clk_cfg = LEDC_AUTO_CLK
        };

        esp_err_t ret = ledc_timer_config(&timer_config);
        if (ret != ESP_OK) {
            currentState.isValid = false;
            currentState.errorMessage = "LEDC timer config failed";
            return false;
        }

        // Configure LEDC channel
        ledc_channel_config_t channel_config = {
            .gpio_num = config.pin,
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel = (ledc_channel_t)config.channel,
            .intr_type = LEDC_INTR_DISABLE,
            .timer_sel = (ledc_timer_t)(config.channel / 8),
            .duty = 0,
            .hpoint = 0
        };

        ret = ledc_channel_config(&channel_config);
        if (ret != ESP_OK) {
            currentState.isValid = false;
            currentState.errorMessage = "LEDC channel config failed";
            return false;
        }

        // Calculate max duty value
        maxDutyValue = (1 << config.resolution) - 1;

        // Set initial duty cycle
        setDutyCycle(config.initialDutyCycle);

        currentState.isValid = true;
        return true;
    }

    void update() override {
        if (!enabled || !currentState.isValid) return;

        // PWM is hardware-driven, no periodic update needed
        // Just update timestamp
        currentState.lastUpdate = millis();
    }

    IOPinState getState() const override {
        return currentState;
    }

    bool setValue(const IOPinState& value) override {
        if (!enabled || !currentState.isValid) {
            return false;
        }

        // Interpret floatValue as duty cycle percentage (0.0 - 100.0)
        setDutyCycle(value.floatValue);
        return true;
    }

    /**
     * @brief Set PWM duty cycle
     * @param dutyCycle Duty cycle in percent (0.0 - 100.0)
     */
    void setDutyCycle(float dutyCycle) {
        if (!currentState.isValid) return;

        // Clamp to 0-100%
        if (dutyCycle < 0.0f) dutyCycle = 0.0f;
        if (dutyCycle > 100.0f) dutyCycle = 100.0f;

        currentDutyCycle = dutyCycle;

        // Calculate duty value
        uint32_t dutyValue = (uint32_t)((dutyCycle / 100.0f) * maxDutyValue);

        // Set duty
        ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)config.channel, dutyValue);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)config.channel);

        // Update state
        currentState.floatValue = dutyCycle;
        currentState.intValue = dutyValue;
        currentState.boolValue = (dutyCycle > 0.0f);
        currentState.lastUpdate = millis();
    }

    /**
     * @brief Set PWM frequency
     * @param frequency New frequency in Hz
     */
    void setFrequency(uint16_t frequency) {
        if (!currentState.isValid) return;

        config.frequency = frequency;

        ledc_set_freq(LEDC_LOW_SPEED_MODE,
                     (ledc_timer_t)(config.channel / 8),
                     frequency);

        // Restore duty cycle
        setDutyCycle(currentDutyCycle);
    }

    /**
     * @brief Get current duty cycle
     */
    float getDutyCycle() const {
        return currentDutyCycle;
    }

    /**
     * @brief Get current frequency
     */
    uint16_t getFrequency() const {
        return config.frequency;
    }

    /**
     * @brief Fade to target duty cycle
     * @param targetDutyCycle Target duty cycle (0-100%)
     * @param fadeTimeMs Fade time in milliseconds
     */
    void fadeTo(float targetDutyCycle, uint32_t fadeTimeMs) {
        if (!currentState.isValid) return;

        // Clamp target
        if (targetDutyCycle < 0.0f) targetDutyCycle = 0.0f;
        if (targetDutyCycle > 100.0f) targetDutyCycle = 100.0f;

        uint32_t targetDuty = (uint32_t)((targetDutyCycle / 100.0f) * maxDutyValue);

        ledc_set_fade_with_time(LEDC_LOW_SPEED_MODE,
                               (ledc_channel_t)config.channel,
                               targetDuty,
                               fadeTimeMs);
        ledc_fade_start(LEDC_LOW_SPEED_MODE, (ledc_channel_t)config.channel, LEDC_FADE_NO_WAIT);

        currentDutyCycle = targetDutyCycle;
        currentState.floatValue = targetDutyCycle;
    }

    String getConfigJson() const override {
        char buffer[256];
        snprintf(buffer, sizeof(buffer),
                 "{\"name\":\"%s\",\"type\":\"PWM_OUTPUT\",\"pin\":%d,"
                 "\"frequency\":%u,\"resolution\":%u,\"channel\":%u,"
                 "\"dutyCycle\":%.2f}",
                 pinName.c_str(), config.pin,
                 config.frequency, config.resolution, config.channel,
                 currentDutyCycle);
        return String(buffer);
    }

private:
    PWMOutputConfig config;
    float currentDutyCycle;
    uint32_t maxDutyValue;
};

#endif // PWM_OUTPUT_PIN_H
