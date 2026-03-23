#pragma once
#include <Arduino.h>

#if VEML7700_ENABLED

void veml7700Init();
void veml7700Read();               // call once per reporting cycle, not every loop
bool veml7700HasReading();
void veml7700GetData(float &lux);
void veml7700Print();

#endif // VEML7700_ENABLED
