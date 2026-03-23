#include "ina219.h"

#if INA219_ENABLED

#include <Wire.h>
#include <Adafruit_INA219.h>

static Adafruit_INA219 ina219(0x40);
static float lastVoltage  = NAN;
static float lastCurrent  = NAN;
static float lastPower    = NAN;
static bool valid = false;

void ina219Init() {
    Wire.begin();
    if (!ina219.begin()) {
        Serial.println("INA219: init failed — check wiring");
    } else {
        Serial.println("INA219: ready");
    }
}

void ina219Read() {
    float busV = ina219.getBusVoltage_V();
    float shuntV = ina219.getShuntVoltage_mV() / 1000.0f;  // Convert mV to V
    float supplyV = busV + shuntV;  // Solar panel voltage = bus + shunt drop
    float i = ina219.getCurrent_mA();
    float p = ina219.getPower_mW();
    // INA219 returns 0 on comms failure; treat near-zero voltage as invalid
    if (supplyV < 0.0f) {
        valid = false;
        return;
    }
    lastVoltage = supplyV;  // Store supply voltage (solar panel), not bus voltage
    lastCurrent = i;
    lastPower   = p;
    valid = true;
}

bool ina219HasReading() {
    return valid;
}

void ina219GetData(float &voltage, float &current_mA, float &power_mW) {
    voltage    = lastVoltage;
    current_mA = lastCurrent;
    power_mW   = lastPower;
}

void ina219Print() {
    if (valid) {
        Serial.print("INA219 (solar): V=");
        Serial.print(lastVoltage, 2);
        Serial.print("V I=");
        Serial.print(lastCurrent, 1);
        Serial.print("mA P=");
        Serial.print(lastPower, 1);
        Serial.println("mW");
    } else {
        Serial.println("INA219: read failed");
    }
}

#endif // INA219_ENABLED
