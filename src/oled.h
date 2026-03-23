#pragma once

#include <Arduino.h>

#if OLED_ENABLED

void oledInit();
void oledUpdate();  // Update display with current sensor status
void oledSleep(uint32_t seconds);  // Show sleep screen
void oledSleepUpdate();  // Update status icons during sleep (owl's autonomic nervous system!)
void oledClear();

#endif // OLED_ENABLED
