#pragma once
#include <Arduino.h>

#if SHT31_ENABLED

void sht31Init();
void sht31Read();               // call once per reporting cycle, not every loop
bool sht31HasReading();
void sht31GetData(float &temp, float &hum);
void sht31Print();

#endif // SHT31_ENABLED
