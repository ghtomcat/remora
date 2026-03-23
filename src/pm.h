#pragma once
#include <Arduino.h>

#if SPS30_ENABLED

void sps30Init();
void sps30Read();               // call once per reporting cycle, not every loop
bool sps30HasReading();
void sps30GetData(float &pm1p0, float &pm2p5, float &pm4p0, float &pm10p0);
void sps30Print();

// Power management
void sps30Start();              // Start measurement (takes ~10s to stabilize)
void sps30Stop();               // Stop measurement and enter idle mode

#endif // SPS30_ENABLED
