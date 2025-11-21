#ifndef ESP_TASK_WDT_H
#define ESP_TASK_WDT_H

#include <Arduino.h>

// Mock FreeRTOS types if not defined
#ifndef TaskHandle_t
typedef void* TaskHandle_t;
#endif

// Mock functions
inline void esp_task_wdt_init(uint32_t timeout, bool panic) {}
inline void esp_task_wdt_add(TaskHandle_t handle) {}
inline void esp_task_wdt_reset() {}
inline void esp_task_wdt_delete(TaskHandle_t handle) {}

#endif
