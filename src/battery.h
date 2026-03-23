#pragma once
#include <Arduino.h>

#if BATTERY_ENABLED

void batteryInit();
void batteryRead();
bool batteryHasReading();
void batteryGetData(float &voltage);
void batteryPrint();

#endif // BATTERY_ENABLED
