#pragma once

#include <Arduino.h>

#if LORAWAN_ENABLED

void lorawanInit();
void lorawanJoin();

// Single consolidated uplink — 24-byte custom binary payload.
// Pass 0 for any field whose sensor is disabled or has no reading.
void lorawanSendAll(float lat, float lon, float alt,
                    float temp, float hum,
                    float pm1, float pm25, float pm4, float pm10,
                    float battV,
                    float lux,
                    float solarV);

// Send one queued message (called automatically after successful uplink)
void lorawanSendQueued();

// Status functions
bool lorawanIsJoined();           // Check if joined to network
uint32_t lorawanGetUplinkCount(); // Get total uplink count
bool lorawanLastUplinkOk();       // Check if last uplink succeeded

#endif // LORAWAN_ENABLED
