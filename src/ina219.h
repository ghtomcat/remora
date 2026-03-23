#pragma once
#include <Arduino.h>

#if INA219_ENABLED

void ina219Init();
void ina219Read();               // call once per reporting cycle, not every loop
bool ina219HasReading();
void ina219GetData(float &voltage, float &current_mA, float &power_mW);
void ina219Print();

#endif // INA219_ENABLED
