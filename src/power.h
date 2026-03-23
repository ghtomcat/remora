#ifndef POWER_H
#define POWER_H

#include <Arduino.h>

// Sleep interval in seconds (default: 5 minutes = 300 seconds)
// Set to 30s for testing, 300s for production
#define SLEEP_INTERVAL_SEC 30

// Initialize power management
void powerInit();

// Sleep for specified seconds.
// Uses FreeRTOS delay() — BLE and SoftDevice remain fully active during sleep.
void powerSleep(uint32_t seconds);

// Sleep until motion detected or timeout
// Uses GPIO interrupt to wake on MPU6050 motion detection
void powerSleepUntilMotion(uint32_t maxSeconds, uint8_t interruptPin);

#endif // POWER_H
