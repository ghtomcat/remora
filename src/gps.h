#pragma once

#include <Arduino.h>

#if GPS_ENABLED

void gpsInit();
void gpsFeed();
bool gpsHasFix();
void gpsGetData(float &lat, float &lon, float &alt, uint8_t &sats, float &hdop);
uint32_t gpsGetAge();  // Get age of GPS data in milliseconds
void gpsPrint();
bool gpsIsResponding();  // Check if GPS is sending NMEA data

// Power management
void gpsStandby();     // Enter low-power standby mode (~7mA)
void gpsWakeup();      // Wake from standby mode
bool gpsWaitForFix(uint32_t timeout_ms);  // Wait for fix with timeout

#endif // GPS_ENABLED
