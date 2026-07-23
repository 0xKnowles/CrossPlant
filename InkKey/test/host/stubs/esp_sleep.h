#pragma once

#include <cstdint>

#define ESP_GPIO_WAKEUP_GPIO_LOW 0

inline int esp_deep_sleep_enable_gpio_wakeup(uint64_t, int) { return 0; }

// Simulator: records the sleep instead of powering off; the harness checks it.
extern bool hostDeepSleepEntered;
inline void esp_deep_sleep_start() { hostDeepSleepEntered = true; }
