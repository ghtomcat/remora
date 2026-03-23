#pragma once
#include <Arduino.h>

#if BLE_ENABLED

#include <bluefruit.h>

extern BLEUart bleuart;

void bleInit();
void bleProcessCommands();  // Check for and process incoming BLE commands

#endif // BLE_ENABLED
